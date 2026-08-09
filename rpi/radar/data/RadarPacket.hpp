#pragma once
#include <stdint.h>
#include <vector>
// UDP로 수신 받는 데이터 프레임
// 한 프레임을 여러 패킷으로 끊어서 수신
// UDP는 순서를 지키지 않아서 재조립 필요

#define RADAR_MAGIC 0x52414452
struct RadarPacketHeader {
    uint32_t magic_id;      
    uint32_t frame_id;      

    uint16_t packet_id;     // 현재 몇개째인지
    uint16_t packet_count;  // 총 110 개 

    uint16_t payload_bytes; // 마지막 제외: 1200 bytes 
};

struct RadarPacket {
    struct RadarPacketHeader header;

    // UDP는 바이트 단위 전송이라 byte로 받음
    // Reassembler 에서 원본 IQ 데이터로 재조립 
    std::vector<uint8_t> payload;       
};
