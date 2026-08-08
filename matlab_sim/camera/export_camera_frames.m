%% export_camera_frames.m
% To Workspace 블록(변수명: camera_output, Save format: Structure With Time)에
% 로깅된 카메라 프레임을 PNG 파일로 저장하는 스크립트.
%
% 참고: 모델의 Data Import/Export 설정에서 "Single simulation output"이
% 켜져있으면, camera_output이 base workspace에 직접 생기지 않고
% out.camera_output 형태로 'out' 객체 안에 들어갑니다.
% 이 스크립트는 두 경우를 모두 처리합니다.
%
% 사용법:
%   1. Simulink에서 camera_capture_model.slx 시뮬레이션을 먼저 실행
%   2. 이 스크립트를 실행 (matlab_sim/camera 폴더에서 실행 권장)

clear outDir frames numFrames k frame fname co;

%% 1. camera_output 찾기 (base workspace 직접 or out 객체 안)
if exist('camera_output', 'var')
    co = camera_output;
elseif exist('out', 'var') && isa(out, 'Simulink.SimulationOutput') && isprop(out, 'camera_output')
    co = out.camera_output;
    fprintf('camera_output을 out 객체에서 찾았습니다 (out.camera_output).\n');
else
    error(['camera_output을 찾을 수 없습니다. ' ...
           'Simulink 시뮬레이션을 먼저 실행했는지, ' ...
           'To Workspace 블록의 Variable name이 camera_output인지 확인하세요.']);
end

%% 2. Structure With Time 포맷에서 프레임 데이터 꺼내기
% co.signals.values 크기: [H x W x C x N] (N = 프레임 수)
frames = co.signals.values;
numFrames = size(frames, 4);

fprintf('로깅된 프레임 수: %d\n', numFrames);
fprintf('프레임 크기: %d x %d x %d\n', size(frames,1), size(frames,2), size(frames,3));

%% 3. 저장 폴더 준비
outDir = fullfile(pwd, 'image_data');
if ~exist(outDir, 'dir')
    mkdir(outDir);
end

%% 4. 프레임별로 PNG 저장
for k = 1:numFrames
    frame = frames(:, :, :, k);

    % Simulink 비디오 신호는 종종 uint8이 아닐 수 있어 안전하게 변환
    if ~isa(frame, 'uint8')
        frame = im2uint8(frame);
    end

    fname = fullfile(outDir, sprintf('frame_%03d.png', k));
    imwrite(frame, fname);
end

fprintf('완료: %d장의 PNG를 %s 에 저장했습니다.\n', numFrames, outDir);