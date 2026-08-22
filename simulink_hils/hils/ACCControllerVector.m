classdef ACCControllerVector < matlab.System
    % ACCControllerVector
    %
    % Stable ACC boundary without a Simulink.Bus object.
    % Input 1: LeadTarget [5x1] double
    %   [frame_id; valid; distance; relative_velocity; angle]
    % Input 2: EgoSpeed [m/s]
    %
    % The source can later change from MATLAB radar to the RPi UDP decoder
    % without changing this controller or the top-level model wiring.

    properties
        SetSpeed = 20.0
        MinDistance = 10.0
        TimeHeadway = 1.0
        KpSpeed = 2.5
        KpGap = 1.2
        KvRelative = 2.5
        MaxAcceleration = 4.0
        MaxDeceleration = -10.0
        SampleTime = 0.01
    end

    methods (Access = protected)
        function [accelCmd, leadDistance, desiredDistance, relativeVelocity, leadValid] = ...
                stepImpl(obj, leadTarget, egoSpeed)
            data = double(leadTarget(:));
            if numel(data) ~= 5
                error('LeadTarget vector must contain exactly 5 elements.');
            end

            frameID = data(1); %#ok<NASGU>
            leadValid = data(2) ~= 0.0;
            leadDistance = data(3);
            relativeVelocity = data(4);
            angleRad = data(5); %#ok<NASGU>

            egoSpeed = max(double(egoSpeed), 0.0);
            desiredDistance = obj.MinDistance + obj.TimeHeadway * egoSpeed;

            speedError = obj.SetSpeed - egoSpeed;
            accelSpeed = obj.KpSpeed * speedError;

            if leadValid && leadDistance > 0.0
                spacingError = leadDistance - desiredDistance;
                accelSpacing = obj.KpGap * spacingError + ...
                    obj.KvRelative * relativeVelocity;
                accelCmd = min(accelSpeed, accelSpacing);
            else
                accelCmd = accelSpeed;
                leadDistance = -1.0;
                relativeVelocity = 0.0;
                leadValid = false;
            end

            accelCmd = min(accelCmd, obj.MaxAcceleration);
            accelCmd = max(accelCmd, obj.MaxDeceleration);
            if egoSpeed <= 0.05 && accelCmd < 0.0
                accelCmd = 0.0;
            end
        end

        function [s1,s2,s3,s4,s5] = getOutputSizeImpl(~)
            s1=[1 1]; s2=[1 1]; s3=[1 1]; s4=[1 1]; s5=[1 1];
        end
        function [t1,t2,t3,t4,t5] = getOutputDataTypeImpl(~)
            t1='double'; t2='double'; t3='double'; t4='double'; t5='logical';
        end
        function [c1,c2,c3,c4,c5] = isOutputComplexImpl(~)
            c1=false; c2=false; c3=false; c4=false; c5=false;
        end
        function [f1,f2,f3,f4,f5] = isOutputFixedSizeImpl(~)
            f1=true; f2=true; f3=true; f4=true; f5=true;
        end
        function [n1,n2] = getInputNamesImpl(~)
            n1='LeadTarget'; n2='EgoSpeed';
        end
        function [n1,n2,n3,n4,n5] = getOutputNamesImpl(~)
            n1='AccelerationCmd'; n2='LeadDistance'; n3='DesiredDistance'; ...
            n4='RelativeVelocity'; n5='LeadValid';
        end
        function sts = getSampleTimeImpl(obj)
            sts = createSampleTime(obj,'Type','Discrete', ...
                'SampleTime',obj.SampleTime,'OffsetTime',0);
        end
    end
end
