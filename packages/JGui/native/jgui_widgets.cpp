#include "jgui_api.hpp"
#include "imgui.h"

JGUI_API std::intptr_t jgui_begin_window(const char* title) {
    if (!ImGui::GetCurrentContext() || !title) return 0;
    return ImGui::Begin(title) ? 1 : 0;
}

JGUI_API std::intptr_t jgui_end_window() {
    if (!ImGui::GetCurrentContext()) return 0;
    ImGui::End();
    return 1;
}

JGUI_API std::intptr_t jgui_text(const char* text) {
    if (!ImGui::GetCurrentContext()) return 0;
    ImGui::TextUnformatted(text ? text : "");
    return 1;
}

JGUI_API std::intptr_t jgui_button(const char* label) {
    if (!ImGui::GetCurrentContext() || !label) return 0;
    return ImGui::Button(label) ? 1 : 0;
}

JGUI_API std::intptr_t jgui_checkbox(const char* label, std::intptr_t value) {
    if (!ImGui::GetCurrentContext() || !label) return value ? 1 : 0;
    bool checked = value != 0;
    ImGui::Checkbox(label, &checked);
    return checked ? 1 : 0;
}

JGUI_API std::intptr_t jgui_slider_int(const char* label, std::intptr_t value, std::intptr_t minimum, std::intptr_t maximum) {
    if (!ImGui::GetCurrentContext() || !label) return value;
    int current = static_cast<int>(value);
    int min_value = static_cast<int>(minimum);
    int max_value = static_cast<int>(maximum);
    if (min_value > max_value) {
        const int tmp = min_value;
        min_value = max_value;
        max_value = tmp;
    }
    ImGui::SliderInt(label, &current, min_value, max_value);
    return static_cast<std::intptr_t>(current);
}

JGUI_API std::intptr_t jgui_same_line() {
    if (!ImGui::GetCurrentContext()) return 0;
    ImGui::SameLine();
    return 1;
}

JGUI_API std::intptr_t jgui_separator() {
    if (!ImGui::GetCurrentContext()) return 0;
    ImGui::Separator();
    return 1;
}

JGUI_API std::intptr_t jgui_demo_window() {
    if (!ImGui::GetCurrentContext()) return 0;
    ImGui::ShowDemoWindow();
    return 1;
}
