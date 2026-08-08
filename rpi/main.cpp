#include "UDPSocket.hpp"
#include "ThreadSafeQueue.hpp"
#include "RadarFrame.hpp"
#include "RadarPacket.hpp"
#include "PacketReceiver.hpp"

#include <iostream>     // std::cerr
#include <exception>    // std::exception
#include <cstdlib>      // EXIT_SUCCESS, EXIT_FAILURE
#include <utility>

#define RPI_PORT        2000    //RPI의 UDP Port

int main(int argv, char* args[])
{
    /*
    1차 테스트:
    - UDP 수신 검사 (packet_id, frame_id 검사)
    - iq_data_recv.h 파일로 export
    */
    try {
        UDPSocket udp_socket;
        udp_socket.bind(RPI_PORT);

        // queue 생성
        ThreadSafeQueue<RadarPacket> packet_queue;

        // 스레드 객체 생성
        PacketReceiver receiver_thread(packet_queue, std::move(udp_socket));
        receiver_thread.run();
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << "\n";
        return EXIT_FAILURE;
    }
    
    return EXIT_SUCCESS;
}