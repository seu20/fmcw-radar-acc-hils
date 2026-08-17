#include "TargetSender.hpp"
#include <stdexcept>
#include <cstring>
#include <string>
#include <cmath>
#include <iostream>
#include <optional>


void *TargetSender::thread_func_(void *arg)
{
    TargetSender* self = static_cast<TargetSender*>(arg);

    try {
        self->run();
    }
    catch (const std::exception& e)
    {
        std::cerr << "[TargetSender] "
                  << e.what() << std::endl;
    }

    return nullptr;
}


// 같은 차선에 있는 Target 중 가장 가까운 Target 선택
std::optional<Target> TargetSender::find_LeadTarget(
    const std::vector<Target>& targets)
{
    std::optional<Target> lead_target = std::nullopt;

    for (const auto& target : targets)
    {
        // Radar 기준 Target의 횡방향 거리
        float lateral_distance =
            target.distance * std::sin(target.angle);

        // 현재 단순화된 차선 범위 밖이면 제외
        if (std::abs(lateral_distance) >
            center_lane_distance_from_ego)
        {
            continue;
        }

        // 첫 번째 유효 Target
        if (!lead_target.has_value())
        {
            lead_target = target;
            continue;
        }

        // 기존 Lead Target보다 가까우면 교체
        if (target.distance < lead_target->distance)
        {
            lead_target = target;
        }
    }

    return lead_target;
}


void TargetSender::start()
{
    running_ = true;

    int res = pthread_create(
        &thread_id_,
        nullptr,
        thread_func_,
        this
    );

    if (res != 0)
    {
        running_ = false;

        throw std::runtime_error{
            std::string("Target Sender not Created: ") +
            std::strerror(res)
        };
    }
}


void TargetSender::run()
{
    while (running_)
    {
        TargetFrame frame;

        try
        {
            frame = target_queue_.pop();
        }
        catch (const std::exception& e)
        {
            std::cerr << e.what() << std::endl;
            break;
        }

        std::optional<Target> target =
            find_LeadTarget(frame.targets);


        // 기본값:
        // 이번 Radar Frame에는 Lead Target 없음
        LeadTargetFrame lead_target_frame {
            frame.frame_id,
            false,
            {
                0.0f,      // distance
                0.0f,      // relative_velocity
                0.0f       // angle
            }
        };


        // Lead Target이 존재하면 실제 Target 저장
        if (target.has_value())
        {
            lead_target_frame.valid = true;
            lead_target_frame.target = target.value();
        }


        // LeadTargetFrame을 byte buffer로 보고 UDP 송신
        udp_socket_.send(
            reinterpret_cast<const uint8_t*>(
                &lead_target_frame
            ),
            sizeof(LeadTargetFrame),
            target_ip_,
            target_port_
        );
    }
}


void TargetSender::stop()
{
    if (running_)
    {
        running_ = false;

        target_queue_.close();

        pthread_join(
            thread_id_,
            nullptr
        );
    }
}