#pragma once
#include <cstddef>
#include <vector>

struct Detection
{
    size_t range_idx;
    size_t doppler_idx;
    float power;
};

struct Peak {
    size_t range_idx;
    size_t doppler_idx;
    float power;
};

// Processor 에서 Controller로 보내는 타겟의 정보
struct Target {
    float distance;
    float relative_velocity;
    float angle;
};

struct Cluster {
    int cluster_id;
    std::vector<Detection> points;
};