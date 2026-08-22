function leadTarget = decodeLeadTargetFrame(packet)
% decodeLeadTargetFrame Decode RPi LeadTargetFrame (default C++ alignment).
%
% C++ layout (Raspberry Pi / x86 little-endian ABI):
%   byte  0..3   uint32 frame_id
%   byte  4      uint8  valid
%   byte  5..7   padding
%   byte  8..11  float  distance
%   byte 12..15  float  relative_velocity
%   byte 16..19  float  angle
%
% Output is the stable MATLAB ACC boundary:
%   [frame_id; valid; distance; relative_velocity; angle]  (5x1 double)
%
% Only this decoder knows about UDP padding/alignment.  ACC does not.

packet = uint8(packet(:));
if numel(packet) ~= 20
    error('LeadTargetFrame packet must be exactly 20 bytes.');
end

frameID = typecast(packet(1:4),   'uint32');
valid   = packet(5);
distance = typecast(packet(9:12),  'single');
relVel   = typecast(packet(13:16), 'single');
angleRad = typecast(packet(17:20), 'single');

leadTarget = [double(frameID); double(valid); double(distance); ...
              double(relVel); double(angleRad)];
end
