#pragma once
#include <pthread.h>
#include <atomic>
#include <RadarFrame.hpp>
#include <ThreadSafeQueue.hpp>
#include <fftw3.h>          // FFT 라이브러리
#include "RadarTypes.hpp"
#include "DBSCAN.hpp"

class RadarProcessor {
private:
    ThreadSafeQueue<RadarFrame>& frame_queue_;

    pthread_t thread_id_;

    static void *thread_func(void *arg);

    std::atomic<bool> running_;     // 메인 스레드와 RadarProcessor 스레드에서 동시성 보장 ( 컴파일러 최적화 방지 )

    // Range FFT 결과
    // [chirp][range_bin][rx]
    std::vector<std::complex<float>> range_fft_data_;

    // 최종 Range-Doppler 결과
    // [range_bin][doppler_bin][rx]
    std::vector<std::complex<float>> rdm_;

    // CA-CFAR 를 위한 rx_channels 개수의 rdm의 power 계산 배열
    // [range_bin][doppler_bin]
    std::vector<float> power_;

    // CFAR 을 통한 detection
    std::vector<uint8_t> detections_;

    // CFAR detection 목록
    std::vector<Detection> detected_points_;

    // Peak 검출 결과
    std::vector<Peak> peaks_;

    // Angle 계산 결과
    std::vector<float> angles_;

    // DBSCAN 객체
    DBSCAN dbscan_;


    float angle_;

    uint32_t last_frame_id_;
    bool is_first_frame_;


    // FFT 관련 파라미터 
    fftwf_plan range_plan_ = nullptr;
    fftwf_plan doppler_plan_ = nullptr;

    fftwf_complex* range_in_ = nullptr;
    fftwf_complex* range_out_ = nullptr;

    fftwf_complex* doppler_in_ = nullptr;
    fftwf_complex* doppler_out_ = nullptr;


    static constexpr size_t rx_channels = 2;
    static constexpr size_t samples = 128;
    static constexpr size_t chirps = 64;
    static constexpr size_t range_bins = 128;
    static constexpr size_t doppler_bins = 64;

    // 거리, 속도 resolution
    static constexpr float range_resolution = 0.999f;
    static constexpr float doppler_resolution = 1.521f;

    // CFAR 용 상수
    static constexpr int T_Range = 8;
    static constexpr int T_Doppler = 4;
    static constexpr int G_Range = 3;
    static constexpr int G_Doppler = 3;

    // N Cells
    static constexpr int N_Cells = 
        (
            (2 * (G_Range + T_Range) + 1) * 
            (2 * (G_Doppler + T_Doppler) + 1)
        ) 
        - 
        (
            (2 * G_Range + 1) * 
            (2 * G_Doppler + 1)
        );

    // pfa (Probabilty False Alarm) = 1e-6
    // pfa 를 통해 threshold multiplier 값을 도출 -> 14.14
    static constexpr float alpha = 14.14f;

public:

    RadarProcessor(ThreadSafeQueue<RadarFrame>& frame_queue):
        frame_queue_(frame_queue),
        running_(false),
        last_frame_id_(0),
        is_first_frame_(true),
        angle_(0.0)
    {}

    ~RadarProcessor();

    const std::vector<std::complex<float>>& getRDM() const
    {
        return rdm_;
    }

    const std::vector<uint8_t>& getDetections() const
    {
        return detections_;
    }

    const std::vector<Detection>& getDetectedPoints() const
    {
        return detected_points_;
    }

    void init();
    
    void process(const RadarFrame& frame);

    void start();

    bool run();

    void stop();

    void Range_FFT(const std::vector<std::complex<float>>& iq_data);

    void Doppler_FFT();

    void PowerCalculation();

    void CFAR();

    void PeakDetection(const std::vector<Cluster>& clusters);

    void AngleEstimation();

    void TargetConversion();
};

