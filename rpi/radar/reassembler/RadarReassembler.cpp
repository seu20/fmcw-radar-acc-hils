#include "RadarReassembler.hpp"
#include <vector>
#include <cstring>

void *RadarReassembler::thread_func(void* arg)
{
    RadarReassembler* self = static_cast<RadarReassembler*>(arg);
    self->run();
    return nullptr;
}

std::vector<std::complex<float>> RadarReassembler::ConvertToComplex(const std::vector<std::vector<uint8_t>>& payloads)
{
    std::vector<std::complex<float>> iq_data;

    const size_t packet_count = payloads.size();

    for (size_t i = 0; i < packet_count; ++i)
    {
        const size_t payload_bytes = payloads[i].size();

        for (size_t offset = 0;
             sizeof(float) * 2 * (offset + 1) <= payload_bytes;
             ++offset)
        {
            float i_value;
            float q_value;

            std::memcpy(
                &i_value,
                payloads[i].data() + sizeof(float) * 2 * offset,
                sizeof(float));

            std::memcpy(
                &q_value,
                payloads[i].data()
                    + sizeof(float) * 2 * offset
                    + sizeof(float),
                sizeof(float));

            iq_data.emplace_back(i_value, q_value);
        }
    }

    return iq_data;
}

void RadarReassembler::start()
{
    running_ = true;

    pthread_create(
            &thread_id_,
            nullptr,
            thread_func,
            this
    );
}

// PacketQueue 에서 pop 후 데이터 정렬
// UDP는 순서가 보장되지 않아서 frame_id 읽으면서 순서정렬해야함
// payload uint8_t 로 오는거 float32로 변환
bool RadarReassembler::run()
{
    while(running_)
    {
        // 뒤의 데이터 pop
        RadarPacket packet = packet_queue_.pop();

        // packet_id 검사
        uint16_t packet_id = packet.header.packet_id;

        // frame_id 검사
        uint32_t frame_id = packet.header.frame_id;

        // frame 이 처음 수신됐으면 
        /* 
        - payloads 크기 초기화
        - 수신 기록 초기화
        - 수신 갯수 초기화
        */
        if (frames_[frame_id].payloads.empty())
        {
            // frames_[frame_id].received_packet.resize(packet.header.packet_count, false);
            frames_[frame_id].payloads.resize(packet.header.packet_count);
            frames_[frame_id].recv_counts = 0;
        }

        {
            // 해당 frame에 채우기
            frames_[frame_id].payloads[packet_id] = std::move(packet.payload);
            // frames_[frame_id].received_packet[packet_id] = true;
            frames_[frame_id].recv_counts++;
        }

        // packet_count 다 채워졌으면 
        // 1. vector<vector<uint8_t>> 배열을 vector<complex<float>> 로 변환
        // 2. frame_queue_ 에 push 하고 frames_에서 해당 frame erase
        if ( frames_[frame_id].recv_counts == packet.header.packet_count )
        {
            std::vector<std::complex<float>> iq_data = ConvertToComplex(frames_[frame_id].payloads);
            // packet 이 다 도착했으면 frame 포장 후 queue 에 넣기
            RadarFrame frame = {
                frame_id,
                std::move(iq_data)
            };
            frame_queue_.push(frame);

            frames_.erase(frame_id);
        }   
    }
    return true;
}

void RadarReassembler::stop()
{
    running_ = false;
    pthread_join(thread_id_, nullptr);
}





