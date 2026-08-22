#include "PowerMapSender.hpp"

#include <arpa/inet.h>

#include <algorithm>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>


void* PowerMapSender::thread_func_(void* arg)
{
    PowerMapSender* self =
        static_cast<PowerMapSender*>(arg);

    try
    {
        self->run();
    }
    catch (const std::exception& e)
    {
        std::cerr
            << "[PowerMapSender] "
            << e.what()
            << std::endl;
    }

    return nullptr;
}


void PowerMapSender::start()
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

        throw std::runtime_error(
            "PowerMapSender Thread not created"
        );
    }
}


void PowerMapSender::run()
{
    while (running_)
    {
        PowerMapFrame frame;

        try
        {
            frame = power_map_queue_.pop();
        }
        catch (const std::exception&)
        {
            break;
        }


        if (frame.power.size() != POWER_MAP_SIZE)
        {
            std::cerr
                << "[PowerMapSender] Invalid Power Map Size: "
                << frame.power.size()
                << std::endl;

            continue;
        }


        const uint8_t* power_bytes =
            reinterpret_cast<const uint8_t*>(
                frame.power.data()
            );


        const std::size_t total_bytes =
            frame.power.size() * sizeof(float);


        const uint16_t packet_count =
            static_cast<uint16_t>(
                (total_bytes + MAX_PAYLOAD_BYTES - 1)
                / MAX_PAYLOAD_BYTES
            );


        for (
            uint16_t packet_id = 0;
            packet_id < packet_count;
            ++packet_id
        )
        {
            const std::size_t offset =
                static_cast<std::size_t>(packet_id)
                * MAX_PAYLOAD_BYTES;


            const std::size_t remaining =
                total_bytes - offset;


            const uint16_t payload_bytes =
                static_cast<uint16_t>(
                    std::min(
                        remaining,
                        MAX_PAYLOAD_BYTES
                    )
                );


            std::vector<uint8_t> datagram(
                HEADER_SIZE + payload_bytes
            );


            // =============================================
            // Header
            //
            // 0  ~ 3  magic
            // 4  ~ 7  frame_id
            // 8  ~ 9  packet_id
            // 10 ~ 11 packet_count
            // 12 ~ 13 payload_bytes
            // =============================================

            uint32_t magic_be =
                htonl(MAGIC);

            uint32_t frame_id_be =
                htonl(frame.frame_id);

            uint16_t packet_id_be =
                htons(packet_id);

            uint16_t packet_count_be =
                htons(packet_count);

            uint16_t payload_bytes_be =
                htons(payload_bytes);


            std::memcpy(
                datagram.data() + 0,
                &magic_be,
                sizeof(magic_be)
            );

            std::memcpy(
                datagram.data() + 4,
                &frame_id_be,
                sizeof(frame_id_be)
            );

            std::memcpy(
                datagram.data() + 8,
                &packet_id_be,
                sizeof(packet_id_be)
            );

            std::memcpy(
                datagram.data() + 10,
                &packet_count_be,
                sizeof(packet_count_be)
            );

            std::memcpy(
                datagram.data() + 12,
                &payload_bytes_be,
                sizeof(payload_bytes_be)
            );


            // =============================================
            // Payload
            // =============================================

            std::memcpy(
                datagram.data() + HEADER_SIZE,
                power_bytes + offset,
                payload_bytes
            );


            udp_socket_.send(
                datagram.data(),
                datagram.size(),
                target_ip_,
                target_port_
            );
        }
    }
}


void PowerMapSender::stop()
{
    if (running_)
    {
        running_ = false;

        power_map_queue_.close();

        pthread_join(
            thread_id_,
            nullptr
        );
    }
}