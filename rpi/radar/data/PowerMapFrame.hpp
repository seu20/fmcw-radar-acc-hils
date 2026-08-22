#pragma once

#include <cstdint>
#include <vector>

struct PowerMapFrame
{
    uint32_t frame_id;

    // [range_bin][doppler_bin]
    // 128 × 64 = 8192 float
    std::vector<float> power;
};