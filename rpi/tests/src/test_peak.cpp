#include <gtest/gtest.h>

#include "RadarProcessor.hpp"

#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <stdexcept>

using namespace std;


// ============================================================
// MATLAB DBSCAN 결과 Load
//
// CSV:
// cluster_id, range_idx, doppler_idx, power
// ============================================================

vector<Cluster> loadClusters(const string& path)
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
// MATLAB Peak 결과 Load
//
// CSV:
// cluster_id, range_idx, doppler_idx, power
//
// C++ Peak에는 cluster_id가 없으므로
// 비교에는 range_idx, doppler_idx, power만 사용
// ============================================================

vector<Peak> loadMatlabPeaks(const string& path)
{
    ifstream file(path);

    if (!file.is_open())
    {
        throw runtime_error(
            "Failed to open: " + path
        );
    }

    vector<Peak> peaks;

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
                "Invalid peaks.csv format"
            );
        }

        peaks.push_back(
            {
                static_cast<size_t>(row[1]),
                static_cast<size_t>(row[2]),
                row[3]
            }
        );
    }

    return peaks;
}


// ============================================================
// C++ Peak 결과 Export
//
// CSV:
// range_idx, doppler_idx, power
//
// 저장:
// tests/data/rpi_data/peak/peaks.csv
// ============================================================

void exportPeaks(
    const vector<Peak>& peaks,
    const string& path)
{
    ofstream file(path);

    if (!file.is_open())
    {
        throw runtime_error(
            "Failed to create: " + path
        );
    }

    for (const auto& peak : peaks)
    {
        file
            << peak.range_idx << ","
            << peak.doppler_idx << ","
            << peak.power
            << "\n";
    }
}


// ============================================================
// Peak MATLAB Golden Reference Test
// ============================================================

TEST(PeakDetectionTest, MatchesMatlab)
{
    // build/ 기준

    const string matlabDBSCANPath =
        "../tests/data/matlab_data/dbscan/"
        "dbscan_clusters.csv";

    const string matlabPeakPath =
        "../tests/data/matlab_data/peak/"
        "peaks.csv";

    const string rpiPeakPath =
        "../tests/data/rpi_data/peak/"
        "peaks.csv";


    // ========================================================
    // 1. MATLAB DBSCAN Cluster Load
    // ========================================================

    vector<Cluster> clusters =
        loadClusters(
            matlabDBSCANPath
        );

    ASSERT_FALSE(
        clusters.empty()
    );


    // ========================================================
    // 2. RadarProcessor 생성
    //
    // PeakDetection만 호출하므로
    // thread / FFT init은 실행하지 않음
    // ========================================================

    ThreadSafeQueue<RadarFrame> frame_queue;
    ThreadSafeQueue<TargetFrame> target_queue;

    RadarProcessor processor(
        frame_queue,
        target_queue
    );


    // ========================================================
    // 3. C++ PeakDetection 실행
    // ========================================================

    processor.PeakDetection(
        clusters
    );

    const vector<Peak>& actual =
        processor.getPeaks();


    // ========================================================
    // 4. C++ 결과 CSV Export
    // ========================================================

    exportPeaks(
        actual,
        rpiPeakPath
    );


    // ========================================================
    // 5. MATLAB Golden Reference Load
    // ========================================================

    vector<Peak> expected =
        loadMatlabPeaks(
            matlabPeakPath
        );


    // ========================================================
    // 6. Peak 개수 비교
    //
    // Cluster 하나당 Peak 하나
    // ========================================================

    ASSERT_EQ(
        actual.size(),
        expected.size()
    )
        << "Peak count mismatch";


    ASSERT_EQ(
        actual.size(),
        clusters.size()
    )
        << "Each cluster must produce exactly one peak";


    // ========================================================
    // 7. Peak별 비교
    // ========================================================

    for (size_t i = 0;
         i < expected.size();
         ++i)
    {
        EXPECT_EQ(
            actual[i].range_idx,
            expected[i].range_idx
        )
            << "Range index mismatch"
            << " peak=" << i;


        EXPECT_EQ(
            actual[i].doppler_idx,
            expected[i].doppler_idx
        )
            << "Doppler index mismatch"
            << " peak=" << i;


        EXPECT_NEAR(
            actual[i].power,
            expected[i].power,
            1e-3f
        )
            << "Power mismatch"
            << " peak=" << i;
    }
}