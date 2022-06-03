#ifndef CPPTORRENT_ASYNC_WORKER_H
#define CPPTORRENT_ASYNC_WORKER_H

#include <functional>
#include <vector>
#include <list>
#include <map>
#include <atomic>
#include <thread>
#include <condition_variable>
#include <mutex>

#include "move_function.h"

struct AsyncWorker {
public:
    using CallBack = move_function;
    using Node = std::pair<CallBack, size_t>;
    using Iter = std::list<Node>::iterator;

    explicit AsyncWorker(size_t thread_count = 1);

    void Start() {
        stopped_.store(false);
        cv_.notify_all();
    }

    void Stop() {
        stopped_.store(true);
        cv_.notify_all();
    }

    size_t Enqueue(CallBack func);

    void Erase(size_t callback_id);

    ~AsyncWorker();

private:
    void run();

    std::list<Node> queue_;
    std::map<size_t, Iter> executing_callbacks_;

    std::vector<std::thread> threads_;

    mutable std::mutex mut_;

    std::condition_variable cv_;

    std::atomic_bool quit_ = false;

    std::atomic_bool stopped_ = true;
};

#endif // CPPTORRENT_ASYNC_WORKER_H
