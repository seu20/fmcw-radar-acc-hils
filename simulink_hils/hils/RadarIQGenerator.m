classdef RadarIQGenerator < matlab.System

    properties (Access = private)
        radar_

        numSamples_ = 128
        numChirps_ = 64
        numRx_ = 2

        chirpDuration_ = 20e-6

        adcFs_ = 6.4e6
        waveformFs_ = 150e6

        resampleP_
        resampleQ_
    end


    methods (Access = protected)

        %% =========================================================
        % Simulation 시작 시 1회 실행
        % ==========================================================
        function setupImpl(obj, ~)

            % createRadar.m에서 생성한 radar 객체 사용
            %
            % init_hils.m에서
            %
            % radar = createRadar(egoVehicle, scnro);
            %
            % 로 생성해 둔다.
            obj.radar_ = evalin('base', 'radar');

            % 150 MHz -> 6.4 MHz
            [obj.resampleP_, obj.resampleQ_] = ...
                rat(obj.adcFs_ / obj.waveformFs_);
        end


        %% =========================================================
        % Simulink Sample Time마다 호출
        % ==========================================================
        function rawIQ = stepImpl(obj, actors)

            rawIQ = complex(zeros( ...
                obj.numSamples_, ...
                obj.numChirps_, ...
                obj.numRx_, ...
                'single'));

            numActors = double(actors.NumActors);

            % Radar target 구조 생성
            targets = repmat( ...
                struct( ...
                    'Position', [0 0 0], ...
                    'Velocity', [0 0 0]), ...
                1, ...
                numActors);


            % 이 Simulink step의 시작 시간
            frameStartTime = double(actors.Time);


            %% =====================================================
            % 64 Chirps = 1 Radar Frame
            % ======================================================
            for chirpIdx = 1:obj.numChirps_

                chirpOffset = ...
                    (chirpIdx - 1) * obj.chirpDuration_;


                %% Actor pose
                %
                % Scenario Reader는 0.01 s마다 갱신되지만
                % Radar frame 내부에서는 20 us 단위로 chirp가 진행됨.
                %
                % 따라서 현재 Position + Velocity 기반으로
                % frame 내부 위치를 선형 보간한다.

                for actorIdx = 1:numActors

                    position = ...
                        double(actors.Actors(actorIdx).Position);

                    velocity = ...
                        double(actors.Actors(actorIdx).Velocity);


                    targets(actorIdx).Position = ...
                        position + ...
                        velocity * chirpOffset;

                    targets(actorIdx).Velocity = ...
                        velocity;
                end


                %% 현재 Chirp 시간

                radarTime = ...
                    frameStartTime + chirpOffset;


                %% Transmitted FMCW reference

                txRef = ...
                    obj.radar_.Waveform();


                %% Received radar signal

                [iqSig, ~] = ...
                    obj.radar_( ...
                        targets, ...
                        radarTime);


                %% RX별 Dechirp + ADC equivalent resampling

                for rx = 1:obj.numRx_

                    beatHighFs = ...
                        dechirp( ...
                            iqSig(:,rx), ...
                            txRef);


                    beatADC = ...
                        resample( ...
                            beatHighFs, ...
                            obj.resampleP_, ...
                            obj.resampleQ_);


                    rawIQ(:,chirpIdx,rx) = ...
                        single( ...
                            beatADC(1:obj.numSamples_));
                end
            end
        end


        %% =========================================================
        % Output specification
        % ==========================================================
        function sizeOut = getOutputSizeImpl(obj)
            sizeOut = ...
                [ ...
                    obj.numSamples_, ...
                    obj.numChirps_, ...
                    obj.numRx_ ...
                ];
        end


        function typeOut = getOutputDataTypeImpl(~)
            typeOut = 'single';
        end


        function complexOut = isOutputComplexImpl(~)
            complexOut = true;
        end


        function fixedOut = isOutputFixedSizeImpl(~)
            fixedOut = true;
        end


        %% =========================================================
        % Block port names
        % ==========================================================
        function name = getInputNamesImpl(~)
            name = 'Actors';
        end


        function name = getOutputNamesImpl(~)
            name = 'RawIQ';
        end


        %% =========================================================
        % Sample Time
        % ==========================================================
        function sts = getSampleTimeImpl(obj)

        sts = createSampleTime( ...
            obj, ...
            'Type', 'Discrete', ...
            'SampleTime', 0.2, ...
            'OffsetTime', 0);
        end
    end
end