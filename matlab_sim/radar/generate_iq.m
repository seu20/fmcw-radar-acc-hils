%% FMCW Radar - 2 RX IQ / Range-Doppler / CFAR / DBSCAN / Peak / Angle
clear;
clc;
close all;

addpath('../scenario')


%% 1. Output Directory

dataDir = fullfile(pwd, 'data');

windowDir = fullfile(dataDir, 'window');
rdmDir    = fullfile(dataDir, 'rdm');
cfarDir   = fullfile(dataDir, 'cfar');
dbscanDir = fullfile(dataDir, 'dbscan');
peakDir   = fullfile(dataDir, 'peak');
angleDir  = fullfile(dataDir, 'angle');

outputDirs = {
    dataDir
    windowDir
    rdmDir
    cfarDir
    dbscanDir
    peakDir
    angleDir
};

for i = 1:numel(outputDirs)

    if ~exist(outputDirs{i}, 'dir')
        mkdir(outputDirs{i});
    end
end


%% 2. Scenario / Radar

scenarioFcn = @createScenario_approach;

[scnro, egoVehicle, radar] = ...
    scenarioFcn();


%% 3. Radar Parameters

fc = 77e9;

c = physconst('LightSpeed');
lambda = c / fc;

bandwidth = 150e6;
chirpDuration = 20e-6;

numSamples = 128;
numChirps = 64;
numRx = 2;

% ADC-equivalent sampling rate
adcFs = ...
    numSamples / chirpDuration;

% waveform simulation sampling rate
waveformFs = ...
    radar.Waveform.SampleRate;

% RX antenna spacing
d = lambda / 2;

% Chebyshev sidelobe attenuation
chebAtten = 100;


%% 4. DBSCAN Parameters
%
% C++:
%
% eps = 1.5f
% min_samples = 3
%
% Distance:
% sqrt(
%   range_diff^2 +
%   doppler_diff^2
% )
%
% 실제 거리/속도가 아니라
% Range-Doppler BIN 좌표에서 clustering

dbscanEps = 1.5;
dbscanMinSamples = 3;


%% 5. Resolution

rangeRes = ...
    c / (2 * bandwidth);

velocityRes = ...
    lambda / ...
    (2 * numChirps * chirpDuration);


fprintf( ...
    'Range Resolution    : %.3f m\n', ...
    rangeRes);

fprintf( ...
    'Velocity Resolution : %.3f m/s\n', ...
    velocityRes);

fprintf( ...
    'ADC Fs              : %.2f MHz\n', ...
    adcFs / 1e6);

fprintf( ...
    'Waveform Fs         : %.2f MHz\n', ...
    waveformFs / 1e6);

fprintf( ...
    'Lambda              : %.3f mm\n', ...
    lambda * 1e3);

fprintf( ...
    'RX spacing          : %.3f mm\n\n', ...
    d * 1e3);


%% 6. Scenario Timing

scnro.SampleTime = chirpDuration;

% rawIQ(sample, chirp, rx)
rawIQ = complex(zeros( ...
    numSamples, ...
    numChirps, ...
    numRx));

% 150 MHz -> 6.4 MHz
[p, q] = ...
    rat(adcFs / waveformFs);

advance(scnro);


%% 7. IQ Acquisition

for chirpIdx = 1:numChirps

    poses = ...
        targetPoses(egoVehicle);

    time = ...
        scnro.SimulationTime;

    txRef = ...
        radar.Waveform();

    [iqSig, ~] = ...
        radar(poses, time);


    for rx = 1:numRx

        % Dechirp
        beatHighFs = ...
            dechirp( ...
                iqSig(:,rx), ...
                txRef);

        % 150 MHz -> 6.4 MHz
        beatADC = ...
            resample( ...
                beatHighFs, ...
                p, ...
                q);

        rawIQ(:,chirpIdx,rx) = ...
            beatADC(1:numSamples);
    end

    advance(scnro);
end

fprintf('IQ acquisition complete\n');


%% 8. Chebyshev Window

winRange = ...
    chebwin( ...
        numSamples, ...
        chebAtten);

winDoppler = ...
    chebwin( ...
        numChirps, ...
        chebAtten).';


% C++ float와 동일하게 저장
rangeChebWindow = ...
    single(winRange);

dopplerChebWindow = ...
    single(winDoppler.');


%% 9. Export Window CSV

writematrix( ...
    rangeChebWindow, ...
    fullfile( ...
        windowDir, ...
        'range_cheb_window.csv'));

writematrix( ...
    dopplerChebWindow, ...
    fullfile( ...
        windowDir, ...
        'doppler_cheb_window.csv'));


%% 10. Export Chebyshev Window -> cheb_window.h

fidWin = fopen( ...
    fullfile( ...
        windowDir, ...
        'cheb_window.h'), ...
    'w');

if fidWin == -1
    error('cheb_window.h 파일을 열 수 없습니다.');
end


fprintf( ...
    fidWin, ...
    '#pragma once\n\n');

fprintf( ...
    fidWin, ...
    '#define RANGE_WINDOW_SIZE %d\n', ...
    numSamples);

fprintf( ...
    fidWin, ...
    '#define DOPPLER_WINDOW_SIZE %d\n\n', ...
    numChirps);


% Range Window
fprintf( ...
    fidWin, ...
    'static const float range_cheb_window[RANGE_WINDOW_SIZE] = {\n');

for i = 1:numSamples

    fprintf( ...
        fidWin, ...
        '    %.9ef', ...
        rangeChebWindow(i));

    if i < numSamples
        fprintf(fidWin, ',');
    end

    fprintf(fidWin, '\n');
end

fprintf(fidWin, '};\n\n');


% Doppler Window
fprintf( ...
    fidWin, ...
    'static const float doppler_cheb_window[DOPPLER_WINDOW_SIZE] = {\n');

for i = 1:numChirps

    fprintf( ...
        fidWin, ...
        '    %.9ef', ...
        dopplerChebWindow(i));

    if i < numChirps
        fprintf(fidWin, ',');
    end

    fprintf(fidWin, '\n');
end

fprintf(fidWin, '};\n');

fclose(fidWin);

fprintf('Window export complete\n');


%% 11. Range-Doppler FFT

rdm = complex(zeros( ...
    numSamples, ...
    numChirps, ...
    numRx));


for rx = 1:numRx

    % 2D separable Chebyshev Window
    iqWindowed = ...
        rawIQ(:,:,rx) .* ...
        winRange .* ...
        winDoppler;

    % Range FFT
    rangeFFT = ...
        fft( ...
            iqWindowed, ...
            numSamples, ...
            1);

    % Doppler FFT + fftshift
    rdm(:,:,rx) = ...
        fftshift( ...
            fft( ...
                rangeFFT, ...
                numChirps, ...
                2), ...
            2);
end


%% 12. Export RDM

writematrix( ...
    real(rdm(:,:,1)), ...
    fullfile( ...
        rdmDir, ...
        'rdm_rx1_real.csv'));

writematrix( ...
    imag(rdm(:,:,1)), ...
    fullfile( ...
        rdmDir, ...
        'rdm_rx1_imag.csv'));

writematrix( ...
    real(rdm(:,:,2)), ...
    fullfile( ...
        rdmDir, ...
        'rdm_rx2_real.csv'));

writematrix( ...
    imag(rdm(:,:,2)), ...
    fullfile( ...
        rdmDir, ...
        'rdm_rx2_imag.csv'));

fprintf('RDM CSV export complete\n');


%% 13. RX Power Combination

% C++ PowerCalculation()
%
% power[range][doppler]
% =
% |RX1|^2 + |RX2|^2

powerMap = ...
    abs(rdm(:,:,1)).^2 + ...
    abs(rdm(:,:,2)).^2;


%% 14. 2D CA-CFAR

trainingRange = 8;
trainingDoppler = 4;

guardRange = 3;
guardDoppler = 3;

pfa = 1e-6;


cfar = phased.CFARDetector2D( ...
    'Method', 'CA', ...
    'TrainingBandSize', ...
        [trainingRange trainingDoppler], ...
    'GuardBandSize', ...
        [guardRange guardDoppler], ...
    'ThresholdFactor', 'Auto', ...
    'ProbabilityFalseAlarm', ...
        pfa);


rangeStart = ...
    trainingRange + ...
    guardRange + 1;

rangeEnd = ...
    numSamples - ...
    trainingRange - ...
    guardRange;

dopplerStart = ...
    trainingDoppler + ...
    guardDoppler + 1;

dopplerEnd = ...
    numChirps - ...
    trainingDoppler - ...
    guardDoppler;


numRangeCuts = ...
    rangeEnd - ...
    rangeStart + 1;

numDopplerCuts = ...
    dopplerEnd - ...
    dopplerStart + 1;

numCuts = ...
    numRangeCuts * ...
    numDopplerCuts;


cutIdx = ...
    zeros(2, numCuts);


%% IMPORTANT
%
% C++ CFAR 순서:
%
% for (range_bin)
%     for (doppler_bin)
%
% detected_points_도 이 순서로 push 된다.
%
% 따라서 MATLAB도 동일한 순서로 CUT 생성.

idx = 1;

for rangeBin = rangeStart:rangeEnd

    for dopplerBin = ...
            dopplerStart:dopplerEnd

        cutIdx(:,idx) = ...
            [
                rangeBin;
                dopplerBin
            ];

        idx = idx + 1;
    end
end


%% CFAR

detections = ...
    cfar( ...
        powerMap, ...
        cutIdx);

detectedIdx = ...
    cutIdx(:,detections);

numDetections = ...
    size( ...
        detectedIdx, ...
        2);


fprintf( ...
    'CFAR detections : %d\n', ...
    numDetections);


%% 15. CFAR Detection Map

cfarMap = ...
    zeros( ...
        numSamples, ...
        numChirps, ...
        'uint8');


for k = 1:numDetections

    rangeBin = ...
        detectedIdx(1,k);

    dopplerBin = ...
        detectedIdx(2,k);

    cfarMap( ...
        rangeBin, ...
        dopplerBin) = 1;
end


writematrix( ...
    cfarMap, ...
    fullfile( ...
        cfarDir, ...
        'cfar_map.csv'));


%% 16. Detection Points
%
% C++ Detection:
%
% struct Detection
% {
%     size_t range_idx;
%     size_t doppler_idx;
%     float power;
% };
%
% MATLAB은 1-based index이므로
% CSV에는 C++과 동일하게 0-based로 변환하여 저장.
%
% Columns:
%
% 1 : range_idx
% 2 : doppler_idx
% 3 : power

detectedPoints = ...
    zeros( ...
        numDetections, ...
        3);


for k = 1:numDetections

    matlabRangeIdx = ...
        detectedIdx(1,k);

    matlabDopplerIdx = ...
        detectedIdx(2,k);


    cppRangeIdx = ...
        matlabRangeIdx - 1;

    cppDopplerIdx = ...
        matlabDopplerIdx - 1;


    pointPower = ...
        powerMap( ...
            matlabRangeIdx, ...
            matlabDopplerIdx);


    detectedPoints(k,:) = ...
        [
            cppRangeIdx, ...
            cppDopplerIdx, ...
            pointPower
        ];
end


writematrix( ...
    detectedPoints, ...
    fullfile( ...
        cfarDir, ...
        'detected_points.csv'));


%% 17. DBSCAN
%
% C++ DBSCAN 구현과 동일한 동작:
%
% - Range/Doppler BIN 공간
% - Euclidean distance
% - distance^2 <= eps^2
% - 자기 자신 neighbour 포함
% - min_samples = 3
% - Noise = -2
% - UNVISITED = -1
% - Cluster ID = 0부터 시작
% - Noise가 이후 Core의 neighbour가 되면 Border로 편입
% - neighbour 중복 제거하지 않음
%
% clusters{n}에는
% detectedPoints의 MATLAB 배열 index가 저장된다.

UNVISITED = -1;
NOISE = -2;

labels = ...
    repmat( ...
        UNVISITED, ...
        numDetections, ...
        1);

clusters = ...
    cell(0,1);

clusterId = 0;


for point = 1:numDetections

    % 이미 처리된 Point
    if labels(point) ~= UNVISITED
        continue;
    end


    % 현재 Point neighbours
    neighbours = ...
        scanNeighbours( ...
            detectedPoints, ...
            point, ...
            dbscanEps);


    % Core Point가 아니면 Noise
    if numel(neighbours) < ...
            dbscanMinSamples

        labels(point) = ...
            NOISE;

        continue;
    end


    % 새로운 Cluster
    clusterPoints = ...
        zeros(1,0);


    labels(point) = ...
        clusterId;

    clusterPoints(end + 1) = ...
        point;


    %% Cluster Expansion
    %
    % C++:
    %
    % for (size_t i = 0;
    %      i < neighbours.size();
    %      ++i)
    %
    % neighbours가 확장되면
    % loop 범위도 같이 증가함.
    %
    % MATLAB에서는 while로 동일하게 구현.

    i = 1;

    while i <= numel(neighbours)

        neighbourIdx = ...
            neighbours(i);


        % 이전에 Noise였던 Point
        %
        % 현재 Core Point의 neighbour가 되었으므로
        % Border Point로 편입
        if labels(neighbourIdx) == NOISE

            labels(neighbourIdx) = ...
                clusterId;

            clusterPoints(end + 1) = ...
                neighbourIdx;

            i = i + 1;

            continue;
        end


        % 이미 다른 처리에서 방문됨
        if labels(neighbourIdx) ~= ...
                UNVISITED

            i = i + 1;

            continue;
        end


        % 현재 Cluster에 포함
        labels(neighbourIdx) = ...
            clusterId;

        clusterPoints(end + 1) = ...
            neighbourIdx;


        % 이 Point의 neighbours 검사
        nextNeighbours = ...
            scanNeighbours( ...
                detectedPoints, ...
                neighbourIdx, ...
                dbscanEps);


        % Core Point라면
        % neighbour 목록을 확장
        if numel(nextNeighbours) >= ...
                dbscanMinSamples

            % C++과 동일하게
            % 중복 제거하지 않음
            neighbours = ...
                [
                    neighbours, ...
                    nextNeighbours
                ];
        end


        i = i + 1;
    end


    clusters{end + 1, 1} = ...
        clusterPoints;

    clusterId = ...
        clusterId + 1;
end


fprintf( ...
    'DBSCAN clusters  : %d\n', ...
    numel(clusters));


%% 18. Export DBSCAN
%
% Columns:
%
% 1 : cluster_id
% 2 : range_idx
% 3 : doppler_idx
% 4 : power
%
% 모든 index / cluster_id는 C++와 동일하게 0-based.

dbscanOutput = ...
    zeros(0,4);


for clusterIdx = 1:numel(clusters)

    cppClusterId = ...
        clusterIdx - 1;

    clusterPoints = ...
        clusters{clusterIdx};


    for pointIdx = clusterPoints

        dbscanOutput(end + 1,:) = ...
            [
                cppClusterId, ...
                detectedPoints(pointIdx,1), ...
                detectedPoints(pointIdx,2), ...
                detectedPoints(pointIdx,3)
            ];
    end
end


writematrix( ...
    dbscanOutput, ...
    fullfile( ...
        dbscanDir, ...
        'dbscan_clusters.csv'));


%% 19. Peak Detection
%
% C++:
%
% std::max_element(
%     cluster.points.begin(),
%     cluster.points.end(),
%     power 비교
% )
%
% 동일하게 각 Cluster에서
% 가장 Power가 큰 Detection 하나 선택.
%
% Columns:
%
% 1 : cluster_id
% 2 : range_idx
% 3 : doppler_idx
% 4 : power

numClusters = ...
    numel(clusters);

peaks = ...
    zeros( ...
        numClusters, ...
        4);


for clusterIdx = 1:numClusters

    pointIndices = ...
        clusters{clusterIdx};

    clusterPowers = ...
        detectedPoints( ...
            pointIndices, ...
            3);


    [maxPower, localMaxIdx] = ...
        max(clusterPowers);


    pointIdx = ...
        pointIndices(localMaxIdx);


    peaks(clusterIdx,:) = ...
        [
            clusterIdx - 1, ...
            detectedPoints(pointIdx,1), ...
            detectedPoints(pointIdx,2), ...
            maxPower
        ];
end


writematrix( ...
    peaks, ...
    fullfile( ...
        peakDir, ...
        'peaks.csv'));


fprintf( ...
    'Peaks             : %d\n', ...
    size(peaks,1));


%% 20. Angle Estimation
%
% C++:
%
% phase_diff =
%     arg(
%         RX2 * conj(RX1)
%     );
%
% angle =
%     asin(
%         phase_diff / pi
%     );
%
% C++ std::asin() 결과는 radian.
%
% Columns:
%
% 1 : cluster_id
% 2 : range_idx
% 3 : doppler_idx
% 4 : angle_rad

angles = ...
    zeros( ...
        numClusters, ...
        4);


for k = 1:numClusters

    clusterId = ...
        peaks(k,1);

    rangeIdxCpp = ...
        peaks(k,2);

    dopplerIdxCpp = ...
        peaks(k,3);


    % MATLAB 1-based index로 다시 변환
    rangeIdxMatlab = ...
        rangeIdxCpp + 1;

    dopplerIdxMatlab = ...
        dopplerIdxCpp + 1;


    % RX1 / RX2 complex value
    z1 = ...
        rdm( ...
            rangeIdxMatlab, ...
            dopplerIdxMatlab, ...
            1);

    z2 = ...
        rdm( ...
            rangeIdxMatlab, ...
            dopplerIdxMatlab, ...
            2);


    % C++:
    %
    % std::arg(
    %     rx2_iq *
    %     std::conj(rx1_iq)
    % )

    phaseDiff = ...
        angle( ...
            z2 * conj(z1));


    sinTheta = ...
        phaseDiff / pi;


    % Floating-point 안전 처리
    sinTheta = ...
        max( ...
            -1, ...
            min( ...
                1, ...
                sinTheta));


    % C++ std::asin과 동일:
    % radian 결과
    angleRad = ...
        asin(sinTheta);


    angles(k,:) = ...
        [
            clusterId, ...
            rangeIdxCpp, ...
            dopplerIdxCpp, ...
            angleRad
        ];
end


writematrix( ...
    angles, ...
    fullfile( ...
        angleDir, ...
        'angles.csv'));


fprintf( ...
    'Angles            : %d\n', ...
    size(angles,1));


%% 21. Export rawIQ -> iq_data.h
%
% Layout:
%
% [chirp][sample][rx][I/Q]
%
% RX1_I, RX1_Q, RX2_I, RX2_Q

IQ_TOTAL_SIZE = ...
    numSamples * ...
    numChirps * ...
    numRx * ...
    2;


fidIQ = fopen( ...
    'iq_data.h', ...
    'w');

if fidIQ == -1
    error('iq_data.h 파일을 열 수 없습니다.');
end


fprintf( ...
    fidIQ, ...
    '#pragma once\n\n');

fprintf( ...
    fidIQ, ...
    '#define NUM_SAMPLES %d\n', ...
    numSamples);

fprintf( ...
    fidIQ, ...
    '#define NUM_CHIRPS %d\n', ...
    numChirps);

fprintf( ...
    fidIQ, ...
    '#define NUM_RX %d\n', ...
    numRx);

fprintf( ...
    fidIQ, ...
    '#define IQ_COMPONENTS 2\n');

fprintf( ...
    fidIQ, ...
    '#define IQ_TOTAL_SIZE %d\n\n', ...
    IQ_TOTAL_SIZE);

fprintf( ...
    fidIQ, ...
    'static const float iq_data[IQ_TOTAL_SIZE] = {\n');


first = true;


for chirp = 1:numChirps

    for sample = 1:numSamples

        for rx = 1:numRx

            iVal = ...
                single( ...
                    real( ...
                        rawIQ( ...
                            sample, ...
                            chirp, ...
                            rx)));

            qVal = ...
                single( ...
                    imag( ...
                        rawIQ( ...
                            sample, ...
                            chirp, ...
                            rx)));


            if ~first
                fprintf(fidIQ, ',\n');
            end


            fprintf( ...
                fidIQ, ...
                '%.9ef', ...
                iVal);

            fprintf( ...
                fidIQ, ...
                ',\n');

            fprintf( ...
                fidIQ, ...
                '%.9ef', ...
                qVal);


            first = false;
        end
    end
end


fprintf( ...
    fidIQ, ...
    '\n};\n');

fclose(fidIQ);

fprintf('iq_data.h 생성 완료\n');


%% 22. Range-Doppler Map

rangeAxis = ...
    (0:numSamples-1) * ...
    rangeRes;

velocityAxis = ...
    (-numChirps/2:numChirps/2-1) * ...
    velocityRes;


rdmDB = ...
    10 * log10( ...
        powerMap / ...
        max(powerMap(:)) + ...
        eps);


figure;

imagesc( ...
    velocityAxis, ...
    rangeAxis, ...
    rdmDB);

axis xy;

xlabel( ...
    'Radial Velocity (m/s)');

ylabel( ...
    'Range (m)');

title( ...
    'Range-Doppler Map');

colorbar;

clim([-60 0]);


%% ============================================================
% Local Function
% ============================================================

function neighbours = scanNeighbours( ...
    detectedPoints, ...
    pointIdx, ...
    dbscanEps)

    neighbours = ...
        zeros(1,0);


    scanRange = ...
        detectedPoints( ...
            pointIdx, ...
            1);

    scanDoppler = ...
        detectedPoints( ...
            pointIdx, ...
            2);


    for detectedPoint = ...
            1:size(detectedPoints,1)

        rangeDiff = ...
            scanRange - ...
            detectedPoints( ...
                detectedPoint, ...
                1);

        dopplerDiff = ...
            scanDoppler - ...
            detectedPoints( ...
                detectedPoint, ...
                2);


        squaredDistance = ...
            rangeDiff * rangeDiff + ...
            dopplerDiff * dopplerDiff;


        if squaredDistance <= ...
                dbscanEps * dbscanEps

            neighbours(end + 1) = ...
                detectedPoint;
        end
    end
end