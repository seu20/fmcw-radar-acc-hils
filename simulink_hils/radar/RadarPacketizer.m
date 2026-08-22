classdef RadarPacketizer < matlab.System

    properties (Access = private)
        frameId_ = uint32(0)
    end

    properties (Constant, Access = private)
        MAGIC = uint32(hex2dec('52414452'))

        NUM_SAMPLES = 128
        NUM_CHIRPS  = 64
        NUM_RX      = 2

        MAX_PAYLOAD_SIZE = 1200

        HEADER_SIZE  = 14
        PACKET_COUNT = 110
        MAX_PACKET_SIZE = 1214
    end


    methods (Access = protected)

        function [packets, packetLengths] = stepImpl(obj, rawIQ)

            %% =====================================================
            % 1. RawIQ
            %
            % input:
            %   rawIQ(sample, chirp, rx)
            %
            % desired wire layout:
            %
            % [chirp][sample][rx][I/Q]
            %
            % RX1_I RX1_Q RX2_I RX2_Q ...
            %% =====================================================

            iq = permute(rawIQ, [3 1 2]);

            % [rx][sample][chirp]로 만들어서
            % MATLAB column-major flatten 시:
            %
            % rx -> sample -> chirp
            %
            % 순서가 되게 한다.
            iq = iq(:);


            %% =====================================================
            % 2. Complex single -> interleaved float32
            %% =====================================================

            iqFloats = zeros( ...
                numel(iq) * 2, ...
                1, ...
                'single');

            iqFloats(1:2:end) = ...
                single(real(iq));

            iqFloats(2:2:end) = ...
                single(imag(iq));


            %% =====================================================
            % 3. float32 -> raw uint8
            %
            % Payload protocol:
            % little-endian IEEE-754 float32
            %% =====================================================

            [~, ~, endian] = computer;

            if endian == 'B'
                iqFloats = swapbytes(iqFloats);
            end

            frameBytes = ...
                typecast(iqFloats, 'uint8');

            frameBytes = frameBytes(:);


            %% =====================================================
            % 4. Output buffers
            %
            % packet마다 최대:
            %
            % 14 byte header + 1200 byte payload
            %= 1214 byte
            %% =====================================================

            packets = zeros( ...
                obj.MAX_PACKET_SIZE, ...
                obj.PACKET_COUNT, ...
                'uint8');

            packetLengths = zeros( ...
                obj.PACKET_COUNT, ...
                1, ...
                'uint16');


            %% =====================================================
            % 5. 110 UDP packets 생성
            %% =====================================================

            for packetIdx = 0:(obj.PACKET_COUNT - 1)

                payloadStart = ...
                    packetIdx * obj.MAX_PAYLOAD_SIZE + 1;

                remainingBytes = ...
                    numel(frameBytes) - payloadStart + 1;

                payloadBytes = ...
                    min( ...
                        obj.MAX_PAYLOAD_SIZE, ...
                        remainingBytes);


                %% -----------------------------------------------
                % Header
                %
                % offset 0  : magic         uint32 BE
                % offset 4  : frame_id      uint32 BE
                % offset 8  : packet_id     uint16 BE
                % offset 10 : packet_count  uint16 BE
                % offset 12 : payload_bytes uint16 BE
                %% -----------------------------------------------

                header = zeros( ...
                    obj.HEADER_SIZE, ...
                    1, ...
                    'uint8');

                header(1:4) = ...
                    obj.uint32ToBigEndian( ...
                        obj.MAGIC);

                header(5:8) = ...
                    obj.uint32ToBigEndian( ...
                        obj.frameId_);

                header(9:10) = ...
                    obj.uint16ToBigEndian( ...
                        uint16(packetIdx));

                header(11:12) = ...
                    obj.uint16ToBigEndian( ...
                        uint16(obj.PACKET_COUNT));

                header(13:14) = ...
                    obj.uint16ToBigEndian( ...
                        uint16(payloadBytes));


                %% -----------------------------------------------
                % Header 저장
                %% -----------------------------------------------

                packets( ...
                    1:obj.HEADER_SIZE, ...
                    packetIdx + 1) = ...
                    header;


                %% -----------------------------------------------
                % Payload 저장
                %% -----------------------------------------------

                payloadEnd = ...
                    payloadStart + payloadBytes - 1;

                packets( ...
                    obj.HEADER_SIZE + 1 : ...
                    obj.HEADER_SIZE + payloadBytes, ...
                    packetIdx + 1) = ...
                    frameBytes( ...
                        payloadStart:payloadEnd);


                packetLengths(packetIdx + 1) = ...
                    uint16( ...
                        obj.HEADER_SIZE + payloadBytes);
            end


            %% =====================================================
            % 6. 다음 Radar Frame ID
            %% =====================================================

            obj.frameId_ = obj.frameId_ + uint32(1);
        end


        %% =========================================================
        % Output specification
        %% =========================================================

        function [size1, size2] = getOutputSizeImpl(obj)

            size1 = [ ...
                obj.MAX_PACKET_SIZE, ...
                obj.PACKET_COUNT ...
            ];

            size2 = [ ...
                obj.PACKET_COUNT, ...
                1 ...
            ];
        end


        function [type1, type2] = getOutputDataTypeImpl(~)

            type1 = 'uint8';
            type2 = 'uint16';
        end


        function [complex1, complex2] = ...
                isOutputComplexImpl(~)

            complex1 = false;
            complex2 = false;
        end


        function [fixed1, fixed2] = ...
                isOutputFixedSizeImpl(~)

            fixed1 = true;
            fixed2 = true;
        end


        function name = getInputNamesImpl(~)
            name = 'RawIQ';
        end


        function [name1, name2] = getOutputNamesImpl(~)

            name1 = 'Packets';
            name2 = 'PacketLengths';
        end
    end


    methods (Access = private)

        %% =========================================================
        % uint32 -> Network byte order
        %% =========================================================

        function bytes = uint32ToBigEndian(~, value)

            value = uint32(value);

            bytes = uint8([ ...
                bitand(bitshift(value, -24), uint32(255)); ...
                bitand(bitshift(value, -16), uint32(255)); ...
                bitand(bitshift(value,  -8), uint32(255)); ...
                bitand(value,              uint32(255)) ...
            ]);
        end


        %% =========================================================
        % uint16 -> Network byte order
        %% =========================================================

        function bytes = uint16ToBigEndian(~, value)

            value = uint16(value);

            bytes = uint8([ ...
                bitand(bitshift(value, -8), uint16(255)); ...
                bitand(value,              uint16(255)) ...
            ]);
        end
    end
end