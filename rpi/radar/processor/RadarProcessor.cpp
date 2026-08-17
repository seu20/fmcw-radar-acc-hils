#include "RadarProcessor.hpp"
#include "cheb_window.h"
#include <stdexcept>      // std::runtime_error
#include <cstring>        // std::strerror()
#include <iostream>
#include <cmath>
#include <algorithm>

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
    // Power 벡터 사이즈 초기화
    power_.resize(
        range_bins * doppler_bins
    );
    // Detections 벡터 초기화
    detections_.assign(
        range_bins * doppler_bins,
        0
    );

    // Range FFT : 128 samples
    range_in_  = fftwf_alloc_complex(samples);     
    range_out_ = fftwf_alloc_complex(samples);      
    // Doppler FFT : 64 chrips
    doppler_in_  = fftwf_alloc_complex(chirps);
    doppler_out_ = fftwf_alloc_complex(chirps);

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

    // 포인터 검사
    if (range_in_ == nullptr || range_out_ == nullptr)
    {
        throw std::runtime_error{
            "Range Bin not created!"
        };
    }

    if (doppler_in_ == nullptr || doppler_out_ == nullptr)
    {
        throw std::runtime_error(
            "Doppler Bin not created!"
        );
    }

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

void RadarProcessor::process(const RadarFrame& frame)
{
    if (!is_first_frame_ && frame.frame_id <= last_frame_id_)
        return;
    
    is_first_frame_ = false;

    // 1. Range_FFT
    Range_FFT(frame.iq_data);
    // 2. Doppler_FFT
    Doppler_FFT();
    // 3. CFAR을 이용한 threshold 이상의 real target 검출
    CFAR();
    // 4. DBSCAN을 활용한 clustering
    dbscan_.scan(detected_points_);
    // 5. Peak Detection - 각 cluster 중 power 가 가장 큰 peak 검출
    PeakDetection(dbscan_.getClusters());
    // 6. Angle 계산
    AngleEstimation();
    // 7. TargetFrame queue에 push
    target_queue_.push(
        {
            frame.frame_id,
            std::move(TargetConversion())
        }
    );

    last_frame_id_ = frame.frame_id;
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

        process(frame);
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

                // Hamming Window 적용
                range_in_[sample][0] = iq.real() * range_cheb_window[sample];
                range_in_[sample][1] = iq.imag() * range_cheb_window[sample];
            }
            // range_fft
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

                doppler_in_[chirp][0] = range_fft_data_[idx].real() * doppler_cheb_window[chirp];
                doppler_in_[chirp][1] = range_fft_data_[idx].imag() * doppler_cheb_window[chirp];
            }

            fftwf_execute(doppler_plan_);

            for (size_t doppler_bin = 0; doppler_bin < doppler_bins ; ++doppler_bin)
            {
                size_t idx = (range_bin * doppler_bins + doppler_bin) * rx_channels + rx;

                size_t shifted_bin = (doppler_bin + doppler_bins / 2) % doppler_bins;

                rdm_[idx] = std::complex<float>(
                    doppler_out_[shifted_bin][0],
                    doppler_out_[shifted_bin][1]
                );
            }
        }
    }
}

// CA-CFAR 로직을 위한 rdm의 power 계산 함수
void RadarProcessor::PowerCalculation()
{
    // rdm : [range][doppler][rx]
    // power : [range][doppler]
    for (size_t range_bin = 0; range_bin < range_bins; ++range_bin)
    {
        for (size_t doppler_bin = 0; doppler_bin < doppler_bins; ++doppler_bin)
        {
            int power_idx = (doppler_bin + doppler_bins * range_bin);
            float power_sum = 0;
            for(size_t rx = 0; rx < rx_channels; ++rx)
            {
                int rdm_idx = (doppler_bin + doppler_bins * range_bin) * rx_channels + rx;
                power_sum += std::norm(rdm_[rdm_idx]);
            }
            power_[power_idx] = power_sum;
        }
    }
}

void RadarProcessor::CFAR()
{
    // power 계산
    PowerCalculation();
    
    // detections 벡터 초기화
    std::fill(
        detections_.begin(),
        detections_.end(),
        0
    );

    // detected_points_ 초기화
    detected_points_.clear();
    
    // power : [range][doppler]
    for (size_t range_bin = G_Range + T_Range; range_bin < (range_bins - (G_Range + T_Range)); ++range_bin)
    {
        for (size_t doppler_bin = G_Doppler + T_Doppler; doppler_bin < (doppler_bins - (G_Doppler + T_Doppler)); ++doppler_bin )
        {
            // Training 셀들의 총합 파워
            float training_sum = 0;
            
            //Cut Cell
            size_t cut = doppler_bins * range_bin + doppler_bin;

            // Training Cell 의 평균 합산 ( Guard Cells & Cut Cell 제외 )
            for (
                size_t range_cell = range_bin - (G_Range + T_Range); 
                range_cell <= range_bin + G_Range + T_Range; 
                ++range_cell)
            {
                for (
                    size_t doppler_cell = doppler_bin - (G_Doppler + T_Doppler); 
                    doppler_cell <= doppler_bin + (G_Doppler + T_Doppler);
                    ++doppler_cell
                    )
                {
                    // Guard Cell 구역이나, Cut Cell 이면 continue
                    // T 구역이면 합산
                    size_t training_idx = doppler_bins * range_cell + doppler_cell;     // T 좌표
                    if ( 
                        range_cell >=  range_bin - G_Range && 
                        range_cell <= range_bin + G_Range &&
                        doppler_cell >= doppler_bin - G_Doppler &&
                        doppler_cell <= doppler_bin + G_Doppler
                    )
                    {
                        continue;
                    }else{
                        training_sum += power_[training_idx];
                    }
                }
            }
            float training_avg = training_sum / N_Cells;
            if ( power_[cut] >= training_avg * alpha)
            {
                detections_[cut] = 1;
                detected_points_.push_back(
                    {
                        range_bin,
                        doppler_bin,
                        power_[cut]
                    }
                );
            }
        }
    }
}

void RadarProcessor::PeakDetection(const std::vector<Cluster>& clusters)
{
    peaks_.clear();
    for (const auto& cluster : clusters)
    {
        auto max_it = std::max_element(
            cluster.points.begin(),
            cluster.points.end(),
            [](const Detection& A, const Detection& B)
            {
                return A.power < B.power;
            }
        );
        peaks_.push_back(
            {
                (*max_it).range_idx,
                (*max_it).doppler_idx,
                (*max_it).power
            }
        );
    }
}

// Range Doppler Map 의 RX0, RX1 성분을 이용해 각도 계산
void RadarProcessor::AngleEstimation()
{
    // 각도 결과 초기화
    angles_.clear();

    // 각 피크의 좌표를 rx1, rx2 rdm에서 비교
    for( const auto& peak : peaks_ )
    {
        size_t range_idx = peak.range_idx;
        size_t doppler_idx = peak.doppler_idx;

        size_t idx_rx1 = (range_idx * doppler_bins + doppler_idx) * rx_channels;
        size_t idx_rx2 = idx_rx1 + 1;

        std::complex<float>& rx1_iq = rdm_[idx_rx1];
        std::complex<float>& rx2_iq = rdm_[idx_rx2];

        float radian = std::arg(rx2_iq * std::conj(rx1_iq));

        float angle = std::asin(radian / M_PI);

        angles_.push_back(angle);
    }
}

// Peak 와 Angle 벡터를 통해 한 frame에서 검출된 Target들의 벡터를 반환하는 함수
std::vector<Target> RadarProcessor::TargetConversion() const
{
    std::vector<Target> found_Targets;
    
    for (size_t i = 0; i < peaks_.size(); ++i)
    {
        float distance = peaks_[i].range_idx * range_resolution;

        int doppler_bin = static_cast<int>(peaks_[i].doppler_idx)
                        - static_cast<int>(doppler_bins / 2);

        float relative_velocity = doppler_bin * doppler_resolution;

        found_Targets.push_back(
            {
                distance,
                relative_velocity,
                angles_[i]
            }
        );
    }
    return found_Targets;
}
