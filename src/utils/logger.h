#ifndef CPPTORRENT_LOGGER_H
#define CPPTORRENT_LOGGER_H

#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>

struct Logger {
public:
    static Logger &log();
    template <typename... Args> void out(Args... args) {
        std::lock_guard lock(m);

        outtime();
        (*os_ptr) << std::endl;

        ss.str(std::string());
        ss.clear();
        (ss << ... << args);

        std::string line{};
        for (; std::getline(ss, line);) {
            (*os_ptr) << '\t' << line << std::endl;
        }
        (*os_ptr) << std::endl;
    }
    void reset(std::ostream &new_os) { os_ptr = &new_os; }
    ~Logger() {
        out("Logger deleted");
        f_log.close();
    }

private:
    Logger() {
#ifdef FILE_LOGGER
        ss << std::chrono::duration_cast<std::chrono::seconds>(last_action_time.time_since_epoch()).count();
        f_log.open(std::string("out_" + ss.str() + ".log"), std::ios::out | std::ios::binary);
        os_ptr = &f_log;
#endif
        out("Logger created");
    };
    void outtime();
    std::mutex m;
    std::stringstream ss;
    std::fstream f_log;
    std::ostream *os_ptr = &std::cerr;
    std::chrono::steady_clock::time_point last_action_time = std::chrono::steady_clock::now();
};

#ifdef FILE_LOGGER
#define LOG(...) Logger::log().out(__VA_ARGS__)
#define RESET(os)
#endif
#ifdef CONSOLE_LOGGER
#define LOG(...)  Logger::log().out(__VA_ARGS__)
#define RESET(os) Logger::log().reset(os);
#endif
#ifndef CONSOLE_LOGGER
#ifndef FILE_LOGGER
#define LOG(...)
#define RESET(os)
#endif
#endif

#endif // CPPTORRENT_LOGGER_H
