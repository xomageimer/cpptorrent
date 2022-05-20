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
    while (quit_.load()) {
        auto lock = std::unique_lock(mut_);

        cv_.wait(lock, [&] { return (!stopped_ && !queue_.empty()) || quit_.load(); });

        if (!queue_.empty() && !stopped_) {
            auto func = std::move(queue_.front());
            queue_.pop();
            lock.unlock();

            func();
        }
    }
}

void AsyncWorker::Enqueue(AsyncWorker::CallBack &&func) {
    auto lock = std::unique_lock(mut_);
    queue_.push(func);
    cv_.notify_one();
}