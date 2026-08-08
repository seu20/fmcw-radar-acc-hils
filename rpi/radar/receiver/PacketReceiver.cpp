#include "PacketReceiver.hpp"

void *PacketReceiver::threadfunc(void *arg)
{
    PacketReceiver* self = static_cast<PacketReceiver*>(arg);
    self->run();
    return nullptr;
}

void PacketReceiver::start()
{
    running_ = true;

    pthread_create(
        &thread_id_,
        nullptr,
        threadfunc,
        this
        );
}

void PacketReceiver::run()
{
    while(running_)
    {

    }
}

void PacketReceiver::stop()
{
    pthread_join(thread_id_, nullptr);

    running_ = false;
}
