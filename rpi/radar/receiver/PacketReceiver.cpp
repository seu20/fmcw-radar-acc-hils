#include "PacketReceiver.hpp"
#include <iostream>
#include <cstring>
#include <netinet/in.h>     // ntohs(), ntohl()
#include <vector>

void *PacketReceiver::threadfunc(void *arg)
{
    PacketReceiver* self = static_cast<PacketReceiver*>(arg);
    self->run();
    return nullptr;
}

void PacketReceiver::start()
{
    running_ = true;

    pthread_create(
        &thread_id_,
        nullptr,
        threadfunc,
        this
        );
}

bool PacketReceiver::run()
{
    // 배열 최대 크기
    constexpr std::size_t BUFFER_SIZE = 1500;
    // 헤더 크기
    constexpr std::size_t HEADER_SIZE = 12;
    // 버퍼 배열
    uint8_t buffer[BUFFER_SIZE];
    while(running_)
    {
        try {
            struct RadarPacketHeader header;
            std::vector<uint8_t> payload;

            int offset = 0;

            uint32_t temp_32;
            uint16_t temp_16;

            // recvfrom() 은 blocking 함수라 따로 blocking 구현 x
            // TO-DO: 여기서 blocking이면 stop() 해도 모르는거 해결해야함
            ssize_t recv_len = udp_socket_.receive(buffer, BUFFER_SIZE) ; 

            if (recv_len >= HEADER_SIZE) // 헤더 크기 이상 들어왔으면
            {
                // magic_id 대입
                std::memcpy(&temp_32, buffer + offset, sizeof(header.magic_id));
                header.magic_id = ntohl(temp_32);
                offset += sizeof(header.magic_id);

                // frame_id 대입
                std::memcpy(&temp_16, buffer + offset, sizeof(header.frame_id));
                header.frame_id = ntohs(temp_16);
                offset += sizeof(header.frame_id);

                //packet_id
                std::memcpy(&temp_16, buffer + offset, sizeof(header.packet_id));
                header.packet_id = ntohs(temp_16);
                offset += sizeof(header.packet_id);

                //packet_count
                std::memcpy(&temp_16, buffer + offset, sizeof(header.packet_count));
                header.packet_count = ntohs(temp_16);
                offset += sizeof(header.packet_count);

                //payload_bytes
                std::memcpy(&temp_16, buffer + offset, sizeof(header.payload_bytes));
                header.payload_bytes = ntohs(temp_16);
                offset += sizeof(header.payload_bytes);
            }
            else
            {
                continue;
            }

            if (recv_len >= HEADER_SIZE + header.payload_bytes) // payload 크기 검사
            {
                // payload는 바이트 단위라 [uint8_t]
                payload.assign(buffer + offset, buffer + offset + header.payload_bytes);
            }else
            {
                continue;
            }

            RadarPacket packet{
                header,
                payload
            };
            
            packet_queue_.push(packet);
        }
        catch(const std::exception& e)
        {
            std::cerr << e.what() << '\n';
            return false;
        };

    }
    return true;
}

void PacketReceiver::stop()
{
    // 이 순서로 해야지 while loop 중지
    running_ = false;
    pthread_join(thread_id_, nullptr);
}
