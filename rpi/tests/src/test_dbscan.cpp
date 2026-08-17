#include <gtest/gtest.h>

#include "DBSCAN.hpp"

#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <stdexcept>

using namespace std;


// ============================================================
// MATLAB CFAR detected_points.csv Load
//
// CSV:
// range_idx, doppler_idx, power
// ============================================================

vector<Detection> loadDetectedPoints(const string& path)
{
    ifstream file(path);

    if (!file.is_open())
    {
        throw runtime_error(
            "Failed to open: " + path
        );
    }

    vector<Detection> detected_points;

    string line;

    while (getline(file, line))
    {
        if (line.empty())
        {
            continue;
        }

        stringstream ss(line);
        string value;

        vector<float> row;

        while (getline(ss, value, ','))
        {
            row.push_back(
                stof(value)
            );
        }

        if (row.size() != 3)
        {
            throw runtime_error(
                "Invalid detected_points.csv format"
            );
        }

        detected_points.push_back(
            {
                static_cast<size_t>(row[0]),
                static_cast<size_t>(row[1]),
                row[2]
            }
        );
    }

    return detected_points;
}


// ============================================================
// MATLAB dbscan_clusters.csv Load
//
// CSV:
// cluster_id, range_idx, doppler_idx, power
// ============================================================

vector<Cluster> loadMatlabClusters(const string& path)
{
    ifstream file(path);

    if (!file.is_open())
    {
        throw runtime_error(
            "Failed to open: " + path
        );
    }

    vector<Cluster> clusters;

    string line;

    while (getline(file, line))
    {
        if (line.empty())
        {
            continue;
        }

        stringstream ss(line);
        string value;

        vector<float> row;

        while (getline(ss, value, ','))
        {
            row.push_back(
                stof(value)
            );
        }

        if (row.size() != 4)
        {
            throw runtime_error(
                "Invalid dbscan_clusters.csv format"
            );
        }

        int cluster_id =
            static_cast<int>(row[0]);

        Detection detection
        {
            static_cast<size_t>(row[1]),
            static_cast<size_t>(row[2]),
            row[3]
        };

        // CSV는 cluster_id 순서대로 저장되어 있다는 전제
        if (clusters.empty() ||
            clusters.back().cluster_id != cluster_id)
        {
            Cluster cluster;

            cluster.cluster_id =
                cluster_id;

            clusters.push_back(
                cluster
            );
        }

        clusters.back().points.push_back(
            detection
        );
    }

    return clusters;
}


// ============================================================
// C++ DBSCAN 결과 CSV Export
//
// CSV:
// cluster_id, range_idx, doppler_idx, power
//
// 저장:
// tests/data/rpi_data/dbscan/dbscan_clusters.csv
// ============================================================

void exportClusters(
    const vector<Cluster>& clusters,
    const string& path)
{
    ofstream file(path);

    if (!file.is_open())
    {
        throw runtime_error(
            "Failed to create: " + path
        );
    }

    for (const auto& cluster : clusters)
    {
        for (const auto& point : cluster.points)
        {
            file
                << cluster.cluster_id << ","
                << point.range_idx << ","
                << point.doppler_idx << ","
                << point.power
                << "\n";
        }
    }
}


// ============================================================
// DBSCAN MATLAB Golden Reference Test
// ============================================================

TEST(DBSCANTest, MatchesMatlab)
{
    // 현재 실행 위치:
    //
    // rpi/build/
    //
    // 따라서 ../tests/... -> rpi/tests/...

    const string matlabDetectionPath =
        "../tests/data/matlab_data/cfar/"
        "detected_points.csv";

    const string matlabDBSCANPath =
        "../tests/data/matlab_data/dbscan/"
        "dbscan_clusters.csv";

    const string rpiDBSCANPath =
        "../tests/data/rpi_data/dbscan/"
        "dbscan_clusters.csv";


    // ========================================================
    // 1. MATLAB CFAR Detection Load
    // ========================================================

    vector<Detection> detected_points =
        loadDetectedPoints(
            matlabDetectionPath
        );

    ASSERT_FALSE(
        detected_points.empty()
    );


    // ========================================================
    // 2. C++ DBSCAN 실행
    // ========================================================

    DBSCAN dbscan;

    dbscan.scan(
        detected_points
    );

    const vector<Cluster>& actual =
        dbscan.getClusters();


    // ========================================================
    // 3. C++ DBSCAN 결과 CSV Export
    // ========================================================

    exportClusters(
        actual,
        rpiDBSCANPath
    );


    // ========================================================
    // 4. MATLAB Golden Reference Load
    // ========================================================

    vector<Cluster> expected =
        loadMatlabClusters(
            matlabDBSCANPath
        );


    // ========================================================
    // 5. Cluster 개수 비교
    // ========================================================

    ASSERT_EQ(
        actual.size(),
        expected.size()
    )
        << "DBSCAN cluster count mismatch";


    // ========================================================
    // 6. Cluster별 비교
    // ========================================================

    for (size_t cluster_idx = 0;
         cluster_idx < expected.size();
         ++cluster_idx)
    {
        const Cluster& actual_cluster =
            actual[cluster_idx];

        const Cluster& expected_cluster =
            expected[cluster_idx];


        // Cluster ID
        EXPECT_EQ(
            actual_cluster.cluster_id,
            expected_cluster.cluster_id
        )
            << "Cluster ID mismatch"
            << " cluster_idx=" << cluster_idx;


        // Cluster 내부 Detection 개수
        ASSERT_EQ(
            actual_cluster.points.size(),
            expected_cluster.points.size()
        )
            << "Cluster point count mismatch"
            << " cluster_id="
            << expected_cluster.cluster_id;


        // Detection 하나씩 비교
        for (size_t point_idx = 0;
             point_idx < expected_cluster.points.size();
             ++point_idx)
        {
            const Detection& actual_point =
                actual_cluster.points[point_idx];

            const Detection& expected_point =
                expected_cluster.points[point_idx];


            EXPECT_EQ(
                actual_point.range_idx,
                expected_point.range_idx
            )
                << "Range index mismatch"
                << " cluster_id="
                << expected_cluster.cluster_id
                << " point_idx="
                << point_idx;


            EXPECT_EQ(
                actual_point.doppler_idx,
                expected_point.doppler_idx
            )
                << "Doppler index mismatch"
                << " cluster_id="
                << expected_cluster.cluster_id
                << " point_idx="
                << point_idx;


            EXPECT_NEAR(
                actual_point.power,
                expected_point.power,
                1e-3f
            )
                << "Power mismatch"
                << " cluster_id="
                << expected_cluster.cluster_id
                << " point_idx="
                << point_idx;
        }
    }
}