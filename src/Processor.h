#pragma once

#include "Compiler.h"
#include "PluginSlots.h"

#include <juce_audio_processors/juce_audio_processors.h>

// the standalone wrapper supplies the window, Options button,
// audio device settings and input mute around this
class VekselProcessor : public juce::AudioProcessor
{
public:
    VekselProcessor();
    ~VekselProcessor() override;

    // message thread
    void compile(const juce::String& s);
    const juce::String& get_source() const { return source; }

    // audio thread
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void prepareToPlay(double, int) override {}
    void releaseResources() override {}

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Veksel"; }
    bool   acceptsMidi() const override          { return false; }
    bool   producesMidi() const override         { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override                             { return 1; }
    int getCurrentProgram() override                          { return 0; }
    void setCurrentProgram(int) override                      {}
    const juce::String getProgramName(int) override           { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock&) override     {}
    void setStateInformation(const void*, int) override       {}

private:
    PluginSlots    slots;                 // declared before compiler: outlives it
    CompilerThread compiler{ slots };
    juce::String   source;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VekselProcessor)
};
