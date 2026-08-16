cmake_minimum_required(VERSION 3.20)
project(JGui VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(FetchContent)
FetchContent_Declare(
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG v1.92.9
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(imgui)

set(JGUI_IMGUI_SOURCES
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/imgui_demo.cpp
)

if(WIN32)
  add_library(JGui SHARED
    native/jgui_widgets.cpp
    native/jgui_win32_dx11.cpp
    ${JGUI_IMGUI_SOURCES}
    ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_dx11.cpp
  )
  target_compile_definitions(JGui PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
  target_link_libraries(JGui PRIVATE d3d11 dxgi)
else()
  set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_X11 ON CACHE BOOL "" FORCE)
  FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
    GIT_SHALLOW TRUE
  )
  FetchContent_MakeAvailable(glfw)
  find_package(OpenGL REQUIRED)
  add_library(JGui SHARED
    native/jgui_widgets.cpp
    native/jgui_glfw_opengl.cpp
    ${JGUI_IMGUI_SOURCES}
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
  )
  target_link_libraries(JGui PRIVATE glfw OpenGL::GL ${CMAKE_DL_LIBS})
endif()

target_include_directories(JGui PRIVATE
  ${imgui_SOURCE_DIR}
  ${imgui_SOURCE_DIR}/backends
  ${CMAKE_CURRENT_SOURCE_DIR}/native
)

if(MSVC)
  target_compile_options(JGui PRIVATE /W4 /EHsc)
else()
  target_compile_options(JGui PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(JGUI_ARCH x86_64)
else()
  set(JGUI_ARCH x86)
endif()
if(WIN32)
  set(JGUI_PLATFORM windows)
else()
  set(JGUI_PLATFORM linux)
endif()

set(JGUI_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/native/bin/${JGUI_PLATFORM}-${JGUI_ARCH}")
set_target_properties(JGui PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${JGUI_OUTPUT_DIRECTORY}"
  RUNTIME_OUTPUT_DIRECTORY_DEBUG "${JGUI_OUTPUT_DIRECTORY}"
  RUNTIME_OUTPUT_DIRECTORY_RELEASE "${JGUI_OUTPUT_DIRECTORY}"
  RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${JGUI_OUTPUT_DIRECTORY}"
  RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${JGUI_OUTPUT_DIRECTORY}"
  LIBRARY_OUTPUT_DIRECTORY "${JGUI_OUTPUT_DIRECTORY}"
  LIBRARY_OUTPUT_DIRECTORY_DEBUG "${JGUI_OUTPUT_DIRECTORY}"
  LIBRARY_OUTPUT_DIRECTORY_RELEASE "${JGUI_OUTPUT_DIRECTORY}"
  LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO "${JGUI_OUTPUT_DIRECTORY}"
  LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL "${JGUI_OUTPUT_DIRECTORY}"
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/archive"
  OUTPUT_NAME "JGui"
)
