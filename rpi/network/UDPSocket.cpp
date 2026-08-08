#include <UDPSocket.hpp>

#include <sys/socket.h>   // socket(), bind(), recvfrom(), sendto()
#include <netinet/in.h>   // sockaddr_in, htons(), INADDR_ANY
#include <arpa/inet.h>    // inet_pton()
#include <unistd.h>       // close()

#include <cerrno>         // errno
#include <cstring>        // std::strerror()
#include <stdexcept>      // std::runtime_error

UDPSocket::UDPSocket()
{
    socket_fd_ = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (socket_fd_ == -1)
    {
        throw std::runtime_error{
            std::string("Socket not created!: ") + std::strerror(errno)
        };
    }
}

UDPSocket::~UDPSocket()
{
    if (socket_fd_ >= 0)
    {
        ::close(socket_fd_);
    }
}

void UDPSocket::bind(uint16_t port)
{
    sockaddr_in addr{};                             // {}로 구조체 초기화 해야함!
    addr.sin_addr.s_addr = htonl(INADDR_ANY);       // 어느 IP 에서든지 수신
    addr.sin_family = AF_INET;                      // IPv4
    addr.sin_port = htons(port);                    // host-to-network-short(16bit): 호스트에서 네트워크 형식으로 변혼

    if (::bind(
            socket_fd_,
            reinterpret_cast<sockaddr*>(&addr),
            sizeof(addr)) == -1)
    {
        throw std::runtime_error {
            std::string("UDP Binding Failed: ") + std::strerror(errno)
        };
    }
}

ssize_t UDPSocket::receive(uint8_t *buffer, std::size_t buffer_size
    //sockaddr_in* dest_addr  
    // UDP는 굳이 송신자의 ip 번호 알아야할 필요없어서 제외
    )
{
    ssize_t recvlen = (::recvfrom(
            socket_fd_,
            buffer,             // 버퍼 배열
            buffer_size,        // 최대로 받을 수 있는 buffer
            0,                  //
            nullptr,            // 송신자 ip 필요없으니 nullptr
            nullptr));

    if (recvlen == -1)
    {   
        throw std::runtime_error{
            std::string("UDP Receive failed: ") + std::strerror(errno)
        };
    }

    return recvlen;
}


ssize_t UDPSocket::send(const uint8_t *data, std::size_t data_size, const std::string &ip, uint16_t port)
{
    sockaddr_in dest_addr{};                // 어디로 보낼지 설정
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(port);

    /*
        inet_pton() 반환
        ================================================
        1  → 정상적으로 IP 변환 성공
        0  → 문자열이 유효한 IP가 아님
        -1 → AF_INET 같은 address family 자체에 문제
    */

    int result = inet_pton(
        AF_INET,
        ip.c_str(),
        &dest_addr.sin_addr);
    
    if (result == 0)
    {
        throw std::runtime_error("Invalid IPv4 address: " + ip);
    }

    if (result == -1)
    {
        throw std::runtime_error(
            std::string("inet_pton failed: ") + std::strerror(errno)
        );
    }
    

    ssize_t sendlen = ::sendto(
        socket_fd_,
        data,
        data_size,
        0,
        reinterpret_cast<sockaddr*>(&dest_addr),
        sizeof(dest_addr)
    );

    if (sendlen == -1)
    {
        throw std::runtime_error{
            std::string("UDP Send Failed: ") + std::strerror(errno)
        };
    }

    return sendlen;
}
