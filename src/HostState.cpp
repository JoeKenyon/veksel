#include "HostState.h"

namespace HostState
{

    std::atomic<float>  knobs[12];
    std::atomic<int>    switches[3];
    std::atomic<double> sample_rate{44100.0}; // default sr

    void reset_defaults()
    {
        for (auto &k : knobs) k.store(0.5f);
        for (auto &s : switches) s.store(0);
    }

}  // namespace HostState

extern "C"
{

    float veksel_get_knob(int i)
    {
        return (i >= 0 && i < 12) ? HostState::knobs[i].load() : 0.0f;
    }

    int veksel_get_switch(int i)
    {
        return (i >= 0 && i < 3) ? HostState::switches[i].load() : 0;
    }

    double veksel_get_sample_rate()
    {
        return HostState::sample_rate.load();
    }

}  // extern "C"
