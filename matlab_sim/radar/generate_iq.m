%% FMCW Radar - 2 RX IQ / Range-Doppler / Angle
clear; clc; close all;

addpath('../scenario')

%% 1. Scenario / Radar

scenarioFcn = @createScenario_approach;
[scnro, egoVehicle, radar] = scenarioFcn();


%% 2. Radar Parameters

fc = 77e9;

c = physconst('LightSpeed');
lambda = c / fc;

bandwidth = 150e6;
chirpDuration = 20e-6;

numSamples = 128;
numChirps = 64;
numRx = 2;

% ADC-equivalent sampling rate
adcFs = numSamples / chirpDuration;       % 6.4 MHz

% waveform simulation sampling rate
waveformFs = radar.Waveform.SampleRate;   % 150 MHz

% RX antenna spacing
d = lambda / 2;

% Chebyshev sidelobe attenuation
% chebwin(N)의 MATLAB 기본값 = 100 dB
chebAtten = 100;


%% 3. Resolution

rangeRes = c / (2 * bandwidth);

velocityRes = lambda / ...
    (2 * numChirps * chirpDuration);

fprintf('Range Resolution    : %.3f m\n', rangeRes);
fprintf('Velocity Resolution : %.3f m/s\n', velocityRes);
fprintf('ADC Fs              : %.2f MHz\n', adcFs / 1e6);
fprintf('Waveform Fs         : %.2f MHz\n', waveformFs / 1e6);
fprintf('Lambda              : %.3f mm\n', lambda * 1e3);
fprintf('RX spacing          : %.3f mm\n\n', d * 1e3);


%% 4. Scenario Timing

scnro.SampleTime = chirpDuration;

% rawIQ(sample, chirp, rx)
rawIQ = complex(zeros( ...
    numSamples, ...
    numChirps, ...
    numRx));

% resample ratio
[p, q] = rat(adcFs / waveformFs);

advance(scnro);


%% 5. IQ Acquisition

for chirpIdx = 1:numChirps

    poses = targetPoses(egoVehicle);
    time = scnro.SimulationTime;

    txRef = radar.Waveform();

    [iqSig, ~] = radar(poses, time);

    for rx = 1:numRx

        % Dechirp
        beatHighFs = ...
            dechirp(iqSig(:,rx), txRef);

        % 150 MHz -> 6.4 MHz
        beatADC = ...
            resample(beatHighFs, p, q);

        rawIQ(:,chirpIdx,rx) = ...
            beatADC(1:numSamples);
    end

    advance(scnro);
end

fprintf('IQ acquisition complete\n');


%% 6. Chebyshev Window + Range-Doppler FFT

% MATLAB과 C++에서 동일한 attenuation 사용
winRange = ...
    chebwin(numSamples, chebAtten);

winDoppler = ...
    chebwin(numChirps, chebAtten).';

rdm = complex(zeros( ...
    numSamples, ...
    numChirps, ...
    numRx));

for rx = 1:numRx

    % 2D Window
    iqWindowed = ...
        rawIQ(:,:,rx) .* ...
        winRange .* ...
        winDoppler;

    % Range FFT
    rangeFFT = ...
        fft(iqWindowed, numSamples, 1);

    % Doppler FFT
    rdm(:,:,rx) = ...
        fftshift( ...
            fft(rangeFFT, numChirps, 2), ...
            2);
end


%% 7. RX Power Combination

powerMap = ...
    abs(rdm(:,:,1)).^2 + ...
    abs(rdm(:,:,2)).^2;


%% 8. 2D CA-CFAR

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
    'ProbabilityFalseAlarm', pfa);

rangeStart = ...
    trainingRange + guardRange + 1;

rangeEnd = ...
    numSamples - trainingRange - guardRange;

dopplerStart = ...
    trainingDoppler + guardDoppler + 1;

dopplerEnd = ...
    numChirps - trainingDoppler - guardDoppler;


% CUT index 생성
numRangeCuts = ...
    rangeEnd - rangeStart + 1;

numDopplerCuts = ...
    dopplerEnd - dopplerStart + 1;

numCuts = ...
    numRangeCuts * numDopplerCuts;

cutIdx = zeros(2, numCuts);

idx = 1;

for dopplerBin = dopplerStart:dopplerEnd

    for rangeBin = rangeStart:rangeEnd

        cutIdx(:,idx) = ...
            [rangeBin; dopplerBin];

        idx = idx + 1;
    end
end


% CFAR
detections = ...
    cfar(powerMap, cutIdx);

detectedIdx = ...
    cutIdx(:,detections);

numDetections = ...
    size(detectedIdx, 2);

fprintf('CFAR detections : %d\n', ...
    numDetections);


%% 9. Detection Range / Velocity / Angle

detectedRanges = ...
    zeros(1, numDetections);

detectedVelocities = ...
    zeros(1, numDetections);

deltaPhis = ...
    zeros(1, numDetections);

detectedAngles = ...
    zeros(1, numDetections);


for k = 1:numDetections

    rangeBinIdx = ...
        detectedIdx(1,k);

    dopplerBinIdx = ...
        detectedIdx(2,k);


    % Range
    detectedRanges(k) = ...
        (rangeBinIdx - 1) * ...
        rangeRes;


    % Relative Velocity
    detectedVelocities(k) = ...
        ( ...
            dopplerBinIdx ...
            - numChirps/2 ...
            - 1 ...
        ) * velocityRes;


    % RX1 / RX2 complex value
    z1 = ...
        rdm( ...
            rangeBinIdx, ...
            dopplerBinIdx, ...
            1);

    z2 = ...
        rdm( ...
            rangeBinIdx, ...
            dopplerBinIdx, ...
            2);


    % RX phase difference
    deltaPhis(k) = ...
        angle(z2 * conj(z1));


    % d = lambda / 2
    sinTheta = ...
        deltaPhis(k) / pi;

    sinTheta = ...
        max(-1, min(1, sinTheta));


    % Angle
    detectedAngles(k) = ...
        asind(sinTheta);
end


%% 10. Export rawIQ -> iq_data.h
%
% Layout:
% [chirp][sample][rx][I/Q]
%
% RX1_I, RX1_Q, RX2_I, RX2_Q
%
% Data type:
% float32

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


fprintf(fidIQ, ...
    '#pragma once\n\n');

fprintf(fidIQ, ...
    '#define NUM_SAMPLES %d\n', ...
    numSamples);

fprintf(fidIQ, ...
    '#define NUM_CHIRPS %d\n', ...
    numChirps);

fprintf(fidIQ, ...
    '#define NUM_RX %d\n', ...
    numRx);

fprintf(fidIQ, ...
    '#define IQ_COMPONENTS 2\n');

fprintf(fidIQ, ...
    '#define IQ_TOTAL_SIZE %d\n\n', ...
    IQ_TOTAL_SIZE);

fprintf(fidIQ, ...
    'static const float iq_data[IQ_TOTAL_SIZE] = {\n');


first = true;

for chirp = 1:numChirps

    for sample = 1:numSamples

        for rx = 1:numRx

            iVal = ...
                single(real( ...
                    rawIQ(sample,chirp,rx)));

            qVal = ...
                single(imag( ...
                    rawIQ(sample,chirp,rx)));


            if ~first
                fprintf(fidIQ, ',\n');
            end

            fprintf(fidIQ, ...
                '%.9ef', ...
                iVal);

            fprintf(fidIQ, ',\n');

            fprintf(fidIQ, ...
                '%.9ef', ...
                qVal);

            first = false;
        end
    end
end


fprintf(fidIQ, '\n};\n');

fclose(fidIQ);

fprintf('iq_data.h 생성 완료\n');


%% 11. Export Chebyshev Window -> cheb_window.h

rangeChebWindow = ...
    single(winRange);

dopplerChebWindow = ...
    single(winDoppler.');


fidWin = fopen( ...
    'cheb_window.h', ...
    'w');

if fidWin == -1
    error('cheb_window.h 파일을 열 수 없습니다.');
end


fprintf(fidWin, ...
    '#pragma once\n\n');

fprintf(fidWin, ...
    '#define RANGE_WINDOW_SIZE %d\n', ...
    numSamples);

fprintf(fidWin, ...
    '#define DOPPLER_WINDOW_SIZE %d\n\n', ...
    numChirps);


% Range Window
fprintf(fidWin, ...
    'static const float range_cheb_window[RANGE_WINDOW_SIZE] = {\n');

for i = 1:numSamples

    fprintf(fidWin, ...
        '    %.9ef', ...
        rangeChebWindow(i));

    if i < numSamples
        fprintf(fidWin, ',');
    end

    fprintf(fidWin, '\n');
end

fprintf(fidWin, '};\n\n');


% Doppler Window
fprintf(fidWin, ...
    'static const float doppler_cheb_window[DOPPLER_WINDOW_SIZE] = {\n');

for i = 1:numChirps

    fprintf(fidWin, ...
        '    %.9ef', ...
        dopplerChebWindow(i));

    if i < numChirps
        fprintf(fidWin, ',');
    end

    fprintf(fidWin, '\n');
end

fprintf(fidWin, '};\n');

fclose(fidWin);

fprintf('cheb_window.h 생성 완료\n');


%% 12. Export RDM -> CSV

dataDir = 'data';

if ~exist(dataDir, 'dir')
    mkdir(dataDir);
end

writematrix( ...
    real(rdm(:,:,1)), ...
    fullfile(dataDir, 'rdm_rx1_real.csv'));

writematrix( ...
    imag(rdm(:,:,1)), ...
    fullfile(dataDir, 'rdm_rx1_imag.csv'));

writematrix( ...
    real(rdm(:,:,2)), ...
    fullfile(dataDir, 'rdm_rx2_real.csv'));

writematrix( ...
    imag(rdm(:,:,2)), ...
    fullfile(dataDir, 'rdm_rx2_imag.csv'));

fprintf('RDM CSV export complete\n');


%% 13. Range-Doppler Map

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

xlabel('Radial Velocity (m/s)');
ylabel('Range (m)');
title('Range-Doppler Map');

colorbar;

clim([-60 0]);