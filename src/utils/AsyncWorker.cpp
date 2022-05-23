#include "AsyncWorker.h"

#include "random_generator.h"

AsyncWorker::AsyncWorker(size_t thread_count) : threads_(thread_count) {
    for (size_t i = 0; i < thread_count; i++) {
        threads_[i] = std::thread(&AsyncWorker::run, this);
    }
}

AsyncWorker::~AsyncWorker() {
    quit_.store(true);
    cv_.notify_all();
    for (auto &thread : threads_) {
        thread.join();
    }
}

void AsyncWorker::run() {
    while (!quit_.load()) {
        auto lock = std::unique_lock(mut_);

        cv_.wait(lock, [&] { return (!stopped_ && !queue_.empty()) || quit_.load(); });

        if (!queue_.empty() && !stopped_) {
            auto [func, id] = std::move(queue_.front());
            queue_.pop_front();
            executing_callbacks_.erase(id);
            lock.unlock();

            func();
        }
    }
}

size_t AsyncWorker::Enqueue(AsyncWorker::CallBack func) {
    auto lock = std::unique_lock(mut_);

    auto id = random_generator::Random().GetNumber<size_t>();
    while (executing_callbacks_.count(id))
        id = random_generator::Random().GetNumber<size_t>();

    queue_.emplace_back(std::move(func), id);
    executing_callbacks_.emplace(id, std::prev(queue_.end()));

    cv_.notify_one();

    return id;
}

void AsyncWorker::Erase(size_t callback_id) {
    auto lock = std::unique_lock(mut_);

    if (!executing_callbacks_.count(callback_id))
        return;
    queue_.erase(executing_callbacks_.at(callback_id));
    executing_callbacks_.erase(callback_id);
}
