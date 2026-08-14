#include "RadarProcessor.hpp"
#include <stdexcept>      // std::runtime_error
#include <cstring>        // std::strerror()
#include <iostream>

void *RadarProcessor::thread_func(void* arg)
{
    RadarProcessor* self = static_cast<RadarProcessor*>(arg);
    try {
        self->run();
    }
    catch (const std::exception& e)
    {
        std::cerr << "[RadarProcessor] " << e.what() << std::endl;
    }
    return nullptr;
}

void RadarProcessor::init()
{
    // Range FFT 벡터 초기화
    range_fft_data_.resize(
        chirps * samples * rx_channels
    );
    // Range Doppler Map 사이즈 초기화
    rdm_.resize(
        samples * doppler_bins * rx_channels
    );

    // Range FFT : 128 samples
    range_in_  = fftwf_alloc_complex(samples);     
    range_out_ = fftwf_alloc_complex(samples);      

    if (range_in_ == nullptr || range_out_ == nullptr)
    {
        throw std::runtime_error{
            "Range Bin not created!"
        };
    }

    // Doppler FFT : 64 chrips
    doppler_in_  = fftwf_alloc_complex(chirps);
    doppler_out_ = fftwf_alloc_complex(chirps);

    if (doppler_in_ == nullptr || doppler_out_ == nullptr)
    {
        throw std::runtime_error(
            "Doppler Bin not created!"
        );
    }
    // Range FFT fft 설정
    range_plan_ = fftwf_plan_dft_1d(
        samples,
        range_in_,
        range_out_,
        FFTW_FORWARD,
        FFTW_ESTIMATE
    );

    // Doppler FFT fft 설정 저장
    doppler_plan_ = fftwf_plan_dft_1d(
        chirps,
        doppler_in_,
        doppler_out_,
        FFTW_FORWARD,
        FFTW_ESTIMATE
    );

    if (range_plan_ == nullptr)
    {
        throw std::runtime_error("Range FFT plan not created!");
    }

    if (doppler_plan_ == nullptr)
    {
        throw std::runtime_error("Doppler FFT plan not created!");
    }
}

RadarProcessor::~RadarProcessor()
{
    if (range_plan_ != nullptr)
        fftwf_destroy_plan(range_plan_);

    if (doppler_plan_ != nullptr)
        fftwf_destroy_plan(doppler_plan_);

    if (range_in_ != nullptr)
        fftwf_free(range_in_);

    if (range_out_ != nullptr)
        fftwf_free(range_out_);

    if (doppler_in_ != nullptr)
        fftwf_free(doppler_in_);

    if (doppler_out_ != nullptr)
        fftwf_free(doppler_out_);
}

void RadarProcessor::start()
{
    running_ = true;

    int res = pthread_create(
            &thread_id_,
            nullptr,
            thread_func,
            this
    );
    if (res != 0)
    {
        running_ = false;

        throw std::runtime_error(
            std::string("Radar Processor Thread not Created!: ") + std::strerror(res)
        );
    }
}

 
bool RadarProcessor::run()
{
    init();

    // RadarFrame pop 하고 읽음
    while(running_)
    {
        RadarFrame frame;
        
        try{
            frame = frame_queue_.pop();
        }
        catch(const std::exception &e)
        {
            std::cerr << e.what() << std::endl;
            break;
        }

        // FrameID 가 순서대로 오지 않았다면 이번 프레임 건너뛰기 ( 최신 데이터가 중요하기 때문에 )
        if (frame.frame_id <= last_frame_id_)   continue;

        Range_FFT(frame.iq_data);

        Doppler_FFT();

        last_frame_id_ = frame.frame_id;
    }
    return true;
}

void RadarProcessor::stop()
{
    running_ = false;
    frame_queue_.close();
    pthread_join(thread_id_, nullptr);
}

void RadarProcessor::Range_FFT(const std::vector<std::complex<float>>& iq_data)
{
    // RANGE FFT : chirp, sample 마다 iq_data 추출
    for (size_t rx = 0; rx < rx_channels; ++rx)
    {
        for (size_t chirp = 0; chirp < chirps; ++chirp)
        {
            for (size_t sample = 0; sample < samples; ++sample)
            {
                size_t idx = (chirp * samples + sample) * rx_channels + rx;

                auto iq = iq_data[idx];

                range_in_[sample][0] = iq.real();
                range_in_[sample][1] = iq.imag();
            }

            fftwf_execute(range_plan_);

            // Range FFT 결과 배열에 저장
            for (size_t range_bin = 0; range_bin < samples; ++range_bin)
            {
                size_t idx = (chirp * samples + range_bin) * rx_channels + rx;

                // [chirp][range_bin][rx]
                range_fft_data_[idx] = 
                std::complex<float>(
                    range_out_[range_bin][0],
                    range_out_[range_bin][1]
                );
            }
        }
    }
}

void RadarProcessor::Doppler_FFT()
{
    // 최종 Range-Doppler 결과
    // [range_bin][doppler_bin][rx]
    for (size_t rx = 0; rx < rx_channels ; ++rx)
    {
        for(size_t range_bin = 0 ; range_bin < samples ; ++range_bin )
        {
            for (size_t chirp = 0; chirp < chirps; ++chirp)
            {
                size_t idx = (chirp * samples + range_bin) * rx_channels + rx;

                doppler_in_[chirp][0] = range_fft_data_[idx].real();
                doppler_in_[chirp][1] = range_fft_data_[idx].imag();
            }

            fftwf_execute(doppler_plan_);

            for (size_t doppler_bin = 0; doppler_bin < doppler_bins ; ++doppler_bin)
            {
                size_t idx = (range_bin * doppler_bins + doppler_bin) * rx_channels + rx;

                rdm_[idx] = std::complex<float>(
                    doppler_out_[doppler_bin][0],
                    doppler_out_[doppler_bin][1]
                );
            }
        }
    }
}

