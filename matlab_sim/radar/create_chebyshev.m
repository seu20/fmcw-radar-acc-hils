%% export_cheb_window.m

clear; clc;

numSamples = 128;
numChirps = 64;

% generate_iq.m과 동일한 Chebyshev Window
rangeChebWindow   = single(chebwin(numSamples));
dopplerChebWindow = single(chebwin(numChirps));

%% 현재 작업 폴더의 data/ 생성
dataDir = fullfile(pwd, 'data');

if ~exist(dataDir, 'dir')
    mkdir(dataDir);
end

%% data/cheb_window.h
outputPath = fullfile(dataDir, 'cheb_window.h');

fid = fopen(outputPath, 'w');

if fid == -1
    error('cheb_window.h 파일을 열 수 없습니다.');
end

fprintf(fid, '#pragma once\n\n');

fprintf(fid, '#define RANGE_WINDOW_SIZE %d\n', numSamples);
fprintf(fid, '#define DOPPLER_WINDOW_SIZE %d\n\n', numChirps);

%% Range Window
fprintf(fid, ...
    'static const float range_cheb_window[RANGE_WINDOW_SIZE] = {\n');

for i = 1:numSamples

    fprintf(fid, '    %.9ef', rangeChebWindow(i));

    if i < numSamples
        fprintf(fid, ',');
    end

    fprintf(fid, '\n');
end

fprintf(fid, '};\n\n');

%% Doppler Window
fprintf(fid, ...
    'static const float doppler_cheb_window[DOPPLER_WINDOW_SIZE] = {\n');

for i = 1:numChirps

    fprintf(fid, '    %.9ef', dopplerChebWindow(i));

    if i < numChirps
        fprintf(fid, ',');
    end

    fprintf(fid, '\n');
end

fprintf(fid, '};\n');

fclose(fid);