#include <windows.h>
#include <cstdint>

extern "C" std::intptr_t jau_message_box(const char* text, const char* title) {
    return static_cast<std::intptr_t>(MessageBoxA(nullptr, text, title, MB_OK | MB_ICONINFORMATION));
}
