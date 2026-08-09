#pragma once
#include <pthread.h>
#include <atomic>
#include <UDPSocket.hpp>
#include <RadarFrame.hpp>
#include <RadarPacket.hpp>
#include <ThreadSafeQueue.hpp>
#include <unordered_map>

constexpr uint16_t MAX_PACKET_PER_FRAME = 110;


struct FrameBuffer {
    std::vector<std::vector<uint8_t>> payloads;     // payload의 배열
    // std::vector<bool> received_packet;              // 받았으면 받음 처리
    uint16_t recv_counts;                           // 몇개 도착했는지 (다 도착했으면 110 개)
};

class RadarReassembler {
private:
    ThreadSafeQueue<RadarFrame>& frame_queue_;

    ThreadSafeQueue<RadarPacket>& packet_queue_;

    // [key: frame_id, value: FrameBuffer]
    std::unordered_map<uint32_t, FrameBuffer> frames_;
 
    pthread_t thread_id_;

    static void *thread_func(void *arg);

    std::atomic<bool> running_;     // 메인 스레드와 RadarReassembler 스레드에서 동시성 보장 ( 컴파일러 최적화 방지 )

    std::vector<std::complex<float>> ConvertToComplex(const std::vector<std::vector<uint8_t>>& payloads);

public:

    RadarReassembler(
        ThreadSafeQueue<RadarFrame>& frame_queue,
        ThreadSafeQueue<RadarPacket>& packet_queue):
        running_(false),
        frame_queue_(frame_queue),
        packet_queue_(packet_queue)
    {
    }

    void start();

    bool run();

    void stop();
};



