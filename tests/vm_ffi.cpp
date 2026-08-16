#include <cstdint>

extern "C" std::intptr_t vm_add(std::intptr_t a, std::intptr_t b) {
    return a + b;
}

extern "C" std::intptr_t vm_mul(std::intptr_t a, std::intptr_t b) {
    return a * b;
}
