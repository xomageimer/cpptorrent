#include "random_generator.h"

random_generator &random_generator::Random() {
    static random_generator rg;
    return rg;
}

random_generator::random_generator(): rd(), mers_gen(rd()) {}