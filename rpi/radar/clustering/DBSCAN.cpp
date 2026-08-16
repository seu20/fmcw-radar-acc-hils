#include "DBSCAN.hpp"
#include <cmath>

float DBSCAN::distance_squared(
    const Detection& scan_point,
    const Detection& detected_point)
{
    float range_diff =
        static_cast<float>(scan_point.range_idx) -
        static_cast<float>(detected_point.range_idx);

    float doppler_diff =
        static_cast<float>(scan_point.doppler_idx) -
        static_cast<float>(detected_point.doppler_idx);

    float squared_distance =
        range_diff * range_diff +
        doppler_diff * doppler_diff;

    return squared_distance;
}


std::vector<size_t> DBSCAN::scan_neighbour(
    const std::vector<Detection>& detected_points,
    size_t point)
{
    std::vector<size_t> neighbours;

    const Detection& scan_point = detected_points[point];

    for (size_t detected_point = 0;
         detected_point < detected_points.size();
         ++detected_point)
    {
        if (distance_squared(
                scan_point,
                detected_points[detected_point])
            <= eps * eps)
        {
            // Detection 자체가 아니라 index 저장
            neighbours.push_back(detected_point);
        }
    }

    return neighbours;
}


void DBSCAN::scan(
    const std::vector<Detection>& detected_points)
{
    // ==============================
    // Frame 단위 초기화
    // ==============================

    labels.assign(
        detected_points.size(),
        UNVISITED
    );

    clusters.clear();

    int cluster_id = 0;


    // ==============================
    // 모든 Detection 검사
    // ==============================

    for (size_t point = 0;
         point < detected_points.size();
         ++point)
    {
        // 이미 이전 Cluster 확장에서 처리됐으면 skip
        if (labels[point] != UNVISITED)
        {
            continue;
        }


        // ==============================
        // 현재 Point의 neighbour 검색
        // ==============================

        std::vector<size_t> neighbours =
            scan_neighbour(
                detected_points,
                point
            );


        // ==============================
        // Core Point가 아니면 Noise
        // ==============================

        if (neighbours.size() < min_samples)
        {
            labels[point] = NOISE;
            continue;
        }


        // ==============================
        // 새로운 Cluster 생성
        // ==============================

        Cluster cluster;

        cluster.cluster_id = cluster_id;

        // 시작 Core Point
        labels[point] = cluster_id;

        cluster.points.push_back(
            detected_points[point]
        );


        // ==============================
        // Cluster Expansion
        // ==============================

        for (size_t i = 0;
             i < neighbours.size();
             ++i)
        {
            size_t neighbour_idx =
                neighbours[i];


            // --------------------------------
            // 이전에 Noise였던 Point
            //
            // 현재 Core Point의 neighbour로
            // 발견됐으므로 Border Point가 됨
            // --------------------------------

            if (labels[neighbour_idx] == NOISE)
            {
                labels[neighbour_idx] =
                    cluster_id;

                cluster.points.push_back(
                    detected_points[neighbour_idx]
                );

                continue;
            }


            // --------------------------------
            // 이미 처리된 Point
            // --------------------------------

            if (labels[neighbour_idx] != UNVISITED)
            {
                continue;
            }


            // --------------------------------
            // 현재 Cluster에 포함
            // --------------------------------

            labels[neighbour_idx] =
                cluster_id;

            cluster.points.push_back(
                detected_points[neighbour_idx]
            );


            // --------------------------------
            // 이 Point도 Core인지 검사
            // --------------------------------

            std::vector<size_t> next_neighbours =
                scan_neighbour(
                    detected_points,
                    neighbour_idx
                );


            // --------------------------------
            // Core Point라면
            // neighbour를 추가하여 계속 확장
            // --------------------------------

            if (next_neighbours.size()
                >= min_samples)
            {
                for (size_t next_idx :
                     next_neighbours)
                {
                    neighbours.push_back(
                        next_idx
                    );
                }
            }

            // Core가 아니면 Cluster에는 포함되지만
            // 더 이상 확장하지 않음
            // → Border Point
        }


        // ==============================
        // 현재 Cluster 저장
        // ==============================

        clusters.push_back(
            std::move(cluster)
        );

        cluster_id++;
    }
}