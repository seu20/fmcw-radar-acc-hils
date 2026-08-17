#pragma once
#include <vector>
#include "RadarTypes.hpp"

struct TargetFrame {
    uint32_t frame_id;
    std::vector<Target> targets;
};

struct LeadTargetFrame {
    uint32_t frame_id;
    uint8_t valid;
    Target target;
};