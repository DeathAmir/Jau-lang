#include <cstdint>

#ifdef _WIN32
#define JAU_FFI_EXPORT extern "C" __declspec(dllexport)
#else
#define JAU_FFI_EXPORT extern "C" __attribute__((visibility("default")))
#endif

JAU_FFI_EXPORT std::intptr_t vm_add(std::intptr_t a, std::intptr_t b) {
    return a + b;
}

JAU_FFI_EXPORT std::intptr_t vm_mul(std::intptr_t a, std::intptr_t b) {
    return a * b;
}
