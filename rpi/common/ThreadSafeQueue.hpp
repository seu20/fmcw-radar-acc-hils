#pragma once

#include <pthread.h>
#include <queue>

// 어떤 타입을 써도 사용할 수 있게 template 사용
// RadarPacket (UDP로 수신 하는 radar 복소수 정보)
// RadarFrame  (Packet들을 재조립한 Frame)
template <typename T>   

// 동시성을 부여한 queue
class ThreadSafeQueue
{
private:
    std::queue<T> queue_;

    pthread_mutex_t mutex_;

    pthread_cond_t cond_;

public:
    ThreadSafeQueue()
    {
        pthread_mutex_init(&mutex_, nullptr);
        pthread_cond_init(&cond_, nullptr);
    }

    ~ThreadSafeQueue()
    {
        pthread_mutex_destroy(&mutex_);
        pthread_cond_destroy(&cond_);
    }

    void push(T value){
        pthread_mutex_lock(&mutex_);
        queue_.push(value);  
        pthread_cond_signal(&cond_));     
        pthread_mutex_unlock(&mutex);
    }

    // queue pop 하면서 값도 반환
    T pop(){
        pthread_mutex_lock(&mutex_);
        while(queue_.empty())
        {
            // mutex 언락하고 바로 잠들기 위해 mutex를 인자로 갖고있음
            // queue 비어있으면 blocking
            pthread_cond_wait(&cond_, &mutex_); 
        }
        queue_.pop();
        
        pthread_mutex_unlock(&mutex_);
    }
};