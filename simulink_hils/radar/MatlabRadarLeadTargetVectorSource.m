classdef MatlabRadarLeadTargetVectorSource < matlab.System
    % MatlabRadarLeadTargetVectorSource
    %
    % MATLAB-only implementation of the final RPi radar-result boundary.
    %
    % IMPORTANT ARCHITECTURE RULE:
    %   This block receives primitive EgoX/EgoSpeed only. It does not
    %   consume Scenario Reader BusActorsN. Therefore the validated
    %   Scenario Reader -> Radar IQ Generator auto-bus path remains
    %   untouched.
    %
    % The block reconstructs configured actor trajectories, generates a
    % separate FMCW RawIQ frame with radarAcc, runs MATLAB radar DSP, and
    % selects one lead target. Its single [5x1] output carries the semantic fields of the
    % RPi LeadTargetFrame without creating a Simulink.Bus object.

    properties (Nontunable)
        LaneHalfWidth = 1.8
        SampleTime = 0.1
    end

    properties (Access = private)
        radar_
        time_ = 0.0
        frameId_ = uint32(0)
        egoY_ = 0.0
        numActors_ = 0
        waypoints_
        speeds_
        waits_
        actorY_

        resampleP_
        resampleQ_
        winRange_
        winDoppler_
        rangeResolution_
        dopplerResolution_
        alpha_
    end

    properties (Constant, Access = private)
        NUM_SAMPLES = 128
        NUM_CHIRPS = 64
        NUM_RX = 2

        FC = 77e9
        BANDWIDTH = 150e6
        CHIRP_DURATION = 20e-6
        ADC_FS = 6.4e6
        WAVEFORM_FS = 150e6
        CHEB_ATTEN = 100

        TRAINING_RANGE = 8
        TRAINING_DOPPLER = 4
        GUARD_RANGE = 3
        GUARD_DOPPLER = 3
        PFA = 1e-6
        N_TRAINING_CELLS = 296

        DBSCAN_EPS = 1.5
        DBSCAN_MIN_SAMPLES = 3
    end

    methods (Access = protected)
        function setupImpl(obj, ~, ~)
            config = evalin('base', 'scenarioConfig');
            obj.radar_ = evalin('base', 'radarAcc');

            obj.egoY_ = double(config.ego.position(2));
            obj.numActors_ = numel(config.actors);
            obj.waypoints_ = cell(obj.numActors_, 1);
            obj.speeds_ = cell(obj.numActors_, 1);
            obj.waits_ = cell(obj.numActors_, 1);
            obj.actorY_ = zeros(obj.numActors_, 1);

            for i = 1:obj.numActors_
                obj.waypoints_{i} = double(config.actors(i).waypoints);
                obj.speeds_{i} = double(config.actors(i).speed(:));
                obj.waits_{i} = double(config.actors(i).waitTime(:));
                obj.actorY_(i) = double(config.actors(i).waypoints(1,2));
            end

            [obj.resampleP_, obj.resampleQ_] = ...
                rat(obj.ADC_FS / obj.WAVEFORM_FS);

            c = physconst('LightSpeed');
            lambda = c / obj.FC;
            obj.rangeResolution_ = c / (2.0 * obj.BANDWIDTH);
            obj.dopplerResolution_ = ...
                lambda / (2.0 * obj.NUM_CHIRPS * obj.CHIRP_DURATION);

            obj.winRange_ = single(chebwin(obj.NUM_SAMPLES, obj.CHEB_ATTEN));
            obj.winDoppler_ = single(chebwin(obj.NUM_CHIRPS, obj.CHEB_ATTEN).');

            n = double(obj.N_TRAINING_CELLS);
            obj.alpha_ = n * (obj.PFA ^ (-1.0 / n) - 1.0);

            obj.time_ = 0.0;
            obj.frameId_ = uint32(0);
        end

        function leadTarget = stepImpl(obj, egoX, egoSpeed)
            rawIQ = obj.generateRawIQ(double(egoX), double(egoSpeed));
            [validOut, distanceOut, relativeVelocityOut, angleOut] = ...
                obj.processRawIQ(rawIQ);

            % Stable MATLAB/RPi boundary.  The UDP decoder used later emits
            % this exact vector, so ACC and top-level wiring do not change.
            % [frame_id; valid; distance; relative_velocity; angle]
            leadTarget = [ ...
                double(obj.frameId_); ...
                double(validOut); ...
                double(distanceOut); ...
                double(relativeVelocityOut); ...
                double(angleOut) ...
            ];

            obj.frameId_ = obj.frameId_ + uint32(1);
            obj.time_ = obj.time_ + obj.SampleTime;
        end

        function resetImpl(obj)
            obj.time_ = 0.0;
            obj.frameId_ = uint32(0);
        end

        function s = getOutputSizeImpl(~)
            s = [5 1];
        end

        function t = getOutputDataTypeImpl(~)
            t = 'double';
        end

        function c = isOutputComplexImpl(~)
            c = false;
        end

        function f = isOutputFixedSizeImpl(~)
            f = true;
        end

        function [n1, n2] = getInputNamesImpl(~)
            n1 = 'EgoX';
            n2 = 'EgoSpeed';
        end

        function n = getOutputNamesImpl(~)
            n = 'LeadTarget';
        end

        function sts = getSampleTimeImpl(obj)
            sts = createSampleTime(obj, ...
                'Type', 'Discrete', ...
                'SampleTime', obj.SampleTime, ...
                'OffsetTime', 0);
        end
    end

    methods (Access = private)
        function rawIQ = generateRawIQ(obj, egoX, egoSpeed)
            rawIQ = complex(zeros(obj.NUM_SAMPLES, obj.NUM_CHIRPS, obj.NUM_RX, 'single'));

            targets = repmat(struct('Position', [0 0 0], 'Velocity', [0 0 0]), ...
                1, obj.numActors_);

            frameStartTime = obj.time_;

            % Scenario Reader's actor signal is relative to Ego. Recreate
            % the same relative kinematics from scenarioConfig and the
            % current closed-loop ego state using primitive signals only.
            actorX0 = zeros(obj.numActors_,1);
            actorV0 = zeros(obj.numActors_,1);
            for actorIdx = 1:obj.numActors_
                [actorX0(actorIdx), actorV0(actorIdx)] = obj.stateAtTime( ...
                    frameStartTime, obj.waypoints_{actorIdx}, ...
                    obj.speeds_{actorIdx}, obj.waits_{actorIdx});
            end

            for chirpIdx = 1:obj.NUM_CHIRPS
                chirpOffset = (chirpIdx - 1) * obj.CHIRP_DURATION;

                for actorIdx = 1:obj.numActors_
                    relX0 = actorX0(actorIdx) - egoX;
                    relVx = actorV0(actorIdx) - egoSpeed;

                    targets(actorIdx).Position = [ ...
                        relX0 + relVx * chirpOffset, ...
                        obj.actorY_(actorIdx) - obj.egoY_, ...
                        0.0];

                    targets(actorIdx).Velocity = [relVx, 0.0, 0.0];
                end

                radarTime = frameStartTime + chirpOffset;
                txRef = obj.radar_.Waveform();
                [iqSig, ~] = obj.radar_(targets, radarTime);

                for rx = 1:obj.NUM_RX
                    beatHighFs = dechirp(iqSig(:,rx), txRef);
                    beatADC = resample(beatHighFs, obj.resampleP_, obj.resampleQ_);
                    rawIQ(:,chirpIdx,rx) = single(beatADC(1:obj.NUM_SAMPLES));
                end
            end
        end

        function [validOut, distanceOut, relativeVelocityOut, angleOut] = ...
                processRawIQ(obj, rawIQ)

            validOut = uint8(0);
            distanceOut = single(0.0);
            relativeVelocityOut = single(0.0);
            angleOut = single(0.0);

            rdm = complex(zeros(obj.NUM_SAMPLES, obj.NUM_CHIRPS, obj.NUM_RX, 'single'));

            for rx = 1:obj.NUM_RX
                iqWindowed = single(rawIQ(:,:,rx)) .* obj.winRange_ .* obj.winDoppler_;
                rangeFFT = fft(iqWindowed, obj.NUM_SAMPLES, 1);
                rdm(:,:,rx) = fftshift(fft(rangeFFT, obj.NUM_CHIRPS, 2), 2);
            end

            powerMap = double(abs(rdm(:,:,1)).^2 + abs(rdm(:,:,2)).^2);

            totalHalfRange = obj.TRAINING_RANGE + obj.GUARD_RANGE;
            totalHalfDoppler = obj.TRAINING_DOPPLER + obj.GUARD_DOPPLER;

            totalKernel = ones(2 * totalHalfRange + 1, 2 * totalHalfDoppler + 1);
            guardKernel = ones(2 * obj.GUARD_RANGE + 1, 2 * obj.GUARD_DOPPLER + 1);

            totalSum = conv2(powerMap, totalKernel, 'same');
            guardSum = conv2(powerMap, guardKernel, 'same');
            trainingAvg = (totalSum - guardSum) / double(obj.N_TRAINING_CELLS);
            threshold = trainingAvg * obj.alpha_;

            rangeStart = totalHalfRange + 1;
            rangeEnd = obj.NUM_SAMPLES - totalHalfRange;
            dopplerStart = totalHalfDoppler + 1;
            dopplerEnd = obj.NUM_CHIRPS - totalHalfDoppler;

            detectionMask = false(obj.NUM_SAMPLES, obj.NUM_CHIRPS);
            interiorPower = powerMap(rangeStart:rangeEnd, dopplerStart:dopplerEnd);
            interiorThreshold = threshold(rangeStart:rangeEnd, dopplerStart:dopplerEnd);
            detectionMask(rangeStart:rangeEnd, dopplerStart:dopplerEnd) = ...
                interiorPower >= interiorThreshold;

            detectedPoints = zeros(0, 3);
            for rangeBin = rangeStart:rangeEnd
                dopplerLocal = find(detectionMask(rangeBin, dopplerStart:dopplerEnd));
                for k = 1:numel(dopplerLocal)
                    dopplerBin = dopplerStart + dopplerLocal(k) - 1;
                    detectedPoints(end + 1, :) = [ ...
                        rangeBin - 1, dopplerBin - 1, powerMap(rangeBin, dopplerBin)]; %#ok<AGROW>
                end
            end

            if isempty(detectedPoints)
                return;
            end

            clusters = obj.clusterDetections(detectedPoints);
            if isempty(clusters)
                return;
            end

            bestDistance = inf;
            bestRelativeVelocity = 0.0;
            bestAngle = 0.0;
            foundLead = false;

            for clusterIdx = 1:numel(clusters)
                pointIndices = clusters{clusterIdx};
                clusterPowers = detectedPoints(pointIndices, 3);
                [~, localPeakIdx] = max(clusterPowers);
                pointIdx = pointIndices(localPeakIdx);

                rangeIdx0 = detectedPoints(pointIdx, 1);
                dopplerIdx0 = detectedPoints(pointIdx, 2);
                rangeIdx = rangeIdx0 + 1;
                dopplerIdx = dopplerIdx0 + 1;

                z1 = rdm(rangeIdx, dopplerIdx, 1);
                z2 = rdm(rangeIdx, dopplerIdx, 2);
                phaseDiff = angle(z2 * conj(z1));
                sinTheta = max(-1.0, min(1.0, double(phaseDiff) / pi));
                angleRad = asin(sinTheta);

                distance = double(rangeIdx0) * obj.rangeResolution_;
                shiftedDopplerBin = double(dopplerIdx0) - obj.NUM_CHIRPS / 2;
                relativeVelocity = shiftedDopplerBin * obj.dopplerResolution_;
                lateral = distance * sin(angleRad);

                if distance > 0.0 && ...
                        abs(lateral) <= obj.LaneHalfWidth && ...
                        distance < bestDistance
                    bestDistance = distance;
                    bestRelativeVelocity = relativeVelocity;
                    bestAngle = angleRad;
                    foundLead = true;
                end
            end

            if foundLead
                validOut = uint8(1);
                distanceOut = single(bestDistance);
                relativeVelocityOut = single(bestRelativeVelocity);
                angleOut = single(bestAngle);
            end
        end

        function clusters = clusterDetections(obj, detectedPoints)
            numDetections = size(detectedPoints, 1);
            UNVISITED = -1;
            NOISE = -2;
            labels = repmat(UNVISITED, numDetections, 1);
            clusters = cell(0, 1);
            clusterID = 0;

            for point = 1:numDetections
                if labels(point) ~= UNVISITED
                    continue;
                end

                neighbours = obj.scanNeighbours(detectedPoints, point);
                if numel(neighbours) < obj.DBSCAN_MIN_SAMPLES
                    labels(point) = NOISE;
                    continue;
                end

                clusterPoints = zeros(1, 0);
                labels(point) = clusterID;
                clusterPoints(end + 1) = point;

                i = 1;
                while i <= numel(neighbours)
                    neighbourIdx = neighbours(i);

                    if labels(neighbourIdx) == NOISE
                        labels(neighbourIdx) = clusterID;
                        clusterPoints(end + 1) = neighbourIdx; %#ok<AGROW>
                        i = i + 1;
                        continue;
                    end

                    if labels(neighbourIdx) ~= UNVISITED
                        i = i + 1;
                        continue;
                    end

                    labels(neighbourIdx) = clusterID;
                    clusterPoints(end + 1) = neighbourIdx; %#ok<AGROW>

                    nextNeighbours = obj.scanNeighbours(detectedPoints, neighbourIdx);
                    if numel(nextNeighbours) >= obj.DBSCAN_MIN_SAMPLES
                        neighbours = [neighbours, nextNeighbours]; %#ok<AGROW>
                    end
                    i = i + 1;
                end

                clusters{end + 1, 1} = clusterPoints; %#ok<AGROW>
                clusterID = clusterID + 1;
            end
        end

        function neighbours = scanNeighbours(obj, detectedPoints, pointIdx)
            scanRange = detectedPoints(pointIdx, 1);
            scanDoppler = detectedPoints(pointIdx, 2);
            rangeDiff = scanRange - detectedPoints(:, 1);
            dopplerDiff = scanDoppler - detectedPoints(:, 2);
            squaredDistance = rangeDiff .* rangeDiff + dopplerDiff .* dopplerDiff;
            neighbours = find(squaredDistance <= obj.DBSCAN_EPS * obj.DBSCAN_EPS).';
        end

        function [x, v] = stateAtTime(~, t, waypoints, speeds, waitTimes)
            n = size(waypoints, 1);
            elapsed = 0.0;

            for i = 1:n
                waitDuration = waitTimes(i);
                if waitDuration > 0.0
                    if t < elapsed + waitDuration
                        x = waypoints(i, 1);
                        v = speeds(i);
                        return;
                    end
                    elapsed = elapsed + waitDuration;
                end

                if i == n
                    x = waypoints(i, 1) + speeds(i) * (t - elapsed);
                    v = speeds(i);
                    return;
                end

                x0 = waypoints(i, 1);
                x1 = waypoints(i + 1, 1);
                v0 = speeds(i);
                v1 = speeds(i + 1);
                distance = x1 - x0;

                if abs(v0 + v1) < 1e-12
                    segmentTime = 0.0;
                else
                    segmentTime = 2.0 * distance / (v0 + v1);
                end

                if segmentTime > 0.0 && t < elapsed + segmentTime
                    tau = t - elapsed;
                    acceleration = (v1 - v0) / segmentTime;
                    x = x0 + v0 * tau + 0.5 * acceleration * tau * tau;
                    v = v0 + acceleration * tau;
                    return;
                end

                elapsed = elapsed + segmentTime;
            end

            x = waypoints(end, 1);
            v = speeds(end);
        end
    end
end
