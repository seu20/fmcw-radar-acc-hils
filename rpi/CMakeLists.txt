cmake_minimum_required(VERSION 3.16)

project(radar_rpi
    VERSION 1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED)
set(CMAKE_CXX_EXTENSIONS OFF)

# --------------------------------------------------
# 실행 파일 생성
# --------------------------------------------------
add_executable(radar_rpi

    main.cpp

    network/UDPSocket.cpp

    radar/receiver/PacketReceiver.cpp
    radar/reassembler/RadarReassembler.cpp
    radar/processor/RadarProcessor.cpp
)

# --------------------------------------------------
# Header 검색 경로
# --------------------------------------------------
# 헤더 검색 경로
target_include_directories(radar_rpi
    PRIVATE
    common
    network
    protocol
    radar/data
    radar/receiver
    radar/reassembler
    radar/processor
)

# --------------------------------------------------
# pthread
# --------------------------------------------------
find_package(Threads REQUIRED)

# --------------------------------------------------
# FFTW float 버전
# --------------------------------------------------
find_library(
    FFTW3F_LIBRARY
    NAMES fftw3f
    REQUIRED
)

# --------------------------------------------------
# Library Link
# --------------------------------------------------
target_link_libraries(radar_rpi
    PRIVATE
        Threads::Threads
        ${FFTW3F_LIBRARY}
)

# --------------------------------------------------
# Compiler Warning
# --------------------------------------------------
target_link_libraries(radar_rpi
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
)