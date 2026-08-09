#pragma once
#include <stdint.h>
#include <vector>
#include <complex>

// Reassembler 에서 데이터 정렬 후 float 로 변환시킨 한 packet

struct RadarFrame {
    uint32_t frame_id;
    std::vector<std::complex<float>> iq_data;
};