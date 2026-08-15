#pragma once 
#include "Detection.hpp"
#include <vector>

struct Cluster {
    int cluster_id;
    std::vector<Detection> points;
};

class DBSCAN {
private:
    // 반경 반지금
    float eps;

    // core point가 되기 위해 주변에 있어야하는 최소 점들
    float min_samples;

public:
    // 생성자
    DBSCAN():
        eps(),
        min_samples()
    {}

    // CFAR로 감지된 점들 유효한 물체 탐지
    void cluster(const std::vector<Detection>& detected_points_);

    
};