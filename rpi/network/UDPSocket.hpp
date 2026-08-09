#pragma once

#include <cstddef>
#include <cstdint>
#include <sys/types.h>
#include <string>

class UDPSocket
{
private:
    int socket_fd_;

public:
    UDPSocket();
    ~UDPSocket();

    // 복사 금지  ( socket_fd 를 보유하고 있어서 이중 close 방지! )
    UDPSocket(const UDPSocket&) = delete;
    UDPSocket& operator=(const UDPSocket&) = delete;

    // 이동 허용 ( std::move()를 활용한 이동은 가능 )
    UDPSocket(UDPSocket&& other) noexcept;  
    UDPSocket& operator=(UDPSocket&& other) noexcept;
  
    void bind(uint16_t port);

    ssize_t receive(
        uint8_t* buffer,
        std::size_t buffer_size
    );

    ssize_t send(
        const uint8_t* data,
        std::size_t data_size,
        const std::string &ip,
        uint16_t port
    );

    int get_fd() const { return socket_fd_; }
};

