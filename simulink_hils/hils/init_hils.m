%% HILS Simulation Initialization
% Runtime initialization only.
% This script does NOT open, close, modify, or reconfigure simulink_hils.slx.

thisFolder = fileparts(mfilename('fullpath'));
projectRoot = fileparts(thisFolder);

%% =========================================================
% Generated file folders
%% =========================================================
cacheDir   = fullfile(projectRoot, 'build', 'cache');
codegenDir = fullfile(projectRoot, 'build', 'codegen');

Simulink.fileGenControl( ...
    'set', ...
    'CacheFolder', cacheDir, ...
    'CodeGenFolder', codegenDir, ...
    'createDir', true);

%% =========================================================
% Project paths
%% =========================================================
addpath(thisFolder);
addpath(fullfile(projectRoot, 'scenario'));
addpath(fullfile(projectRoot, 'scenario', 'scenarios'));
addpath(fullfile(projectRoot, 'radar'));
addpath(fullfile(projectRoot, 'network'));

%% =========================================================
% Ego Pose Bus
%% =========================================================
clear EgoPoseBus elems

elems(1) = Simulink.BusElement;
elems(1).Name = 'ActorID';
elems(1).Dimensions = 1;
elems(1).DataType = 'double';

elems(2) = Simulink.BusElement;
elems(2).Name = 'Position';
elems(2).Dimensions = [1 3];
elems(2).DataType = 'double';

elems(3) = Simulink.BusElement;
elems(3).Name = 'Velocity';
elems(3).Dimensions = [1 3];
elems(3).DataType = 'double';

elems(4) = Simulink.BusElement;
elems(4).Name = 'Roll';
elems(4).Dimensions = 1;
elems(4).DataType = 'double';

elems(5) = Simulink.BusElement;
elems(5).Name = 'Pitch';
elems(5).Dimensions = 1;
elems(5).DataType = 'double';

elems(6) = Simulink.BusElement;
elems(6).Name = 'Yaw';
elems(6).Dimensions = 1;
elems(6).DataType = 'double';

elems(7) = Simulink.BusElement;
elems(7).Name = 'AngularVelocity';
elems(7).Dimensions = [1 3];
elems(7).DataType = 'double';

EgoPoseBus = Simulink.Bus;
EgoPoseBus.Elements = elems;
clear elems;

%% =========================================================
% Scenario
%% =========================================================
scenarioName = "demo";

clear scenarioConfig scnro egoVehicle actors radar radarAcc

[ ...
    scnro, ...
    egoVehicle, ...
    actors, ...
    scenarioConfig ...
] = createScenario(scenarioName);

%% =========================================================
% Radar
%% =========================================================
radar = createRadar(egoVehicle, scnro);
radarAcc = createRadar(egoVehicle, scnro);

%% =========================================================
% Summary
%% =========================================================
fprintf('\n');
fprintf('HILS Scenario : %s\n', scenarioName);
fprintf('Ego X         : %.2f m\n', scenarioConfig.ego.position(1));
fprintf('Ego Y         : %.2f m\n', scenarioConfig.ego.position(2));
fprintf('Ego Velocity  : %.2f m/s\n', scenarioConfig.ego.initialSpeed);
fprintf('Actors        : %d\n', numel(actors));
fprintf('\n');

fprintf('ACC source    : MATLAB FMCW radar -> primitive LeadTarget[5] interface\n');
