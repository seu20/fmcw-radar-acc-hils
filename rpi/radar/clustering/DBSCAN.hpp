#pragma once 
#include "RadarTypes.hpp"
#include <vector>

class DBSCAN {
private:
    // 반경 반지금
    float eps = 1.5f;

    // core point가 되기 위해 주변에 있어야하는 최소 점들
    int min_samples = 3;

    // 검사하는 점이 방문됐었는지 or 어떤 cluster인지
    std::vector<int> labels;

    std::vector<Cluster> clusters;

    static constexpr int UNVISITED = -1;
    static constexpr int  NOISE = -2;

    float distance_squared(
        const Detection& scan_point,
        const Detection& detected_point
    );

    std::vector<size_t> scan_neighbour(
        const std::vector<Detection>& detected_points,
        size_t point
    );

public:
    // 생성자
    // DBSCAN():
    // {}

    // CFAR로 감지된 점들 유효한 물체 탐지
    void scan(const std::vector<Detection>& detected_points_);

    // Clusters 반환 함수
    const std::vector<Cluster>& getClusters() const 
    { return clusters; }

};