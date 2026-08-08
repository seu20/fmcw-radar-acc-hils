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

void RadarProcessor::run()
{
    while(running_)
    {

    }
}

void RadarProcessor::stop()
{
    pthread_join(thread_id_, nullptr);
    
    running_ = false;
}
