// veksel_test.cpp — pre-flight sandbox for Veksel plugins.
// Usage: ./veksel_test /path/to/plugin.dylib

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <ctime>
#include <cstring>
#include <dlfcn.h>

struct Stereo { float left; float right; };
using plugin_mono_func = float (*)(float);
using plugin_stereo_func = Stereo (*)(Stereo);

extern "C" {
    float veksel_get_knob(int index) { (void)index; return 0.5f; }
    int veksel_get_switch(int index) { (void)index; return 0; }
    float veksel_get_sample_rate() { return 44100.0f; }
    float veksel_get_time() { return 0.0f; }
}

static bool valid(float x) {
    return std::isfinite(x) && x >= -1.0f && x <= 1.0f;
}

int main(int argc, char** argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s <plugin.dylib>\n", argv[0]);
        return 2;
    }

    void* h = dlopen(argv[1], RTLD_NOW);
    if (!h) {
        fprintf(stderr, "[test] dlopen: %s\n", dlerror());
        return 3;
    }

    auto mono   = reinterpret_cast<plugin_mono_func>(dlsym(h, "veksel_process_mono"));
    auto stereo = reinterpret_cast<plugin_stereo_func>(dlsym(h, "veksel_process_stereo"));

    if (!mono && !stereo) {
        fprintf(stderr, "[test] no entry point\n");
        dlclose(h);
        return 4;
    }

    const float values[] = { 0.0f, 1.0f, -1.0f, 0.5f, -0.5f, 0.0001f, -0.0001f };
    const int NUM_BOUNDARY = sizeof(values) / sizeof(values[0]);

    srand((unsigned)time(nullptr));

    for (int i = 0; i < 10000; i++) {
        float x = (i < NUM_BOUNDARY) ? values[i]
                                     : ((float)rand() / RAND_MAX) * 2.0f - 1.0f;

        if (mono) {
            float result = mono(x);
            if (!valid(result)) {
                fprintf(stderr, "[test] mono invalid output (%f) for input %f\n", result, x);
                dlclose(h);
                return 5;
            }
        }
        if (stereo) {
            Stereo in = {x, -x};
            Stereo out = stereo(in);
            if (!valid(out.left) || !valid(out.right)) {
                fprintf(stderr, "[test] stereo invalid output for input %f\n", x);
                dlclose(h);
                return 6;
            }
        }
    }

    dlclose(h);
    printf("[test] plugin passed\n");
    return 0;
}
