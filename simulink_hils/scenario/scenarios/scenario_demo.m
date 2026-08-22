function config = scenario_demo()

%% ============================================
% Simulation
% =============================================
config.stopTime = 30;
config.sampleTime = 0.01;

%% ============================================
% Road
% =============================================
config.roadCenters = [
    -800 0 0
     800 0 0
];

%% ============================================
% Ego Initial State
% =============================================
config.ego.position = ...
    [0 -5 0];

config.ego.initialSpeed = ...
    20;

config.ego.yaw = ...
    0;

%% ============================================
% Scenario 1 : Lead Car - Same Lane
%
% 0 ~ 1 s   : 20 m/s
% 1 ~ 3 s   : 20 -> 0 m/s
% 3 ~ 4 s   : stop
% 4 ~ 6 s   : 0 -> 20 m/s
% 6 s 이후  : 20 m/s
% =============================================
config.actors(1).name = ...
    'LeadCar';

config.actors(1).position = ...
    [30 -5 0];

config.actors(1).waypoints = [
     30 -5 0
     50 -5 0
     70 -5 0
     90 -5 0
    600 -5 0
];

config.actors(1).speed = [
    20
    20
     0
    20
    20
];

config.actors(1).waitTime = [
    0
    0
    1.0
    0
    0
];

%% ============================================
% Scenario 2 : Stationary Adjacent-Lane Car
%
% Scenario 1에서는 충분히 앞에 있어 시각적 집중을 방해하지 않음.
% 차량은 옆 차선 x = 115 m에서 전체 simulation 동안 정지.
%
% Scenario 1의 LeadCar 이벤트가 약 6 s에 끝난 뒤,
% Ego는 약 3 s 후(약 9 s)에 이 정지 차량 옆을 통과한다.
%
% Radar에는 검출될 수 있지만 angle/lateral 기반 Lead selection에서
% 제외되어 ACC 가속/감속에 영향을 주지 않아야 한다.
% =============================================
config.actors(2).name = ...
    'AdjacentCar';

config.actors(2).position = ...
    [115 -1.15 0];

config.actors(2).waypoints = [
    115 -1.15 0
    600 -1.15 0
];

config.actors(2).speed = [
     0
    20
];

config.actors(2).waitTime = [
    30
     0
];

end
