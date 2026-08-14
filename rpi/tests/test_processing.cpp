#include <gtest/gtest.h>
#include "iq_data.h"
#include "RadarProcessor.hpp"
#include "RadarFrame.hpp"
#include <fstream>
#include <sstream>

using namespace std;

vector<float> loadCSV(const string& filename)
{
    vector<float> data;

    ifstream file(filename);

    cout << "open: " << filename
         << " = " << file.is_open() << endl;

    string line;

    while (getline(file, line))
    {
        stringstream ss(line);
        string value;

        while (getline(ss, value, ','))
        {
            data.push_back(stof(value));
        }
    }

    return data;
}

void saveRDMToCSV(
    const vector<complex<float>>& rdm,
    const string& filename_real,
    const string& filename_imag,
    size_t rx)
{
    ofstream real_file(filename_real);
    ofstream imag_file(filename_imag);

    for (size_t range_bin = 0; range_bin < 128; ++range_bin)
    {
        for (size_t doppler_bin = 0; doppler_bin < 64; ++doppler_bin)
        {
            size_t idx =
                (range_bin * 64 + doppler_bin) * 2 + rx;

            real_file << rdm[idx].real();
            imag_file << rdm[idx].imag();

            if (doppler_bin != 63)
            {
                real_file << ",";
                imag_file << ",";
            }
        }

        real_file << "\n";
        imag_file << "\n";
    }
}

// IQ 크기 테스트
TEST(RadarProcessingTest, IQSizeisCorrect)
{
    vector<complex<float>> iq_test;
    for (size_t i = 0; i < IQ_TOTAL_SIZE; i += 2)
    {
        iq_test.emplace_back(
            iq_data[i],
            iq_data[i+1]
        );
    }
    EXPECT_EQ(iq_test.size(), 128 * 64 * 2);
}

TEST(RadarProcessingTest, ExportCppRDMToCSV)
{
    vector<complex<float>> iq_test;

    for (size_t i = 0; i < IQ_TOTAL_SIZE; i += 2)
    {
        iq_test.emplace_back(
            iq_data[i],
            iq_data[i + 1]
        );
    }

    RadarFrame frame;
    frame.frame_id = 1;
    frame.iq_data = move(iq_test);

    ThreadSafeQueue<RadarFrame> frame_queue;
    RadarProcessor test_Processor(frame_queue);

    test_Processor.init();
    test_Processor.process(frame);
    const auto& rdm = test_Processor.getRDM();

    saveRDMToCSV(
        rdm,
        "../tests/data/rpi_data/rpi_rdm_rx1_real.csv",
        "../tests/data/rpi_data/rpi_rdm_rx1_imag.csv",
        0
    );

    saveRDMToCSV(
        rdm,
        "../tests/data/rpi_data/rpi_rdm_rx2_real.csv",
        "../tests/data/rpi_data/rpi_rdm_rx2_imag.csv",
        1
    );
}
TEST(RadarProcessingTest, RDMMatchesMatlab)
{
    // MATLAB Golden Reference
    vector<float> rx1_real = loadCSV("../tests/data/matlab_data/rdm_rx1_real.csv");
    vector<float> rx1_imag = loadCSV("../tests/data/matlab_data/rdm_rx1_imag.csv");
    vector<float> rx2_real = loadCSV("../tests/data/matlab_data/rdm_rx2_real.csv");
    vector<float> rx2_imag = loadCSV("../tests/data/matlab_data/rdm_rx2_imag.csv");

    ASSERT_EQ(rx1_real.size(), 128 * 64);
    ASSERT_EQ(rx1_imag.size(), 128 * 64);
    ASSERT_EQ(rx2_real.size(), 128 * 64);
    ASSERT_EQ(rx2_imag.size(), 128 * 64);

    // IQ 생성
    vector<complex<float>> iq_test;

    for (size_t i = 0; i < IQ_TOTAL_SIZE; i += 2)
    {
        iq_test.emplace_back(
            iq_data[i],
            iq_data[i + 1]
        );
    }

    RadarFrame frame;
    frame.frame_id = 1;
    frame.iq_data = move(iq_test);

    ThreadSafeQueue<RadarFrame> frame_queue;
    RadarProcessor processor(frame_queue);

    processor.init();
    processor.process(frame);

    const auto& rdm = processor.getRDM();

    ASSERT_EQ(rdm.size(), 128 * 64 * 2);

    constexpr float tolerance = 1e-3f;

    float max_error = 0.0f;
    size_t max_range_bin = 0;
    size_t max_doppler_bin = 0;
    string max_component;

    for (size_t range_bin = 0; range_bin < 128; ++range_bin)
    {
        for (size_t doppler_bin = 0; doppler_bin < 64; ++doppler_bin)
        {
            size_t csv_idx =
                range_bin * 64 + doppler_bin;

            size_t rx1_idx =
                (range_bin * 64 + doppler_bin) * 2;

            size_t rx2_idx =
                rx1_idx + 1;

            float error_rx1_real =
                abs(rdm[rx1_idx].real() - rx1_real[csv_idx]);

            float error_rx1_imag =
                abs(rdm[rx1_idx].imag() - rx1_imag[csv_idx]);

            float error_rx2_real =
                abs(rdm[rx2_idx].real() - rx2_real[csv_idx]);

            float error_rx2_imag =
                abs(rdm[rx2_idx].imag() - rx2_imag[csv_idx]);

            if (error_rx1_real > max_error)
            {
                max_error = error_rx1_real;
                max_range_bin = range_bin;
                max_doppler_bin = doppler_bin;
                max_component = "RX1 REAL";
            }

            if (error_rx1_imag > max_error)
            {
                max_error = error_rx1_imag;
                max_range_bin = range_bin;
                max_doppler_bin = doppler_bin;
                max_component = "RX1 IMAG";
            }

            if (error_rx2_real > max_error)
            {
                max_error = error_rx2_real;
                max_range_bin = range_bin;
                max_doppler_bin = doppler_bin;
                max_component = "RX2 REAL";
            }

            if (error_rx2_imag > max_error)
            {
                max_error = error_rx2_imag;
                max_range_bin = range_bin;
                max_doppler_bin = doppler_bin;
                max_component = "RX2 IMAG";
            }

            EXPECT_NEAR(
                rdm[rx1_idx].real(),
                rx1_real[csv_idx],
                tolerance
            );

            EXPECT_NEAR(
                rdm[rx1_idx].imag(),
                rx1_imag[csv_idx],
                tolerance
            );

            EXPECT_NEAR(
                rdm[rx2_idx].real(),
                rx2_real[csv_idx],
                tolerance
            );

            EXPECT_NEAR(
                rdm[rx2_idx].imag(),
                rx2_imag[csv_idx],
                tolerance
            );
        }
    }

    cout << "Max RDM error = " << max_error << endl;
    cout << "Location      = range_bin "
         << max_range_bin
         << ", doppler_bin "
         << max_doppler_bin
         << endl;

    cout << "Component     = "
         << max_component
         << endl;
}