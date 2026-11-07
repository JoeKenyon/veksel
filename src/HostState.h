#pragma once

#include <atomic>

// Knobs, switches and sample rate. Main thread writes, audio thread reads.
// Exposed to plugins via the extern "C" accessors declared below.
namespace HostState
{
    extern std::atomic<float>  knobs[12];
    extern std::atomic<int>    switches[3];
    extern std::atomic<double> sample_rate;

    void reset_defaults();

}  // namespace HostState

// Plugins call these. extern "C" so the names aren't mangled.
extern "C"
{
    float  veksel_get_knob(int i);
    int    veksel_get_switch(int i);
    double veksel_get_sample_rate();
}
