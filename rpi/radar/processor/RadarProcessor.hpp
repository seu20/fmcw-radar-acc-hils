#pragma once
#include <pthread.h>
#include <atomic>
#include <RadarFrame.hpp>
#include <ThreadSafeQueue.hpp>
#include <fftw3.h>          // FFT 라이브러리

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

    int last_frame_id_;

    // Hamming Window ( Spectral Lobe를 줄이기 위한 필터 )
    std::vector<float> range_hamming_window;
    std::vector<float> doppler_hamming_window;
    
    // FFT 관련 파라미터 
    fftwf_plan range_plan_ = nullptr;
    fftwf_plan doppler_plan_ = nullptr;

    fftwf_complex* range_in_ = nullptr;
    fftwf_complex* range_out_ = nullptr;

    fftwf_complex* doppler_in_ = nullptr;
    fftwf_complex* doppler_out_ = nullptr;

    void init();

    static constexpr size_t rx_channels = 2;
    static constexpr size_t samples = 128;
    static constexpr size_t chirps = 64;
    static constexpr size_t range_bins = 128;
    static constexpr size_t doppler_bins = 64;

public:

    RadarProcessor(ThreadSafeQueue<RadarFrame>& frame_queue):
        frame_queue_(frame_queue),
        running_(false),
        last_frame_id_(-1)
    {}

    ~RadarProcessor();

    void start();

    bool run();

    void stop();

    void Range_FFT(const std::vector<std::complex<float>>& iq_data);

    void Doppler_FFT();
};

