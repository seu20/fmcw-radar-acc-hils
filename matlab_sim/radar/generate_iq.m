%% FMCW Radar - 2 RX IQ / Range-Doppler / Angle
clear; clc; close all;

addpath('../scenario')

%% 1. Scenario / Radar
scenarioFcn = @createScenario_approach;
[scnro, egoVehicle, radar] = scenarioFcn();

%% 2. Radar parameters
fc = 77e9;
c = physconst('LightSpeed');
lambda = c / fc;

bandwidth = 150e6;
chirpDuration = 20e-6;

numSamples = 128;
numChirps = 64;
numRx = 2;

% dechirp 후 ADC-equivalent sampling rate
adcFs = numSamples / chirpDuration;       % 6.4 MHz

% dechirp 전 waveform simulation sampling rate
waveformFs = radar.Waveform.SampleRate;   % 150 MHz

sweepSlope = bandwidth / chirpDuration;

% RX antenna spacing
d = lambda / 2;

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

%% 4. Scenario timing
scnro.SampleTime = chirpDuration;

% rawIQ(sample, chirp, rx)
rawIQ = complex(zeros(numSamples, numChirps, numRx));

% resample ratio
[p, q] = rat(adcFs / waveformFs);

advance(scnro);

%% 5. IQ acquisition
for chirpIdx = 1:numChirps

    poses = targetPoses(egoVehicle);
    time = scnro.SimulationTime;

    txRef = radar.Waveform();
    [iqSig, ~] = radar(poses, time);

    for rx = 1:numRx

        % dechirp
        beatHighFs = dechirp(iqSig(:,rx), txRef);

        % 150 MHz -> 6.4 MHz
        beatADC = resample(beatHighFs, p, q);

        rawIQ(:, chirpIdx, rx) = ...
            beatADC(1:numSamples);
    end

    advance(scnro);
end

fprintf('IQ acquisition complete\n');
fprintf('rawIQ size = ');
disp(size(rawIQ));

%% 6. Window + Range-Doppler FFT
winRange = hamming(numSamples);
winDoppler = hamming(numChirps).';

rdm = complex(zeros(numSamples, numChirps, numRx));

for rx = 1:numRx

    iqWindowed = ...
        rawIQ(:,:,rx) .* winRange .* winDoppler;

    % Range FFT
    rangeFFT = fft(iqWindowed, numSamples, 1);

    % Doppler FFT
    rdm(:,:,rx) = fftshift( ...
        fft(rangeFFT, numChirps, 2), 2);
end

%% 7. 두 RX power 결합
powerMap = ...
    abs(rdm(:,:,1)).^2 + ...
    abs(rdm(:,:,2)).^2;

%% 8. Peak detection
[~, maxIdx] = max(powerMap(:));

[rangeBinIdx, dopplerBinIdx] = ...
    ind2sub(size(powerMap), maxIdx);

detectedRange = ...
    (rangeBinIdx - 1) * rangeRes;

detectedVelocity = ...
    (dopplerBinIdx - numChirps/2 - 1) ...
    * velocityRes;

%% 9. Angle estimation
% 같은 Range-Doppler bin에서 RX1/RX2 복소수 값
z1 = rdm(rangeBinIdx, dopplerBinIdx, 1);
z2 = rdm(rangeBinIdx, dopplerBinIdx, 2);

% RX 채널 위상차
deltaPhi = angle(z2 * conj(z1));

% d = lambda/2
sinTheta = deltaPhi / pi;

% 수치오차 보호
sinTheta = max(-1, min(1, sinTheta));

detectedAngle = asind(sinTheta);

%% 10. Results
fprintf('\n=== Detection Result ===\n');
fprintf('Range       : %.2f m\n', detectedRange);
fprintf('Velocity    : %.2f m/s\n', detectedVelocity);
fprintf('Phase diff  : %.2f deg\n', rad2deg(deltaPhi));
fprintf('Angle       : %.2f deg\n', detectedAngle);

%% Export rawIQ -> iq_data.h
% Layout:
% [chirp][sample][rx][I/Q]
%
% iq_data =
% RX1_I, RX1_Q, RX2_I, RX2_Q,   <- chirp 0, sample 0
% RX1_I, RX1_Q, RX2_I, RX2_Q,   <- chirp 0, sample 1
% ...
%
% Data type: float32

IQ_TOTAL_SIZE = numSamples * numChirps * numRx * 2;

fid = fopen('iq_data.h', 'w');

if fid == -1
    error('iq_data.h 파일을 열 수 없습니다.');
end

fprintf(fid, '#pragma once\n\n');

fprintf(fid, '#define NUM_SAMPLES %d\n', numSamples);
fprintf(fid, '#define NUM_CHIRPS %d\n', numChirps);
fprintf(fid, '#define NUM_RX %d\n', numRx);
fprintf(fid, '#define IQ_COMPONENTS 2\n');
fprintf(fid, '#define IQ_TOTAL_SIZE %d\n\n', IQ_TOTAL_SIZE);

fprintf(fid, 'static const float iq_data[IQ_TOTAL_SIZE] = {\n');

first = true;

for chirp = 1:numChirps
    for sample = 1:numSamples
        for rx = 1:numRx

            iVal = single(real(rawIQ(sample, chirp, rx)));
            qVal = single(imag(rawIQ(sample, chirp, rx)));

            % I
            if ~first
                fprintf(fid, ',\n');
            end

            fprintf(fid, '%.9ef', iVal);
            first = false;

            % Q
            fprintf(fid, ',\n');
            fprintf(fid, '%.9ef', qVal);
        end
    end
end

fprintf(fid, '\n};\n');

fclose(fid);

fprintf('iq_data.h 생성 완료\n');
fprintf('IQ elements : %d\n', IQ_TOTAL_SIZE);
fprintf('Frame bytes : %d bytes\n', IQ_TOTAL_SIZE * 4);

%% Export RDM -> CSV
% rdm layout:
% [range_bin][doppler_bin][rx]
%
% 각 CSV 크기:
% 128 rows x 64 columns
%
% RX1 / RX2 각각 real, imag 분리 저장

writematrix( ...
    real(rdm(:,:,1)), ...
    'rdm_rx1_real.csv');

writematrix( ...
    imag(rdm(:,:,1)), ...
    'rdm_rx1_imag.csv');

writematrix( ...
    real(rdm(:,:,2)), ...
    'rdm_rx2_real.csv');

writematrix( ...
    imag(rdm(:,:,2)), ...
    'rdm_rx2_imag.csv');

fprintf('RDM CSV export complete\n');
fprintf('RDM size : %d x %d x %d\n', ...
    numSamples, numChirps, numRx);

%% 11. Range-Doppler Map
rangeAxis = (0:numSamples-1) * rangeRes;

velocityAxis = ...
    (-numChirps/2:numChirps/2-1) ...
    * velocityRes;

rdmDB = 10 * log10( ...
    powerMap / max(powerMap(:)) + eps);

figure;

imagesc(velocityAxis, rangeAxis, rdmDB);
axis xy;

xlabel('Radial Velocity (m/s)');
ylabel('Range (m)');
title('Range-Doppler Map');

colorbar;
clim([-60 0]);

hold on;

plot( ...
    detectedVelocity, ...
    detectedRange, ...
    'rx', ...
    'MarkerSize', 12, ...
    'LineWidth', 2);

hold off;