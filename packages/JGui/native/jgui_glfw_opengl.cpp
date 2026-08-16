#include "jgui_api.hpp"
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>

static GLFWwindow* g_window = nullptr;
static bool g_glfw_initialized = false;

JGUI_API std::intptr_t jgui_create(const char* title, std::intptr_t width, std::intptr_t height) {
    if (g_window) return 1;
    if (!glfwInit()) return 0;
    g_glfw_initialized = true;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    const int w = width > 0 ? static_cast<int>(width) : 960;
    const int h = height > 0 ? static_cast<int>(height) : 640;
    g_window = glfwCreateWindow(w, h, title ? title : "JGui", nullptr, nullptr);
    if (!g_window) {
        glfwTerminate();
        g_glfw_initialized = false;
        return 0;
    }

    glfwMakeContextCurrent(g_window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(g_window, true);
    ImGui_ImplOpenGL3_Init("#version 130");
    return 1;
}

JGUI_API std::intptr_t jgui_begin_frame() {
    if (!g_window || !ImGui::GetCurrentContext()) return 0;
    glfwPollEvents();
    if (glfwWindowShouldClose(g_window)) return 0;
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    return 1;
}

JGUI_API std::intptr_t jgui_render() {
    if (!g_window || !ImGui::GetCurrentContext()) return 0;
    ImGui::Render();
    int display_width = 0, display_height = 0;
    glfwGetFramebufferSize(g_window, &display_width, &display_height);
    glViewport(0, 0, display_width, display_height);
    glClearColor(0.055f, 0.060f, 0.070f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(g_window);
    return 1;
}

JGUI_API std::intptr_t jgui_destroy() {
    if (ImGui::GetCurrentContext()) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }
    if (g_window) {
        glfwDestroyWindow(g_window);
        g_window = nullptr;
    }
    if (g_glfw_initialized) {
        glfwTerminate();
        g_glfw_initialized = false;
    }
    return 1;
}
