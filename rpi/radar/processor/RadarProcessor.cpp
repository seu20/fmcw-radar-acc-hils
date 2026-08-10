#include "RadarProcessor.hpp"

void *RadarProcessor::thread_func(void* arg)
{
    RadarProcessor* self = static_cast<RadarProcessor*>(arg);
    self->run();
    return nullptr;
}

void RadarProcessor::init()
{
    // 검사해야함 !!

    // Range FFT : 128 samples
    range_in_  = fftwf_alloc_complex(128);     
    range_out_ = fftwf_alloc_complex(128);      


    // Doppler FFT : 64 chrips
    doppler_in_  = fftwf_alloc_complex(64);
    doppler_out_ = fftwf_alloc_complex(64);

    // Range FFT fft 설정
    range_plan_ = fftwf_plan_dft_1d(
        128,
        range_in_,
        range_out_,
        FFTW_FORWARD,
        FFTW_ESTIMATE
    );

    // Doppler FFT fft 설정 저장
    doppler_plan_ = fftwf_plan_dft_1d(
        64,
        doppler_in_,
        doppler_out_,
        FFTW_FORWARD,
        FFTW_ESTIMATE
    );
}

void RadarProcessor::start()
{
    running_ = true;

    pthread_create(
            &thread_id_,
            nullptr,
            thread_func,
            this
    );
}

 
bool RadarProcessor::run()
{
    // 검사해야함 !! FFT 잘 만들어졌는지
    init();

    // RadarFrame pop 하고 읽음
    while(running_)
    {
        // 블로킹 가능성!
        RadarFrame frame = frame_queue_.pop();

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
                size_t idx = (chirp * samples + range_bin) * 2 + rx;

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
                size_t idx = (chirp * samples + range_bin) * 2 + rx;

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

// TODO : range_bin_, doppler_bin_ 실패 검사
// TODO : range_fft_data 사이즈 설정, rdm_ 사이즈 설정
// TODO : pthread_create 반환값 확인
// TODO : pop()에서 블로킹 가능성
// TODO : 소멸자에서 FFTW 포인터 free
