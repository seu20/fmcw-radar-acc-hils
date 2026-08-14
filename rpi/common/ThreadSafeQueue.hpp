#pragma once

#include <pthread.h>
#include <queue>
#include <stdexcept>
#include <utility>

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

    bool closed_;

public:
    ThreadSafeQueue():closed_(false)
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
        queue_.push(std::move(value));  
        pthread_cond_signal(&cond_);     
        pthread_mutex_unlock(&mutex_);
    }

    // queue pop 하면서 값도 반환
    T pop(){
        pthread_mutex_lock(&mutex_);
        while(queue_.empty() && !closed_)
        {
            // mutex 언락하고 바로 잠들기 위해 mutex를 인자로 갖고있음
            // queue 비어있으면 blocking
            pthread_cond_wait(&cond_, &mutex_); 
        }

        // 큐가 비었고 closed 가 참이라면 탈출
        if (queue_.empty() && closed_)
        {
            pthread_mutex_unlock(&mutex_);
            throw std::runtime_error("Queue Closed !");
        }
        // 새로 생성하지 않고 std::move 를 통해 이동
        T value = std::move(queue_.front());   
        queue_.pop();

        pthread_mutex_unlock(&mutex_);

        return value;
    }

    // queue 닫기 함수 
    // pop 에서 일으키는 blocking을 방지하기 위해 상위 스레드에서 close() 함수 사용
    // broadcast 사용해서 cond_wait을 모두 깨움
    void close()
    {
        pthread_mutex_lock(&mutex_);

        closed_ = true;
        pthread_cond_broadcast(&cond_);

        pthread_mutex_unlock(&mutex_);
    }
};