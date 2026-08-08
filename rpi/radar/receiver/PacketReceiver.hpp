#pragma once
#include "protocol.hpp"
#include "ThreadSafeQueue.hpp"
#include <queue>

// UDP 로 packet 받고 Packet객체의 queue 에 push
class PacketReceiver {
private:
    ThreadSafeQueue<RadarPacket>& packet_queue_;

public:
    UdpReceiver(
        ThreadSafeQueue<RadarPacket>& queue
    )
        : packet_queue_(queue)
    {
    }
};
};