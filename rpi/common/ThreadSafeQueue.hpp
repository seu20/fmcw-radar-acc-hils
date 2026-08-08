#pragma once

template <typename T>   // 어떤 타입을 써도 사용할 수 있게
class ThreadSafeQueue
{
private:
    std::queue<T> queue_;

    pthread_mutex_t mutex_;

    pthread_cond_t cond_;

public:
    void push(T value);

    T pop();
};
