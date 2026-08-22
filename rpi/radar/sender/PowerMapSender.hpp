#pragma once

#include <pthread.h>
#include <atomic>
#include <cstdint>
#include <string>
#include <utility>

#include "ThreadSafeQueue.hpp"
#include "PowerMapFrame.hpp"
#include "UDPSocket.hpp"


class PowerMapSender
{
private:
    pthread_t thread_id_;

    static void* thread_func_(void* arg);

    std::atomic<bool> running_;

    ThreadSafeQueue<PowerMapFrame>& power_map_queue_;

    UDPSocket udp_socket_;

    std::string target_ip_;

    uint16_t target_port_;


    // "PMAP"
    static constexpr uint32_t MAGIC = 0x504D4150;

    static constexpr std::size_t HEADER_SIZE = 14;

    static constexpr std::size_t MAX_PAYLOAD_BYTES = 1200;

    static constexpr std::size_t RANGE_BINS = 128;
    static constexpr std::size_t DOPPLER_BINS = 64;

    static constexpr std::size_t POWER_MAP_SIZE =
        RANGE_BINS * DOPPLER_BINS;


public:
    PowerMapSender(
        ThreadSafeQueue<PowerMapFrame>& power_map_queue,
        UDPSocket udp_socket,
        std::string target_ip,
        uint16_t target_port
    ):
        running_(false),
        power_map_queue_(power_map_queue),
        udp_socket_(std::move(udp_socket)),
        target_ip_(std::move(target_ip)),
        target_port_(target_port)
    {}


    void start();

    void run();

    void stop();
};