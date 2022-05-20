#ifndef CPPTORRENT_ASYNCWORKER_H
#define CPPTORRENT_ASYNCWORKER_H

#include <functional>
#include <vector>
#include <queue>
#include <atomic>
#include <thread>
#include <condition_variable>
#include <mutex>

struct AsyncWorker {
public:
    using CallBack = std::function<void()>;

    explicit AsyncWorker(size_t thread_count = 1);

    void Start() {
        stopped_.store(false);
        cv_.notify_all();
    }

    void Stop() {
        stopped_.store(true);
        cv_.notify_all();
    }

    void Enqueue(CallBack &&func);

    ~AsyncWorker();

private:
    void run();

    std::queue<CallBack> queue_;

    std::vector<std::thread> threads_;

    mutable std::mutex mut_;

    std::condition_variable cv_;

    std::atomic_bool quit_ = false;

    std::atomic_bool stopped_ = true;
};

#endif // CPPTORRENT_ASYNCWORKER_H
