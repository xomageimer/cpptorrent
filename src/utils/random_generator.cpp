#include "random_generator.h"

random_generator &random_generator::Random() {
    static random_generator rg;
    return rg;
}

random_generator::random_generator(): std::mt19937_64(std::random_device{}()) {}