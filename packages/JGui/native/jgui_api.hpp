#pragma once
#include <cstdint>

#if defined(_WIN32)
#define JGUI_API extern "C" __declspec(dllexport)
#else
#define JGUI_API extern "C" __attribute__((visibility("default")))
#endif

JGUI_API std::intptr_t jgui_create(const char* title, std::intptr_t width, std::intptr_t height);
JGUI_API std::intptr_t jgui_begin_frame();
JGUI_API std::intptr_t jgui_render();
JGUI_API std::intptr_t jgui_destroy();

JGUI_API std::intptr_t jgui_begin_window(const char* title);
JGUI_API std::intptr_t jgui_end_window();
JGUI_API std::intptr_t jgui_text(const char* text);
JGUI_API std::intptr_t jgui_button(const char* label);
JGUI_API std::intptr_t jgui_checkbox(const char* label, std::intptr_t value);
JGUI_API std::intptr_t jgui_slider_int(const char* label, std::intptr_t value, std::intptr_t minimum, std::intptr_t maximum);
JGUI_API std::intptr_t jgui_same_line();
JGUI_API std::intptr_t jgui_separator();
JGUI_API std::intptr_t jgui_demo_window();
