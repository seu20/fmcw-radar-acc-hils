#include <gtest/gtest.h>

#include "RadarProcessor.hpp"
#include "iq_data.h"

#include <array>
#include <complex>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace std;


// ============================================================
// iq_data.h
//
// float layout:
// [chirp][sample][rx][I/Q]
//
// -> vector<complex<float>>
// ============================================================

vector<complex<float>> loadIQ()
{
    vector<complex<float>> iq;

    iq.reserve(
        NUM_SAMPLES *
        NUM_CHIRPS *
        NUM_RX
    );

    for (size_t i = 0;
         i < IQ_TOTAL_SIZE;
         i += 2)
    {
        iq.emplace_back(
            iq_data[i],
            iq_data[i + 1]
        );
    }

    return iq;
}


// ============================================================
// MATLAB Angle Golden Reference Load
//
// angles.csv:
//
// cluster_id, range_idx, doppler_idx, angle_rad
// ============================================================

vector<array<float, 4>> loadMatlabAngles(
    const string& path)
{
    ifstream file(path);

    if (!file.is_open())
    {
        throw runtime_error(
            "Failed to open: " + path
        );
    }

    vector<array<float, 4>> angles;

    string line;

    while (getline(file, line))
    {
        if (line.empty())
        {
            continue;
        }

        stringstream ss(line);
        string value;

        array<float, 4> row;

        for (size_t i = 0; i < 4; ++i)
        {
            if (!getline(ss, value, ','))
            {
                throw runtime_error(
                    "Invalid angles.csv format"
                );
            }

            row[i] = stof(value);
        }

        angles.push_back(row);
    }

    return angles;
}


// ============================================================
// C++ Angle Result Export
//
// CSV:
//
// cluster_id, range_idx, doppler_idx, angle_rad
// ============================================================

void exportAngles(
    const vector<Peak>& peaks,
    const vector<float>& angles,
    const string& path)
{
    ofstream file(path);

    if (!file.is_open())
    {
        throw runtime_error(
            "Failed to create: " + path
        );
    }

    for (size_t i = 0;
         i < angles.size();
         ++i)
    {
        file
            << i << ","
            << peaks[i].range_idx << ","
            << peaks[i].doppler_idx << ","
            << angles[i]
            << "\n";
    }
}


// ============================================================
// Angle Estimation MATLAB Golden Reference Test
// ============================================================

TEST(AngleEstimationTest, MatchesMatlab)
{
    const string matlabAnglePath =
        "../tests/data/matlab_data/angle/"
        "angles.csv";

    const string rpiAngleDir =
        "../tests/data/rpi_data/angle";

    const string rpiAnglePath =
        rpiAngleDir +
        "/angles.csv";


    // ========================================================
    // 1. MATLAB Golden Reference Load
    // ========================================================

    vector<array<float, 4>> expected =
        loadMatlabAngles(
            matlabAnglePath
        );

    ASSERT_FALSE(
        expected.empty()
    );


    // ========================================================
    // 2. RadarFrame 생성
    // ========================================================

    RadarFrame frame
    {
        1,
        loadIQ()
    };


    // ========================================================
    // 3. RadarProcessor 생성
    // ========================================================

    ThreadSafeQueue<RadarFrame> frame_queue;
    ThreadSafeQueue<TargetFrame> target_queue;

    RadarProcessor processor(
        frame_queue,
        target_queue
    );


    // FFTW buffer / plan 초기화
    processor.init();


    // ========================================================
    // 4. 전체 Radar Processing 실행
    //
    // IQ
    // ↓
    // Range FFT
    // ↓
    // Doppler FFT
    // ↓
    // CFAR
    // ↓
    // DBSCAN
    // ↓
    // Peak
    // ↓
    // Angle
    // ========================================================

    processor.process(
        frame
    );


    const vector<float>& actual_angles =
        processor.getAngles();

    const vector<Peak>& actual_peaks =
        processor.getPeaks();


    // ========================================================
    // 5. 결과 개수 확인
    // ========================================================

    ASSERT_EQ(
        actual_angles.size(),
        expected.size()
    )
        << "Angle count mismatch";


    ASSERT_EQ(
        actual_peaks.size(),
        expected.size()
    )
        << "Peak count mismatch";


    // ========================================================
    // 6. C++ 결과 Export
    // ========================================================

    filesystem::create_directories(
        rpiAngleDir
    );

    exportAngles(
        actual_peaks,
        actual_angles,
        rpiAnglePath
    );


    // ========================================================
    // 7. MATLAB과 비교
    //
    // expected[i]:
    //
    // [0] cluster_id
    // [1] range_idx
    // [2] doppler_idx
    // [3] angle_rad
    // ========================================================

    for (size_t i = 0;
         i < expected.size();
         ++i)
    {
        size_t expected_range_idx =
            static_cast<size_t>(
                expected[i][1]
            );

        size_t expected_doppler_idx =
            static_cast<size_t>(
                expected[i][2]
            );

        float expected_angle =
            expected[i][3];


        // ----------------------------------------------------
        // 동일 Peak 좌표에서 Angle을 계산했는지 확인
        // ----------------------------------------------------

        EXPECT_EQ(
            actual_peaks[i].range_idx,
            expected_range_idx
        )
            << "Range index mismatch"
            << " target=" << i;


        EXPECT_EQ(
            actual_peaks[i].doppler_idx,
            expected_doppler_idx
        )
            << "Doppler index mismatch"
            << " target=" << i;


        // ----------------------------------------------------
        // Angle 비교
        //
        // 단위: radian
        //
        // MATLAB double vs
        // C++ FFTW float 차이를 고려하여
        // tolerance = 1e-3 rad
        // ----------------------------------------------------

        EXPECT_NEAR(
            actual_angles[i],
            expected_angle,
            1e-3f
        )
            << "Angle mismatch"
            << " target=" << i;
    }
}