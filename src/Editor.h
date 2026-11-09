#pragma once

#include "Processor.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>

class VekselEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit VekselEditor(VekselProcessor& p);
    ~VekselEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;

    VekselProcessor&             proc;

    juce::CodeDocument           codeDocument;
    juce::CPlusPlusCodeTokeniser codeTokeniser;
    juce::CodeEditorComponent    codeEditor{ codeDocument, &codeTokeniser };
    juce::TextButton             compileButton;
    juce::TextEditor             logText;

    juce::Label                  param_labels[6];
    juce::Slider                 param_sliders[6];
    juce::Rectangle<int>         header_bounds;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VekselEditor)
};
