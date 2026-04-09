#pragma once
#include <vector>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <queue>
#include <future>
class ThreadPool
{
private:
    std::vector<std::thread> _threads;        // 线程数组
    std::queue<std::function<void()>> _tasks; // 任务队列
    std::mutex _tasks_mtx;                    // 任务锁
    std::condition_variable _cv;              // 条件变量
    bool _stop;

public:
    // std::thread::hardware_concurrency()当前硬件CPU核数
    // ThreadPool(unsigned int size = std::thread::hardware_concurrency());
    ThreadPool(unsigned int size = 10);
    ~ThreadPool();
    template <class F, class... Args>
    auto add(F &&f, Args &&...args)
        -> std::future<typename std::result_of<F(Args...)>::type>;
};

template <class F, class... Args>
auto ThreadPool::add(F &&f, Args &&...args) -> std::future<typename std::result_of<F(Args...)>::type>
{
    using return_type = typename std::result_of<F(Args...)>::type;
    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...));
    std::future<return_type> res = task->get_future();
    {
        std::unique_lock<std::mutex> lock(_tasks_mtx);
        if (_stop)
            throw std::runtime_error("enqueue on stopped ThreadPool");

        _tasks.emplace([task]()
                       { (*task)(); });
    }
    _cv.notify_one();
    return res;
}