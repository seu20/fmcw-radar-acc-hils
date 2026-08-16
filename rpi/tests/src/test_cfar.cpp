#include "test.hpp"


TEST(RadarProcessingTest, CFARSizeIsCorrect)
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
    RadarProcessor processor(frame_queue);

    processor.init();
    processor.process(frame);

    const auto& detections =
        processor.getDetections();

    EXPECT_EQ(
        detections.size(),
        128 * 64
    );
}


TEST(RadarProcessingTest, CFARMatchesMatlab)
{
    // MATLAB Golden Reference
    vector<float> matlab_cfar =
        loadCSV(
            "../tests/data/matlab_data/cfar_map.csv"
        );

    ASSERT_EQ(
        matlab_cfar.size(),
        128 * 64
    );


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


    const auto& cpp_cfar =
        processor.getDetections();

    ASSERT_EQ(
        cpp_cfar.size(),
        matlab_cfar.size()
    );


    size_t mismatch_count = 0;

    size_t first_mismatch_range = 0;
    size_t first_mismatch_doppler = 0;

    bool first_mismatch_cpp = false;
    bool first_mismatch_matlab = false;

    bool found_first_mismatch = false;


    for (size_t range_bin = 0;
         range_bin < 128;
         ++range_bin)
    {
        for (size_t doppler_bin = 0;
             doppler_bin < 64;
             ++doppler_bin)
        {
            size_t idx =
                range_bin * 64 +
                doppler_bin;

            bool cpp_value =
                cpp_cfar[idx];

            bool matlab_value =
                matlab_cfar[idx] != 0.0f;


            if (cpp_value != matlab_value)
            {
                ++mismatch_count;

                if (!found_first_mismatch)
                {
                    found_first_mismatch = true;

                    first_mismatch_range =
                        range_bin;

                    first_mismatch_doppler =
                        doppler_bin;

                    first_mismatch_cpp =
                        cpp_value;

                    first_mismatch_matlab =
                        matlab_value;
                }
            }
        }
    }


    EXPECT_EQ(mismatch_count, 0)
        << "\nMismatch count = "
        << mismatch_count

        << "\nFirst mismatch = range_bin "
        << first_mismatch_range

        << ", doppler_bin "
        << first_mismatch_doppler

        << "\nC++ value      = "
        << first_mismatch_cpp

        << "\nMATLAB value   = "
        << first_mismatch_matlab;
}

