cmake_minimum_required(VERSION 3.16)
project(veksel LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(SRC_DIR        "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(IMGUI_DIR      "${CMAKE_CURRENT_SOURCE_DIR}/imgui")
set(TEXTEDITOR_DIR "${CMAKE_CURRENT_SOURCE_DIR}/ImGuiColorTextEdit")
set(GLFW_DIR       "${CMAKE_CURRENT_SOURCE_DIR}/glfw")
set(PORTAUDIO_DIR  "${CMAKE_CURRENT_SOURCE_DIR}/portaudio")
set(TINYCC_SOURCE  "${CMAKE_CURRENT_SOURCE_DIR}/tinycc")
set(TINYCC_BUILD   "${CMAKE_CURRENT_BINARY_DIR}/tinycc")

find_package(OpenGL REQUIRED)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(${GLFW_DIR} EXCLUDE_FROM_ALL)

set(PA_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(PA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(PA_USE_ALSA ON CACHE BOOL "" FORCE)
set(PA_USE_PULSEAUDIO ON CACHE BOOL "" FORCE)
set(PA_USE_JACK OFF CACHE BOOL "" FORCE)
add_subdirectory(${PORTAUDIO_DIR} EXCLUDE_FROM_ALL)

target_compile_options(portaudio PRIVATE -Wno-deprecated-declarations)

add_library(imgui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC ${IMGUI_DIR} ${IMGUI_DIR}/backends)
target_link_libraries(imgui PUBLIC glfw OpenGL::GL)

add_library(texteditor STATIC ${TEXTEDITOR_DIR}/TextEditor.cpp)
target_include_directories(texteditor PUBLIC ${TEXTEDITOR_DIR})
target_link_libraries(texteditor PUBLIC imgui)

find_program(TINYCC_MAKE NAMES gmake make REQUIRED)
file(MAKE_DIRECTORY "${TINYCC_BUILD}")

execute_process(
    COMMAND "${TINYCC_SOURCE}/configure" "--source-path=${TINYCC_SOURCE}" --enable-static --config-bcheck=no
    WORKING_DIRECTORY "${TINYCC_BUILD}"
    RESULT_VARIABLE TINYCC_CONFIGURE_RESULT
)
if(NOT TINYCC_CONFIGURE_RESULT EQUAL 0)
    message(FATAL_ERROR "TinyCC configure failed")
endif()

set(TINYCC_LIB "${TINYCC_BUILD}/libtcc.a")

add_custom_command(
    OUTPUT "${TINYCC_LIB}"
    COMMAND "${TINYCC_MAKE}" -j
    WORKING_DIRECTORY "${TINYCC_BUILD}"
    DEPENDS "${TINYCC_BUILD}/config.mak"
    VERBATIM
)
add_custom_target(tinycc_build DEPENDS "${TINYCC_LIB}")

add_library(tinycc STATIC IMPORTED GLOBAL)
set_target_properties(tinycc PROPERTIES IMPORTED_LOCATION "${TINYCC_LIB}")

configure_file("${TINYCC_SOURCE}/libtcc.h" "${TINYCC_BUILD}/libtcc.h" COPYONLY)

add_executable(veksel
    ${SRC_DIR}/main.cpp
    ${SRC_DIR}/platform.cpp
    ${SRC_DIR}/UI.cpp
    ${SRC_DIR}/VekselEngine.cpp
    ${SRC_DIR}/Audio.cpp
)
add_dependencies(veksel tinycc_build)
target_compile_definitions(veksel PRIVATE GL_SILENCE_DEPRECATION VEKSEL_TCC_LIB_PATH="${TINYCC_BUILD}")
target_include_directories(veksel PRIVATE
    ${SRC_DIR}
    ${TINYCC_BUILD}
)

target_link_libraries(veksel PRIVATE
    imgui
    texteditor
    portaudio
    tinycc
    ${CMAKE_DL_LIBS}
)

if(UNIX AND NOT APPLE)
    target_link_libraries(veksel PRIVATE pthread m)
endif()

if(APPLE)
    target_link_libraries(veksel PRIVATE
        "-framework Cocoa"
        "-framework IOKit"
        "-framework CoreFoundation"
        "-framework CoreAudio"
        "-framework AudioToolbox"
        "-framework AudioUnit"
    )
endif()
