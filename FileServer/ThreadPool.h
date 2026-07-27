#pragma once

#include <vector>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>


class ThreadPool
{
public:

    explicit ThreadPool(size_t threadNum);

    ~ThreadPool();


    void start();


    void stop();


    void submit(std::function<void()> task);



private:

    void worker();


private:

    std::vector<std::thread> workers_;


    std::queue<std::function<void()>> tasks_;


    std::mutex mutex_;


    std::condition_variable cond_;


    std::atomic<bool> running_{false};

};