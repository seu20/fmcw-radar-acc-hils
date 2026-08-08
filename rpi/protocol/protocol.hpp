#pragma once
#include <stdint.h>

#pragma pack(push, 1)

struct RadarPacketHeader
{
    uint32_t frame_id;      // 
    uint16_t packet_id;     //
    uint16_t packet_count;  // 
    uint16_t payload_bytes; // 이번 frame에 총 몇개의 바이트가 차지하는지
};

#pragma pack(pop)