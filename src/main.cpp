#include <iostream>
#include <chrono>
#include "threadpool.hpp"

int main()
{
    using namespace std::chrono_literals;
    ThreadPool pool(5);
    std::vector<std::future<std::thread::id>> v;

    std::mutex mut;

    auto startTime = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < pool.Size() * 2; ++i)
        v.push_back(pool.AddTask(
            [&mut]
            {
                {
                    std::lock_guard l(mut);
                    std::cout << "hello from: " << std::this_thread::get_id() << '\n' << std::flush;
                }
                std::this_thread::sleep_for(1s);
                return std::this_thread::get_id();
            }
        ));

    pool.Stop();

    auto endTime = std::chrono::high_resolution_clock::now();

    for (auto &f : v)
        std::cout << "result: " << f.get() << '\n';

    float timeElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
    std::cout << "Time elapsed: " << timeElapsed / 1000.0 << " s.\n";

    return 0;
}
