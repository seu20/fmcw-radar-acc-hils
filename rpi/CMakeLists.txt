cmake_minimum_required(VERSION 3.16)

project(radar_rpi
    VERSION 1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED)
set(CMAKE_CXX_EXTENSIONS OFF)


# ==================================================
# 외부 라이브러리 찾기
# ==================================================

find_package(Threads REQUIRED)

find_library(
    FFTW3F_LIBRARY
    NAMES fftw3f
    REQUIRED
)
# ==================================================
# 프로젝트 라이브러리
# ==================================================

add_library(radar_core
    radar/processor/RadarProcessor.cpp
    radar/reassembler/RadarReassembler.cpp
    radar/receiver/PacketReceiver.cpp
    network/UDPSocket.cpp
)

target_include_directories(radar_core
    PUBLIC
    common
    network
    protocol
    radar/data
    radar/receiver
    radar/reassembler
    radar/processor
)

target_link_libraries(radar_core
    PUBLIC
        Threads::Threads
        ${FFTW3F_LIBRARY}
)

# ==================================================
# 실제 프로그램
# ==================================================

add_executable(radar_rpi
    main.cpp
)

target_link_libraries(radar_rpi
    PRIVATE
        radar_core
)
# ==================================================
# GoogleTest
# ==================================================

enable_testing()

find_package(GTest REQUIRED)

add_executable(process_test
    tests/test_processing.cpp
)

target_link_libraries(process_test
    PRIVATE
        radar_core
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(process_test)