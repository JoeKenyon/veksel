cmake_minimum_required(VERSION 3.22)
project(Veksel VERSION 1.0.0)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(JUCE)

juce_add_plugin(Veksel
    FORMATS Standalone
    PRODUCT_NAME "Veksel"
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "Veksel needs audio input")

target_sources(Veksel PRIVATE
    src/Main.cpp
    src/Processor.cpp
    src/Editor.cpp
    src/Compiler.cpp
    src/Log.cpp
)

target_link_libraries(Veksel PRIVATE
    juce::juce_audio_utils
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
)

# Recommended JUCE compile definitions
target_compile_definitions(Veksel PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)
