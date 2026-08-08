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

    // 복사 금지

    // 이동 허용
  

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

