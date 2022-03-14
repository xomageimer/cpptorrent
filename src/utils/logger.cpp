#include "logger.h"

Logger &Logger::log() {
    static Logger logger_{};
    return logger_;
}

void Logger::outtime() {
    auto prev_time = last_action_time;
    last_action_time = std::chrono::steady_clock::now();
    (*os_ptr) << std::chrono::duration_cast<std::chrono::milliseconds>(last_action_time - prev_time).count() << "ms: ";
}