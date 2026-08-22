classdef RadarUdpSender < matlab.System

    properties (Nontunable)
        % Raspberry Pi IP
        DestinationIP = "10.0.0.3"

        % Raspberry Pi PacketReceiver bind port
        DestinationPort = 5000
    end


    properties (Access = private)
        udp_
    end


    properties (Constant, Access = private)
        PACKET_COUNT    = 110
        MAX_PACKET_SIZE = 1214
        HEADER_SIZE     = 14
    end


    methods (Access = protected)

        function setupImpl(obj, ~, ~)

            %% ===============================================
            % Destination 설정 검사
            %% ===============================================

            if strlength(string(obj.DestinationIP)) == 0
                error( ...
                    'RadarUdpSender:InvalidIP', ...
                    'DestinationIP을 설정해야 합니다.');
            end

            if obj.DestinationPort <= 0 || ...
               obj.DestinationPort > 65535

                error( ...
                    'RadarUdpSender:InvalidPort', ...
                    'DestinationPort는 1~65535 범위여야 합니다.');
            end


            %% ===============================================
            % UDP Socket 생성
            %
            % 1214 byte packet 하나가
            % 반드시 UDP datagram 하나가 되도록 설정
            %% ===============================================

            obj.udp_ = udpport( ...
                "datagram", ...
                "IPV4", ...
                "OutputDatagramSize", ...
                obj.MAX_PACKET_SIZE);
        end


        function sentCount = ...
                stepImpl(obj, packets, packetLengths)

            sentCount = uint16(0);


            %% ===============================================
            % Radar frame 1개
            %
            % 110 UDP datagram 전송
            %% ===============================================

            for packetIdx = 1:obj.PACKET_COUNT

                packetLength = ...
                    double(packetLengths(packetIdx));


                %% -------------------------------------------
                % Packet 길이 검증
                %% -------------------------------------------

                if packetLength < obj.HEADER_SIZE || ...
                   packetLength > obj.MAX_PACKET_SIZE

                    error( ...
                        'RadarUdpSender:InvalidPacketLength', ...
                        'Invalid packet length: %d', ...
                        packetLength);
                end


                %% -------------------------------------------
                % 실제 packet 길이만 추출
                %
                % packets는 항상 1214 x 110이지만
                % 마지막 packet은 실제로 286 byte
                %% -------------------------------------------

                packet = packets( ...
                    1:packetLength, ...
                    packetIdx);


                %% -------------------------------------------
                % UDP datagram 송신
                %% -------------------------------------------

                write( ...
                    obj.udp_, ...
                    packet, ...
                    "uint8", ...
                    string(obj.DestinationIP), ...
                    double(obj.DestinationPort));


                sentCount = sentCount + uint16(1);
            end
        end


        function releaseImpl(obj)

            % udpport reference 해제
            obj.udp_ = [];
        end


        %% ===============================================
        % Simulink Port 정보
        %% ===============================================

        function [name1, name2] = getInputNamesImpl(~)

            name1 = 'Packets';
            name2 = 'PacketLengths';
        end


        function name = getOutputNamesImpl(~)

            name = 'SentPackets';
        end


        function size = getOutputSizeImpl(~)

            size = [1 1];
        end


        function type = getOutputDataTypeImpl(~)

            type = 'uint16';
        end


        function complex = isOutputComplexImpl(~)

            complex = false;
        end


        function fixed = isOutputFixedSizeImpl(~)

            fixed = true;
        end
    end
end