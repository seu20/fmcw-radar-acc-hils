#pragma once
#include <pthread.h>
#include <atomic>

class RadarProcessor {
private:

    pthread_t thread_id_;

    static void *thread_func(void *arg);

    std::atomic<bool> running_;     // 메인 스레드와 RadarProcessor 스레드에서 동시성 보장 ( 컴파일러 최적화 방지 )

public:

    RadarProcessor():running_(false){}

    void start();

    void run();

    void stop();
};

