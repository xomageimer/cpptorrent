#ifndef CPPTORRENT_PROFILE_H
#define CPPTORRENT_PROFILE_H

#include <iostream>
#include <chrono>
#include <sstream>
#include <string>

#include <mutex>
#include <utility>

struct TotalDuration {
    std::string message;
    std::chrono::steady_clock::duration value;

    std::mutex m;
    size_t count {0};

    explicit TotalDuration(std::string msg = "")
        : message(std::move(msg)), value(0) {
    }

    ~TotalDuration() {
        std::lock_guard<std::mutex> lock(m);

        if (!count) {
            std::cerr << message << ": " << "\n\tnever called\n" << std::endl;
            return;
        }

        std::ostringstream os;
        os << message << ": " << "\n\ttotal time: "
           << std::chrono::duration_cast<std::chrono::milliseconds>(value).count()
           << " ms" << "\n\taverage time of one execution: "
           << std::chrono::duration_cast<std::chrono::milliseconds>(value).count() / count << " ms" << std::endl;
        std::cerr << os.str() << std::endl;
    }
};

class AddDuration {
public:
    explicit AddDuration(TotalDuration &dest)
        : total_dur(dest), start(std::chrono::steady_clock::now()) {
    }

    ~AddDuration() {
        std::lock_guard<std::mutex> lock(total_dur.m);
        total_dur.value += std::chrono::steady_clock::now() - start;
        total_dur.count++;
    }

private:
    TotalDuration & total_dur;
    std::chrono::steady_clock::time_point start;
};

#define UNIQ_ID_IMPL_2(lineno) _a_local_var_##lineno
#define UNIQ_ID_IMPL(lineno) UNIQ_ID_IMPL_2(lineno)
#define UNIQ_ID UNIQ_ID_IMPL(__LINE__)

#define ADD_DURATION(value) \
AddDuration UNIQ_ID{value};

#endif // CPPTORRENT_PROFILE_H
