cmake_minimum_required(VERSION 3.20)
project(veksel CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(glfw3 REQUIRED)

set(IMGUI ${CMAKE_SOURCE_DIR}/imgui)

# PortAudio from the submodule
set(PA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(PA_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
add_subdirectory(portaudio)

add_executable(veksel
    src/main.cpp
    src/platform.cpp
    ${IMGUI}/imgui.cpp
    ${IMGUI}/imgui_draw.cpp
    ${IMGUI}/imgui_tables.cpp
    ${IMGUI}/imgui_widgets.cpp
    ${IMGUI}/backends/imgui_impl_glfw.cpp
    ${IMGUI}/backends/imgui_impl_opengl3.cpp
)

target_include_directories(veksel
    PRIVATE
    portaudio/include
    ${IMGUI}
    ${IMGUI}/backends
)

target_compile_definitions(veksel PRIVATE GL_SILENCE_DEPRECATION)

target_link_libraries(veksel PRIVATE
    glfw
    portaudio
    "-framework OpenGL"
)
