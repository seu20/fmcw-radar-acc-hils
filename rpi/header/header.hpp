#pragma once
#include <stdint.h>

typedef struct {
    uint32_t magic;
    uint32_t frame_id;

    uint16_t packet_id;
    uint16_t packet_count;

    uint64_t simulation_timestamp_us;

    uint16_t sample_count;
    uint16_t chirp_count;

    uint8_t  rx_count;
    uint8_t  data_type;
    uint16_t payload_bytes;
} RadarPacketHeader;

