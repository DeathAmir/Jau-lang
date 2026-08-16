cmake_minimum_required(VERSION 3.20)
project(JauAnalyzer VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../../packages/JGui ${CMAKE_BINARY_DIR}/jgui)

add_executable(jau-analyzer main.cpp)
target_include_directories(jau-analyzer PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../../packages/JGui/native)
target_link_libraries(jau-analyzer PRIVATE JGui)

if(MSVC)
  target_compile_options(jau-analyzer PRIVATE /W4 /EHsc)
else()
  target_compile_options(jau-analyzer PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(UNIX AND NOT APPLE)
  set_target_properties(jau-analyzer PROPERTIES BUILD_RPATH "$ORIGIN")
endif()

add_custom_command(TARGET jau-analyzer POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:JGui> $<TARGET_FILE_DIR:jau-analyzer>
  VERBATIM
)
