function loadSceneParams()
% createScenario_approach.m을 읽어서 Ego와 Target의 위치/속도를 Workspace에 할당합니다.

% scenario 폴더를 경로에 추가
thisFolder = fileparts(mfilename('fullpath'));
addpath(fullfile(thisFolder, '..', 'scenario'));

% 1. 마스터 시나리오 로드
[scnro, egoVehicle, ~] = createScenario_approach();
stopTime = scnro.StopTime;

% 2. Target 액터 특정
allActors = scnro.Actors;
targetActor = allActors(2);

% 3. t=0 초기 위치 기록
ego_pos = egoVehicle.Position;
target_pos = targetActor.Position;
% 4. 한 스텝 전진시켜서 정확한 이동 속도(스칼라) 계산
dt = scnro.SampleTime;
advance(scnro);

ego_pos_next = egoVehicle.Position;
target_pos_next = targetActor.Position;

ego_speed = (ego_pos_next - ego_pos) / dt;
target_speed = (target_pos_next - target_pos) / dt;

% 시나리오 원상복구
restart(scnro);

% 5. Simulink 연동을 위해 Base Workspace에 변수 강제 할당
assignin('base', 'ego_pos', ego_pos);
assignin('base', 'ego_speed', ego_speed);
assignin('base', 'target_pos', target_pos);
assignin('base', 'target_speed', target_speed);
assignin('base', 'stopTime', stopTime);

% 6. 출력용 속력(스칼라)은 별도로 계산 — fprintf 포맷과 인자 개수를 맞추기 위함
ego_speed_mag = norm(ego_speed);
target_speed_mag = norm(target_speed);

fprintf('Workspace 할당 완료:\n');
fprintf(' - Ego 차량   : 초기위치 [%.1f, %.1f, %.1f], 속도 %.2f m/s\n', ego_pos, ego_speed_mag);
fprintf(' - Target 차량: 초기위치 [%.1f, %.1f, %.1f], 속도 %.2f m/s\n', target_pos, target_speed_mag);
end