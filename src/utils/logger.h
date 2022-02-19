#ifndef CPPTORRENT_LOGGER_H
#define CPPTORRENT_LOGGER_H

#include <iostream>
#include <fstream>
#include <string>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <chrono>

struct Logger {
public:
    static Logger & log();
    template <typename ... Args>
    void out(Args&&... args) {
        std::lock_guard lock(m);

        outtime();
        (*os_ptr) << std::endl;

        ss.str(std::string());
        ss.clear();
        (ss << ... << args);

        std::string line;
        for (; std::getline(ss, line); ){
            (*os_ptr) << '\t' << line << std::endl;
        }
        (*os_ptr) << std::endl;
    }
    void reset(std::ostream & new_os) { os_ptr = &new_os; }
    ~Logger() {
        out("Logger deleted");
    }
private:
    Logger() {
        out("Logger created");
    };
    void outtime();
    std::mutex m;
    std::stringstream ss;
    std::ostream * os_ptr = &std::cerr;
    std::chrono::steady_clock::time_point last_action_time = std::chrono::steady_clock::now();
};

#ifdef LOGGER
#define LOG(...)                 \
Logger::log().out(__VA_ARGS__)
#define RESET(os)                \
Logger::log().reset(os);
#else
#define LOG(...)
#define RESET(os)
#endif

#endif //CPPTORRENT_LOGGER_H
