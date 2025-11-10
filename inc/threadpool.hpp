#ifndef THREADPOOL_HPP__
#define THREADPOOL_HPP__

#include <thread>
#include <vector>
#include <queue>
#include <condition_variable>
#include <mutex>
#include <future>
#include <functional>

class ThreadPool
{
public:
    explicit ThreadPool(size_t nWorkers)
        : stopRequired_(false)
    {
        if (nWorkers == 0)
            throw std::runtime_error("nWorkers should be > 0!");

        workers_.reserve(nWorkers);

        for (size_t i = 0; i < nWorkers; ++i)
            workers_.emplace_back([this] { Worker(); });
    }

    ThreadPool()
        : ThreadPool(std::thread::hardware_concurrency()) {}

    ~ThreadPool() noexcept
    {
        Stop();
    }

    template <typename Func, typename ...Args>    
    auto AddTask(Func &&func, Args &&...args)
        -> std::future<std::invoke_result_t<Func, Args...>>
    {
        using Ret = std::invoke_result_t<Func, Args...>;

        std::packaged_task<Ret()> task(
            [func = std::forward<Func>(func), args_tuple = std::make_tuple(std::forward<Args>(args)...)]
            {
                return std::apply(
                    [&](auto &&...captured_args) -> Ret {
                        return std::invoke(func, std::forward<decltype(captured_args)>(captured_args)...);
                    },
                    std::move(args_tuple)
                );
            }
        );
        std::future<Ret> fut = task.get_future();

        {
            std::lock_guard l(queueMutex_);
            if (stopRequired_)
                throw std::runtime_error("ThreadPool is stopping or stopped");

            taskQueue_.emplace([task = std::move(task)] mutable { task(); });
        }
        
        poolNotifier_.notify_one();

        return fut;
    }

    void Stop()
    {
        {
            std::lock_guard l(queueMutex_);
            if (stopRequired_)
                return;

            stopRequired_ = true;
        }

        poolNotifier_.notify_all();

        for (auto &w : workers_)
            if (w.joinable())
                w.join();
    }

    size_t Size() const noexcept
    {
        return workers_.size();
    }

private:
    void Worker()
    {
        while (true)
        {
            std::packaged_task<void()> task;

            {
                std::unique_lock l(queueMutex_);
                poolNotifier_.wait(
                    l, 
                    [this] -> bool { return !taskQueue_.empty() || stopRequired_; }   
                );

                if (taskQueue_.empty() && stopRequired_)
                    return;

                task = std::move(taskQueue_.front());
                taskQueue_.pop();
            }

            task();
        }
    }

    std::condition_variable poolNotifier_;

    mutable std::mutex queueMutex_;
    std::queue<std::packaged_task<void()>> taskQueue_;

    std::vector<std::thread> workers_;

    bool stopRequired_;
};

#endif // THREADPOOL_HPP__
