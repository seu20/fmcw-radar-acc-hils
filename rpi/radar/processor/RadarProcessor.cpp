#include "RadarProcessor.hpp"

void *RadarProcessor::thread_func(void* arg)
{
    RadarProcessor* self = static_cast<RadarProcessor*>(arg);
    self->run();
    return nullptr;
}



void RadarProcessor::start()
{
    running_ = true;

    pthread_create(
            &thread_id_,
            nullptr,
            thread_func,
            this
    );
}

 
bool RadarProcessor::run()
{
    while(running_)
    {
        // 뒤의 데이터 pop
    }
}

void RadarProcessor::stop()
{
    running_ = false;
    pthread_join(thread_id_, nullptr);
}
