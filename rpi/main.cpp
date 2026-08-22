#include "UDPSocket.hpp"
#include "ThreadSafeQueue.hpp"

#include "RadarFrame.hpp"
#include "RadarPacket.hpp"
#include "TargetFrame.hpp"
#include "PowerMapFrame.hpp"

#include "PacketReceiver.hpp"
#include "RadarReassembler.hpp"
#include "RadarProcessor.hpp"

#include "TargetSender.hpp"
#include "PowerMapSender.hpp"

#include "DBSCAN.hpp"

#include <iostream>
#include <exception>
#include <cstdlib>
#include <utility>

#include <unistd.h>


#define PC_IP               "10.0.0.2"

// 기존 ACC Target
#define PC_TARGET_PORT      3000

// 새 Power Map Visualization
#define PC_POWER_MAP_PORT   3001

// PC → RPi RawIQ
#define RPI_PORT            5000


int main(int argc, char* argv[])
{
    try
    {
        // ==========================================
        // UDP Socket
        // ==========================================

        // PC → RPi RawIQ 수신
        UDPSocket udp_recv_socket;

        udp_recv_socket.bind(
            RPI_PORT
        );


        // RPi → PC Target 전송
        UDPSocket udp_target_send_socket;


        // RPi → PC Power Map 전송
        UDPSocket udp_power_map_send_socket;



        // ==========================================
        // Queue
        // ==========================================

        ThreadSafeQueue<RadarPacket>
            packet_queue;

        ThreadSafeQueue<RadarFrame>
            frame_queue;

        ThreadSafeQueue<TargetFrame>
            target_queue;

        ThreadSafeQueue<PowerMapFrame>
            power_map_queue;



        // ==========================================
        // Worker Objects
        // ==========================================

        PacketReceiver receiver_thread(
            packet_queue,
            std::move(udp_recv_socket)
        );


        RadarReassembler packet_reassembler(
            packet_queue,
            frame_queue
        );


        RadarProcessor radar_processor(
            frame_queue,
            target_queue,
            power_map_queue
        );


        TargetSender target_sender(
            target_queue,
            std::move(udp_target_send_socket),
            PC_IP,
            PC_TARGET_PORT
        );


        PowerMapSender power_map_sender(
            power_map_queue,
            std::move(udp_power_map_send_socket),
            PC_IP,
            PC_POWER_MAP_PORT
        );



        // ==========================================
        // Start
        // ==========================================

        packet_reassembler.start();

        receiver_thread.start();

        radar_processor.start();

        target_sender.start();

        power_map_sender.start();


        std::cout
            << "Radar Pipeline Started"
            << std::endl;

        std::cout
            << "Target UDP    : "
            << PC_IP
            << ":"
            << PC_TARGET_PORT
            << std::endl;

        std::cout
            << "Power Map UDP : "
            << PC_IP
            << ":"
            << PC_POWER_MAP_PORT
            << std::endl;



        // ==========================================
        // Main thread 유지
        //
        // while(1){} 사용 금지
        // CPU Core 하나를 계속 소비함
        // ==========================================

        while (true)
        {
            pause();
        }
    }
    catch (const std::exception& e)
    {
        std::cerr
            << e.what()
            << "\n";

        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}