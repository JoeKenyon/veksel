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

set(SRC_DIR        "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(IMGUI_DIR      "${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui")
set(TEXTEDITOR_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/ImGuiColorTextEdit")
set(GLFW_DIR       "${CMAKE_CURRENT_SOURCE_DIR}/third_party/glfw")
set(PORTAUDIO_DIR  "${CMAKE_CURRENT_SOURCE_DIR}/third_party/portaudio")
set(TINYCC_DIR     "${CMAKE_CURRENT_SOURCE_DIR}/third_party/tinycc")
set(TINYCC_INC_DIR "${CMAKE_CURRENT_BINARY_DIR}/tinycc_inc")

# setup glfw
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
add_subdirectory("${GLFW_DIR}" EXCLUDE_FROM_ALL)

# setup opengl and threads
find_package(OpenGL REQUIRED)
find_package(Threads REQUIRED)

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
    if(MSVC)
        target_compile_options(portaudio PRIVATE /wd4996)
    else()
        target_compile_options(portaudio PRIVATE -Wno-deprecated-declarations)
    endif()
endif()

# setup tinycc natively via cmake
file(MAKE_DIRECTORY "${TINYCC_INC_DIR}")
configure_file("${TINYCC_DIR}/libtcc.h" "${TINYCC_INC_DIR}/libtcc.h" COPYONLY)

if(WIN32)
    find_program(TINYCC_SH NAMES bash sh REQUIRED)
    file(TO_CMAKE_PATH "${TINYCC_DIR}" TINYCC_DIR_POSIX)
    file(TO_CMAKE_PATH "${TINYCC_INC_DIR}" TINYCC_INC_POSIX)
    string(REGEX REPLACE "^([A-Za-z]):/" "/\\1/" TINYCC_DIR_MSYS "${TINYCC_DIR_POSIX}")
    string(TOLOWER "${TINYCC_DIR_MSYS}" TINYCC_DIR_MSYS)
    string(REGEX REPLACE "^([A-Za-z]):/" "/\\1/" TINYCC_INC_MSYS "${TINYCC_INC_POSIX}")
    string(TOLOWER "${TINYCC_INC_MSYS}" TINYCC_INC_MSYS)

    execute_process(
        COMMAND "${TINYCC_SH}" "${TINYCC_DIR_MSYS}/configure"
            "--source-path=${TINYCC_DIR_MSYS}"
            "--prefix=${TINYCC_INC_MSYS}"
            --config-bcheck=no
        WORKING_DIRECTORY "${TINYCC_INC_DIR}"
        RESULT_VARIABLE TINYCC_CONFIGURE_RESULT
    )
else()
    execute_process(
        COMMAND "${TINYCC_DIR}/configure"
            "--source-path=${TINYCC_DIR}"
            "--prefix=${TINYCC_INC_DIR}"
            --config-bcheck=no
        WORKING_DIRECTORY "${TINYCC_INC_DIR}"
        RESULT_VARIABLE TINYCC_CONFIGURE_RESULT
    )
endif()

if(NOT TINYCC_CONFIGURE_RESULT EQUAL 0)
    message(FATAL_ERROR "TinyCC configure failed")
endif()

set(TINYCC_TARGET_DEFS "")
if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64|ARM64")
    list(APPEND TINYCC_TARGET_DEFS TCC_TARGET_ARM64=1)
elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
    list(APPEND TINYCC_TARGET_DEFS TCC_TARGET_X86_64=1)
else()
    list(APPEND TINYCC_TARGET_DEFS TCC_TARGET_I386=1)
endif()

if(APPLE)
    list(APPEND TINYCC_TARGET_DEFS TCC_TARGET_MACHO=1)
elseif(WIN32)
    list(APPEND TINYCC_TARGET_DEFS TCC_TARGET_PE=1)
endif()

add_executable(tinycc_c2str "${TINYCC_DIR}/conftest.c")
target_include_directories(tinycc_c2str PRIVATE
    "${TINYCC_INC_DIR}"
    "${TINYCC_DIR}"
)
target_compile_definitions(tinycc_c2str PRIVATE
    C2STR=1
    ${TINYCC_TARGET_DEFS}
)
if(NOT MSVC)
    target_compile_options(tinycc_c2str PRIVATE -w)
endif()

set(TCCDEFS_GENERATED "${TINYCC_INC_DIR}/tccdefs_.h")
add_custom_command(
    OUTPUT "${TCCDEFS_GENERATED}"
    COMMAND tinycc_c2str "${TINYCC_DIR}/include/tccdefs.h" "${TCCDEFS_GENERATED}"
    DEPENDS tinycc_c2str "${TINYCC_DIR}/include/tccdefs.h"
    VERBATIM
)

add_library(tinycc STATIC
    "${TINYCC_DIR}/libtcc.c"
    "${TCCDEFS_GENERATED}"
)

target_include_directories(tinycc
    PUBLIC "${TINYCC_INC_DIR}"
    PRIVATE "${TINYCC_DIR}"
)

target_compile_definitions(tinycc PRIVATE
    ONE_SOURCE=1
    ${TINYCC_TARGET_DEFS}
)

if(NOT MSVC)
    target_compile_options(tinycc PRIVATE -w)
endif()

# core library
add_library(veksel_core
    "${SRC_DIR}/Log.cpp"
    "${SRC_DIR}/Engine.cpp"
    "${SRC_DIR}/Audio.cpp"
    "${SRC_DIR}/Builtins.cpp"
)

target_include_directories(veksel_core
    PUBLIC "${SRC_DIR}"
    PRIVATE "${TINYCC_INC_DIR}"
)

target_link_libraries(veksel_core
    PUBLIC portaudio
    PRIVATE
        tinycc
        ${CMAKE_DL_LIBS}
        Threads::Threads
)

if(UNIX AND NOT APPLE)
    target_link_libraries(veksel_core PRIVATE m)
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

file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/presets"
     DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")

target_link_libraries(veksel PRIVATE
    veksel_core
    veksel_imgui
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