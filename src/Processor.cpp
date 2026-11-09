#include "Processor.h"
#include "Editor.h"

#include <cmath>

namespace
{
    const char* DEFAULT_SOURCE =
        "veksel_main\n"
        "{\n"
        "   return input;\n"
        "}\n";
}

VekselProcessor::VekselProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput ("Input",  juce::AudioChannelSet::mono(),   true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    compiler.startThread();
    compile(DEFAULT_SOURCE);
}

VekselProcessor::~VekselProcessor()
{
    // join before `slots` is destroyed
    compiler.signalThreadShouldExit();
    compiler.notify();
    compiler.stopThread(-1);
}

void VekselProcessor::compile(const juce::String& s)
{
    source = s;
    compiler.set_code(s);
}

bool VekselProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet()  == juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void VekselProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals no_denormals;

    // swap in a pending plugin if the previous retiree has been collected
    if (slots.to_be_deleted.load() == nullptr)
    {
        if (Plugin* new_plugin = slots.pending.exchange(nullptr))
        {
            Plugin* old_plugin = slots.current;
            slots.current = new_plugin;
            if (old_plugin)
                slots.to_be_deleted.store(old_plugin);
        }
    }

    const int n = buffer.getNumSamples();
    if (buffer.getNumChannels() == 0 || n == 0)
        return;

    float* ch0 = buffer.getWritePointer(0);

    // no input bus: channel 0 holds garbage
    if (getTotalNumInputChannels() == 0)
        juce::FloatVectorOperations::clear(ch0, n);

    Plugin* plugin = slots.current;

    for (int i = 0; i < n; ++i)
    {
        float sample = ch0[i];

        if (plugin)
            sample = plugin->function(sample);

        if (! std::isfinite(sample)) sample = 0.0f;
        ch0[i] = juce::jlimit(-1.0f, 1.0f, sample);
    }

    // mono -> every output channel
    for (int ch = 1; ch < buffer.getNumChannels(); ++ch)
        buffer.copyFrom(ch, 0, ch0, n);
}

juce::AudioProcessorEditor* VekselProcessor::createEditor()
{
    return new VekselEditor(*this);
}
