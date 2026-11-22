cmake_minimum_required(VERSION 3.16)
project(veksel LANGUAGES C CXX)

if(APPLE)
    add_compile_options(-Wno-deprecated-literal-operator)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(SRC_DIR          "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(IMGUI_DIR        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui")
set(TEXTEDITOR_DIR   "${CMAKE_CURRENT_SOURCE_DIR}/third_party/ImGuiColorTextEdit")
set(GLFW_DIR         "${CMAKE_CURRENT_SOURCE_DIR}/third_party/glfw")
set(PORTAUDIO_DIR    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/portaudio")
set(TINYCC_SOURCE    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/tinycc")
set(TINYCC_BUILD     "${CMAKE_CURRENT_BINARY_DIR}/tinycc")

# setup glfw
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
add_subdirectory("${GLFW_DIR}" EXCLUDE_FROM_ALL)

# setup opengl
find_package(OpenGL REQUIRED)

# setup imgui
add_library(veksel_imgui STATIC
    "${IMGUI_DIR}/imgui.cpp"
    "${IMGUI_DIR}/imgui_draw.cpp"
    "${IMGUI_DIR}/imgui_tables.cpp"
    "${IMGUI_DIR}/imgui_widgets.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_glfw.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp"
)

target_include_directories(veksel_imgui PUBLIC
    "${IMGUI_DIR}"
    "${IMGUI_DIR}/backends"
)

target_link_libraries(veksel_imgui PUBLIC
    glfw
    OpenGL::GL
)

# setup portaudio
set(PA_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(PA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(PA_USE_JACK OFF CACHE BOOL "" FORCE)
add_subdirectory("${PORTAUDIO_DIR}" EXCLUDE_FROM_ALL)

if(TARGET portaudio)
    target_compile_options(portaudio PRIVATE -Wno-deprecated-declarations)
endif()

# setup tiny cc (only libtcc.a is needed: plugins are built with -nostdlib)
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
    COMMAND "${TINYCC_MAKE}" -j libtcc.a
    WORKING_DIRECTORY "${TINYCC_BUILD}"
    DEPENDS "${TINYCC_BUILD}/config.mak"
    VERBATIM
)

add_custom_target(tinycc_build DEPENDS "${TINYCC_LIB}")

add_library(tinycc STATIC IMPORTED GLOBAL)
set_target_properties(tinycc PROPERTIES IMPORTED_LOCATION "${TINYCC_LIB}")

configure_file(
    "${TINYCC_SOURCE}/libtcc.h"
    "${TINYCC_BUILD}/libtcc.h"
    COPYONLY
)

# core library
add_library(veksel_core
    "${SRC_DIR}/Log.cpp"
    "${SRC_DIR}/Engine.cpp"
    "${SRC_DIR}/Audio.cpp"
    "${SRC_DIR}/Builtins.cpp"
)

add_dependencies(veksel_core tinycc_build)

target_include_directories(veksel_core PUBLIC
    "${SRC_DIR}"
    "${TINYCC_BUILD}"
)

target_link_libraries(veksel_core PUBLIC
    portaudio
    tinycc
    ${CMAKE_DL_LIBS}
)

if(UNIX AND NOT APPLE)
    target_link_libraries(veksel_core PUBLIC pthread m)
endif()

# main app
add_executable(veksel
    "${SRC_DIR}/main.cpp"
    "${SRC_DIR}/UI.cpp"
    "${SRC_DIR}/Platform.cpp"
    "${SRC_DIR}/Paths.cpp"
    "${TEXTEDITOR_DIR}/TextEditor.cpp"
)

target_include_directories(veksel PRIVATE
    "${SRC_DIR}"
    "${TEXTEDITOR_DIR}"
)

# presets live next to the executable
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/presets DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

target_link_libraries(veksel PRIVATE
    veksel_core
    veksel_imgui
    glfw
    OpenGL::GL
)

if(APPLE)
    target_link_libraries(veksel PRIVATE
        "-framework CoreAudio"
        "-framework AudioToolbox"
        "-framework AudioUnit"
        "-framework CoreFoundation"
    )
endif()

# tests
option(VEKSEL_BUILD_TESTS "Build Veksel tests" ON)

if(VEKSEL_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

install(TARGETS veksel RUNTIME DESTINATION .)
install(DIRECTORY presets DESTINATION .)