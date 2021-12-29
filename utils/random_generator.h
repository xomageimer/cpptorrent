#ifndef CPPTORRENT_RANDOM_GENERATOR_H
#define CPPTORRENT_RANDOM_GENERATOR_H

#include <type_traits>
#include <random>

struct random_generator {
public:
    static random_generator & Random();
    template <typename T>
    T GetNumber(){
        return GetNumberBetween<T>(std::numeric_limits<T>::min(), std::numeric_limits<T>::max());
    }
    template <typename T>
    T GetNumberBetween(T lower_bound, T upper_bound){
        if constexpr(std::is_integral_v<T>){
            static std::uniform_int_distribution<T> distr(lower_bound, upper_bound);
            return distr(mers_gen);
        } else if (std::is_floating_point_v<T>){
            static std::uniform_real_distribution<T> distr(lower_bound, upper_bound);
            return distr(mers_gen);
        }
    }
private:
    random_generator();
    std::random_device rd;
    std::mt19937_64 mers_gen;
};


#endif //CPPTORRENT_RANDOM_GENERATOR_H
