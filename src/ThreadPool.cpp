#include "ThreadPool.h"

ThreadPool::ThreadPool(unsigned int size)
    : _stop(false)
{
    for (int i = 0; i < size; ++i)
    {
        _threads.emplace_back(std::thread([this]()
                                          {
            while(true)
            {
                std::function<void()> task;
                {
                    //在这个{}作用域内对std::mutex加锁，出了作用域后自动解锁，不需要调用unlock()
                    std::unique_lock<std::mutex> lock(_tasks_mtx);
                    //wait为fasle，阻塞当前线程并释放锁；true会让线程不等待直接退出wait
                    _cv.wait(lock, [this](){
                        return _stop || !_tasks.empty();//只要任务队列是空，而且stop为false，就是false，阻塞当前线程
                    });
                    if(_stop && _tasks.empty()) return; //任务队列为空且线程池停止，退出线程
                    task = _tasks.front();//取出任务队列的队头任务
                    _tasks.pop();
                    
                }
                task();//执行任务
            } }));
        // printf("create %d thread to push ThreadPool\n", i);
    }
}

ThreadPool::~ThreadPool()
{
    {
        std::unique_lock<std::mutex> lock(_tasks_mtx);
        _stop = true;
    }
    _cv.notify_all();
    for(std::thread &th : _threads)
    {
        if(th.joinable())
            th.join();
    }
}

// void ThreadPool::add(std::function<void()> func)
// {
//     {
//         std::unique_lock<std::mutex> lock(_tasks_mtx); // 自动上锁解锁
//         if (_stop)
//             throw std::runtime_error("ThreadPoll already stop, can't add task any more");
//         _tasks.emplace(func); // 比push性能更优，尾插，减少了移动构造
//     }
//     // printf("新建事件，唤醒一个线程处理\n");
//     _cv.notify_one(); // 唤醒一个线程处理事件
// }


