#include "Processor.h"

// called by the JUCE plugin client / standalone wrapper
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VekselProcessor();
}
