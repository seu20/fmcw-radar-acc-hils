#pragma once

#include <cstddef>

struct Detection
{
    size_t range_idx;
    size_t doppler_idx;
    float power;
};