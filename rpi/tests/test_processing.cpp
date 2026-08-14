#include <gtest/gtest.h>
#include "iq_data.h"
#include "RadarProcessor.hpp"
#include "RadarFrame.hpp"

using namespace std;

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

//
TEST(RadarProcessingTest, ProcessorRuns)
{
    vector<complex<float>> iq_test;
    for (size_t i = 0; i < IQ_TOTAL_SIZE; i += 2)
    {
        iq_test.emplace_back(
            iq_data[i],
            iq_data[i+1]
        );
    }

    RadarFrame frame;
    frame.frame_id = 1;
    frame.iq_data = std::move(iq_test);

    ThreadSafeQueue<RadarFrame> frame_queue;
    RadarProcessor test_Processor(frame_queue);

    test_Processor.init();
    test_Processor.process(frame);
    const auto& rdm = test_Processor.getRDM();

    // 크기 비교
    EXPECT_EQ(
        rdm.size(),
        128 * 64 * 2
    );

    
}

