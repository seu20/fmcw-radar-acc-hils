#include "test.hpp"

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
    vector<float> rx1_real =
        loadCSV("../tests/data/matlab_data/rdm_rx1_real.csv");

    vector<float> rx1_imag =
        loadCSV("../tests/data/matlab_data/rdm_rx1_imag.csv");

    vector<float> rx2_real =
        loadCSV("../tests/data/matlab_data/rdm_rx2_real.csv");

    vector<float> rx2_imag =
        loadCSV("../tests/data/matlab_data/rdm_rx2_imag.csv");


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

    float max_cpp_value = 0.0f;
    float max_matlab_value = 0.0f;

    size_t mismatch_count = 0;


    for (size_t range_bin = 0;
         range_bin < 128;
         ++range_bin)
    {
        for (size_t doppler_bin = 0;
             doppler_bin < 64;
             ++doppler_bin)
        {
            size_t csv_idx =
                range_bin * 64 +
                doppler_bin;

            size_t rx1_idx =
                (range_bin * 64 + doppler_bin) * 2;

            size_t rx2_idx =
                rx1_idx + 1;


            float error_rx1_real =
                abs(
                    rdm[rx1_idx].real()
                    - rx1_real[csv_idx]
                );

            float error_rx1_imag =
                abs(
                    rdm[rx1_idx].imag()
                    - rx1_imag[csv_idx]
                );

            float error_rx2_real =
                abs(
                    rdm[rx2_idx].real()
                    - rx2_real[csv_idx]
                );

            float error_rx2_imag =
                abs(
                    rdm[rx2_idx].imag()
                    - rx2_imag[csv_idx]
                );


            // tolerance 초과 개수만 카운트
            if (error_rx1_real > tolerance)
                ++mismatch_count;

            if (error_rx1_imag > tolerance)
                ++mismatch_count;

            if (error_rx2_real > tolerance)
                ++mismatch_count;

            if (error_rx2_imag > tolerance)
                ++mismatch_count;


            // 최대 오차 위치 저장
            if (error_rx1_real > max_error)
            {
                max_error = error_rx1_real;

                max_range_bin = range_bin;
                max_doppler_bin = doppler_bin;

                max_component = "RX1 REAL";

                max_cpp_value =
                    rdm[rx1_idx].real();

                max_matlab_value =
                    rx1_real[csv_idx];
            }

            if (error_rx1_imag > max_error)
            {
                max_error = error_rx1_imag;

                max_range_bin = range_bin;
                max_doppler_bin = doppler_bin;

                max_component = "RX1 IMAG";

                max_cpp_value =
                    rdm[rx1_idx].imag();

                max_matlab_value =
                    rx1_imag[csv_idx];
            }

            if (error_rx2_real > max_error)
            {
                max_error = error_rx2_real;

                max_range_bin = range_bin;
                max_doppler_bin = doppler_bin;

                max_component = "RX2 REAL";

                max_cpp_value =
                    rdm[rx2_idx].real();

                max_matlab_value =
                    rx2_real[csv_idx];
            }

            if (error_rx2_imag > max_error)
            {
                max_error = error_rx2_imag;

                max_range_bin = range_bin;
                max_doppler_bin = doppler_bin;

                max_component = "RX2 IMAG";

                max_cpp_value =
                    rdm[rx2_idx].imag();

                max_matlab_value =
                    rx2_imag[csv_idx];
            }
        }
    }
    // 테스트는 마지막에 단 한 번만 판정
    EXPECT_LE(max_error, tolerance)
        << "\nMismatch count = "
        << mismatch_count
        << "\nMax error      = "
        << max_error
        << "\nLocation       = range_bin "
        << max_range_bin
        << ", doppler_bin "
        << max_doppler_bin
        << "\nComponent      = "
        << max_component
        << "\nC++ value      = "
        << max_cpp_value
        << "\nMATLAB value   = "
        << max_matlab_value;
}
