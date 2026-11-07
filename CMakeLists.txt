cmake_minimum_required(VERSION 3.20)
project(veksel CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# PortAudio from the submodule
set(PA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(PA_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
add_subdirectory(portaudio)

add_executable(veksel src/main.cpp)

target_include_directories(veksel PRIVATE portaudio/include)
target_link_libraries(veksel PRIVATE portaudio dl pthread)

if (APPLE)
    target_link_libraries(veksel PRIVATE
        "-framework CoreAudio"
        "-framework CoreFoundation"
        "-framework AudioToolbox"
        "-framework AudioUnit"
    )
    target_link_options(veksel PRIVATE -Wl,-export_dynamic)
else()
    target_link_options(veksel PRIVATE -rdynamic)
endif()
