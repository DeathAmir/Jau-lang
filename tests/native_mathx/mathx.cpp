#include <cstdint>

extern "C" std::intptr_t mx_add(std::intptr_t a, std::intptr_t b) {
    return a + b;
}

extern "C" std::intptr_t mx_mul(std::intptr_t a, std::intptr_t b) {
    return a * b;
}

extern "C" std::intptr_t mx_pow(std::intptr_t base, std::intptr_t exp) {
    std::intptr_t r = 1;
    for (std::intptr_t i = 0; i < exp; ++i) r *= base;
    return r;
}
