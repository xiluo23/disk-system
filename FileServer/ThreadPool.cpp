#include"ThreadPool.h"

ThreadPool::ThreadPool(size_t threadNum)
{
    workers_.reserve(threadNum);

}

ThreadPool::~ThreadPool(){
    running_=false;
    cond_.notify_all();
    for(auto&t:workers_){
        t.join();
    }
}


void ThreadPool::worker()
{
    while(running_)
    {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(mutex_);


            cond_.wait(
                lock,
                [this]
                {
                    return !tasks_.empty()
                           ||
                           !running_;
                });
            if(!running_
               &&
               tasks_.empty())
                return;
            task =
                std::move(tasks_.front());
            tasks_.pop();
        }
        // 执行业务
        task();
    }
}

void ThreadPool::submit(std::function<void()> task)
{
    {
        std::lock_guard<std::mutex>lock(mutex_);
        tasks_.push(std::move(task));
    }
    cond_.notify_one();
}

void ThreadPool::start()
{
    running_=true;
    int threadNum=workers_.capacity();
    for(size_t i=0;i<threadNum;i++)
    {
        workers_.emplace_back(
            &ThreadPool::worker,
            this);
    }
}