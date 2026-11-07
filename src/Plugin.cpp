#include "Plugin.h"

Plugin::Plugin(MonoFn m, StereoFn s, void *h)
    : mono_(m), stereo_(s), handle_(h)
{

}

Plugin::~Plugin()
{
    if (handle_)
    {
        dlclose(handle_);
    }
}

PluginPtr load_plugin(const char *path)
{
    void *h = dlopen(path, RTLD_NOW);
    if (!h)
    {
        return nullptr;
    }

    auto p = std::make_unique<Plugin>(
        reinterpret_cast<MonoFn>(dlsym(h, "veksel_process_mono")),
        reinterpret_cast<StereoFn>(dlsym(h, "veksel_process_stereo")),
        h
    );

    if (!p->valid())
    {
        return nullptr;
    }
    return p;
}
