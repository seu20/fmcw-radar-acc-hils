#pragma once
#include "RadarPacket.hpp"
#include "ThreadSafeQueue.hpp"
#include <pthread.h>
#include <queue>
#include <atomic>

// UDP 로 packet 받고 RadarPacket queue 에 push
class PacketReceiver {
private:
    pthread_t thread_id_;

    std::atomic<bool> running_;

    //PacketReceiver 에서 push
    // PacketReassembler에서 pop 하기 때문에 참조해서 원본 수정
    ThreadSafeQueue<RadarPacket>& packet_queue_;   
    
    static void* threadfunc(void* arg);
public:
    PacketReceiver(
        ThreadSafeQueue<RadarPacket>& queue
    )
        : packet_queue_(queue),
          running_(false)
    {
    }

    ~PacketReceiver() { stop(); };

    void start();

    void run();

    void stop();
};




/*
main.cpp

int main()
{
....
ThreadSafeQueue<RadarPacket> packet_queue;
PacketReceiver(packet_queue);           
}
*/