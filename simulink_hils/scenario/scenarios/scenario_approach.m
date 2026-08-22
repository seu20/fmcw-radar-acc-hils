function config = scenario_approach()


%% ============================================
% Simulation
% =============================================

config.stopTime = 5;

config.sampleTime = 20e-6;


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
% Target
% =============================================

config.actors(1).name = ...
    'LeadCar';

config.actors(1).position = ...
    [30 -5 0];

config.actors(1).waypoints = [
    30 -5 0
    80 -5 0
    ];

config.actors(1).speed = [
    10
    10
    ];

config.actors(1).waitTime = [
    0
    0
    ];

end