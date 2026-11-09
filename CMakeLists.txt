cmake_minimum_required(VERSION 3.22)
project(Veksel VERSION 1.0.0)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(JUCE)

juce_add_gui_app(Veksel
    PRODUCT_NAME "Veksel"
    COMPANY_NAME "VekselAudio"
    BUNDLE_ID "com.vekselaudio.veksel"
    GENERATE_JUCE_HEADER ON
)

# Include your source files
target_sources(Veksel PRIVATE
    src/Main.cpp
)

target_link_libraries(Veksel PRIVATE
    juce::juce_gui_extra
    juce::juce_audio_utils
    juce::juce_dsp
)

# Recommended JUCE compile definitions
target_compile_definitions(Veksel PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)
