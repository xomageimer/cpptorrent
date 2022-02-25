#ifndef CPPTORRENT_RANDOM_GENERATOR_H
#define CPPTORRENT_RANDOM_GENERATOR_H

#include <type_traits>
#include <random>
#include <mutex>

struct random_generator : public std::mt19937_64 {
public:
    static random_generator & Random();
    template <typename T>
    T GetNumber(){
        std::lock_guard lock(m);
        return GetNumberBetween<T>(std::numeric_limits<T>::min(), std::numeric_limits<T>::max());
    }
    template <typename T>
    T GetNumberBetween(T lower_bound, T upper_bound){
        std::lock_guard lock(m);
        if constexpr(std::is_integral_v<T>){
            std::uniform_int_distribution<T> distr(lower_bound, upper_bound);
            return distr(*this);
        } else if (std::is_floating_point_v<T>){
            std::uniform_real_distribution<T> distr(lower_bound, upper_bound);
            return distr(*this);
        }
    }
private:
    random_generator();
    std::recursive_mutex m;
};


#endif //CPPTORRENT_RANDOM_GENERATOR_H
