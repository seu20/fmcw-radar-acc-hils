#include "UDPSocket.hpp"
#include "ThreadSafeQueue.hpp"
#include "RadarFrame.hpp"
#include "RadarPacket.hpp"
#include "PacketReceiver.hpp"
#include "RadarReassembler.hpp"
#include "RadarProcessor.hpp"
#include "TargetSender.hpp"
#include "DBSCAN.hpp"

#include <iostream>     // std::cerr
#include <exception>    // std::exception
#include <cstdlib>      // EXIT_SUCCESS, EXIT_FAILURE
#include <utility>

#define PC_IP           "10.0.0.2"  // PC의 IP
#define PC_PORT         3000        // PC의 수신 Port
#define RPI_IP          "10.0.0.3"  // RPI의 IP
#define RPI_PORT        5000        // RPI의 UDP Port

int main(int argv, char* args[])
{
    /*
    1차 테스트:
    - UDP 수신 검사 
    */

    try {
        
        UDPSocket udp_recv_socket;
        udp_recv_socket.bind(RPI_PORT);

        UDPSocket udp_send_socket;

        // queue 생성 (radar packet, radar frame)
        ThreadSafeQueue<RadarPacket> packet_queue;
        ThreadSafeQueue<RadarFrame> frame_queue;
        ThreadSafeQueue<TargetFrame> target_queue;

        // UDP 스레드 객체 생성
        PacketReceiver receiver_thread(packet_queue, std::move(udp_recv_socket));

        // Packet Reassembler 스레드 생성
        RadarReassembler packet_reassembler(packet_queue, frame_queue);

        // Radar Processor 스레드 생성
        RadarProcessor radar_processor(frame_queue, target_queue);

        // Target Sender 스레드 생성
        TargetSender target_sender(target_queue, std::move(udp_send_socket), PC_IP, PC_PORT);

        packet_reassembler.start();
        receiver_thread.start();
        radar_processor.start();
        target_sender.start();
        
        while(1){}
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << "\n";
        return EXIT_FAILURE;
    }
    
    return EXIT_SUCCESS;
}