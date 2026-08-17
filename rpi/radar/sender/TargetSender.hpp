#pragma once
#include <pthread.h>
#include <atomic>
#include "ThreadSafeQueue.hpp"
#include "TargetFrame.hpp"
#include "UDPSocket.hpp"
#include <optional>
#include <utility>
#include <string>
#include <cstdint>

class TargetSender {
private:
    pthread_t thread_id_;

    static void* thread_func_(void* arg);

    std::atomic<bool> running_;

    UDPSocket udp_socket_;

    std::string target_ip_;

    uint16_t target_port_;

    ThreadSafeQueue<TargetFrame>& target_queue_;

    std::optional<Target> find_LeadTarget(const std::vector<Target>& targets);

    static constexpr float center_lane_distance_from_ego = 0.0f;
public:
    TargetSender(
        ThreadSafeQueue<TargetFrame>& target_queue,
        UDPSocket udp_socket,
        std::string target_ip,
        uint16_t target_port
    ):
    running_(false),
    udp_socket_(std::move(udp_socket)),
    target_ip_(target_ip),
    target_port_(target_port),
    target_queue_(target_queue)
    {}

    void start();

    void run();

    void stop();
};