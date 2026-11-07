#pragma once

#include <dlfcn.h>
#include <memory>

// A loaded plugin. Owns its dlopen handle. Non-copyable, move-only.
// Construction should go through dlsym + the Plugin ctor below;
// destruction is handled by ~Plugin, which dlcloses.

struct Stereo { float left, right; };
using MonoFn   = float  (*)(float);
using StereoFn = Stereo (*)(Stereo);

class Plugin
{
public:
    Plugin(MonoFn m, StereoFn s, void *h);
    ~Plugin();

    Plugin(const Plugin&)            = delete;
    Plugin& operator=(const Plugin&) = delete;
    Plugin(Plugin&&)                 = default;
    Plugin& operator=(Plugin&&)      = default;

    bool        valid()  const { return mono_ || stereo_; }
    MonoFn      mono()   const { return mono_; }
    StereoFn    stereo() const { return stereo_; }





private:
    MonoFn   mono_   = nullptr;
    StereoFn stereo_ = nullptr;
    void    *handle_ = nullptr;
};

using PluginPtr = std::unique_ptr<Plugin>;

// Build a Plugin from a .dylib path. Returns nullptr if the file can't be
// loaded or doesn't export either entry point.
PluginPtr load_plugin(const char *path);
