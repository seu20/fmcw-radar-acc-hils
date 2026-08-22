function radar = createRadar(egoVehicle, scnro)

%% FMCW 파라미터
fc = 77e9;
c = physconst('LightSpeed');
lambda = c / fc;

bandwidth = 150e6;
chirpDuration = 20e-6;

% dechirp 전 FMCW waveform 시뮬레이션용
waveformFs = 150e6;

%% Mounting 위치
frontBumperX = egoVehicle.Length - egoVehicle.RearOverhang;
sensorZ = 0.2;

mountingLocation = [frontBumperX, 0, sensorZ];

%% 최대 거리
maxRange = 100;

if length(scnro.Actors) > 1
    targetVehicle = scnro.Actors(2);

    initialDistance = ...
        norm(targetVehicle.Position - egoVehicle.Position);

    maxRange = max(100, initialDistance * 1.5);
end

%% Radar
radar = radarTransceiver( ...
    'MountingLocation', mountingLocation, ...
    'RangeLimits', [0 maxRange]);

%% FMCW waveform
radar.Waveform = phased.FMCWWaveform( ...
    'SampleRate', waveformFs, ...
    'SweepBandwidth', bandwidth, ...
    'SweepTime', chirpDuration, ...
    'SweepDirection', 'Up');

%% TX/RX 설정
radar.Transmitter.PeakPower = db2pow(5)*1e-3;
radar.Transmitter.Gain = 36;

radar.Receiver.Gain = 42;
radar.Receiver.NoiseFigure = 4.5;
radar.Receiver.SampleRate = waveformFs;

radar.TransmitAntenna.OperatingFrequency = fc;
radar.ReceiveAntenna.OperatingFrequency = fc;

%% ============================
% 2 RX ULA
% ============================

rxArray = phased.ULA( ...
    'NumElements', 2, ...
    'ElementSpacing', lambda/2, ...
    'ArrayAxis', 'y');

radar.ReceiveAntenna.Sensor = rxArray;

end