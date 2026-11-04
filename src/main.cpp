#include <cfloat>
#include "imgui.h"
#include "platform.h"
#include "portaudio.h"
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>
#include <fstream>
#include <array>
#include <atomic>
#include <mutex>
#include <chrono>
#include <cmath>
#include <dlfcn.h>
#include <unistd.h>

// what the user's code has to export: float process(float)
typedef float (*ProcessFunc)(float);

// passthrough, used until the first successful compile
static float default_process(float sample)
{
    return sample;
}

// audio thread reads this every callback, compile thread swaps it
static std::atomic<ProcessFunc> active_process{default_process};

// only the compile thread touches this (one build at a time), plus main at exit
static void *active_library_handle = nullptr;

static std::atomic<bool> is_compiling{false};
static std::string console_output = "[info] ready.\n[ok] waiting for changes";
static std::mutex console_mutex;

// all the build junk (source + dylibs) goes in here so the project dir stays clean
static std::string work_dir;

static bool make_work_dir()
{
    char tmpl[] = "/tmp/veksel_XXXXXX";
    if (mkdtemp(tmpl) == nullptr) return false;
    work_dir = tmpl;
    return true;
}

static void cleanup_work_dir()
{
    if (work_dir.empty()) return;
    std::remove((work_dir + "/plugin.c").c_str());
    rmdir(work_dir.c_str());   // fails if something's left, that's fine
}

// the one place that writes console_output. don't lock console_mutex
// anywhere else around it or you get the deadlock from before
static void set_console(const std::string &msg)
{
    std::lock_guard<std::mutex> lock(console_mutex);
    console_output = msg;
}

static void compile_code_async(std::string source_code)
{
    // resets the flag no matter which return we hit
    struct Done { ~Done() { is_compiling.store(false); } } done;

    auto t0 = std::chrono::steady_clock::now();

    // new name every build. macOS dlopen will happily hand back the old
    // image if the path matches one that's still loaded
    static int build_id = 0;
    std::string dylib = "plugin_" + std::to_string(build_id++) + ".dylib";

    set_console("[info] compiling...\n");

    // dump the editor text to disk
    {
        std::ofstream out_file(work_dir + "/plugin.c");
        if (!out_file.is_open())
        {
            set_console("[error] couldn't write plugin.c");
            return;
        }
        // #line 1 makes clang's line numbers match the editor
        out_file << "#include <math.h>\n#line 1\n" << source_code << "\n";
    }

    // cd first so errors just say plugin.c instead of the whole /tmp path
    // -O1 is plenty for a live-coding toy, -O3 just makes you wait
    std::string cmd = "cd " + work_dir + " && clang -shared -O1 -fPIC plugin.c -o " + dylib + " 2>&1";

    FILE *pipe = popen(cmd.c_str(), "r");
    if (pipe == nullptr)
    {
        set_console("[error] couldn't launch clang");
        return;
    }

    // slurp clang's output (errors and warnings both land here)
    std::string build_log;
    std::array<char, 128> buffer;
    while (fgets(buffer.data(), (int)buffer.size(), pipe) != nullptr)
        build_log += buffer.data();

    int exit_code = pclose(pipe);
    if (exit_code != 0)
    {
        set_console("[error] compile failed:\n" + build_log);
        return;
    }

    // load it, then delete the file right away. it stays mapped in memory
    // so we never need it on disk again
    std::string dylib_path = work_dir + "/" + dylib;
    void *new_handle = dlopen(dylib_path.c_str(), RTLD_NOW);
    std::remove(dylib_path.c_str());

    if (new_handle == nullptr)
    {
        set_console(std::string("[error] dlopen failed: ") + dlerror());
        return;
    }

    ProcessFunc new_process = (ProcessFunc)dlsym(new_handle, "process");
    if (new_process == nullptr)
    {
        dlclose(new_handle);
        set_console("[error] no 'process' function found");
        return;
    }

    // point the audio thread at the new code first...
    active_process.store(new_process);
    void *old_handle = active_library_handle;
    active_library_handle = new_handle;

    // ...then wait a bit before unloading the old one, in case the audio
    // thread is still halfway through a call into it. 100ms is way more than
    // one 256-frame block takes
    if (old_handle != nullptr)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        dlclose(old_handle);
    }

    double ms = std::chrono::duration<double, std::milli>(
                    std::chrono::steady_clock::now() - t0).count();
    char head[64];
    snprintf(head, sizeof head, "[ok] hot-swapped (%.0f ms)\n", ms);
    set_console(head + build_log);   // build_log might have warnings in it
}

// params shared with the audio thread, atomics so no locks needed
static std::atomic<float> volume{0.5f};
static std::atomic<float> param1{0.5f};
static std::atomic<float> param2{0.5f};
static std::atomic<float> param3{0.5f};
static std::atomic<float> param4{0.5f};
static std::atomic<float> param5{0.5f};

// scope ring buffer. audio thread writes, ui thread reads, nobody else.
// a torn read just glitches the scope for one frame, not worth a lock
struct CircularBuffer
{
    size_t capacity;
    std::vector<float> buffer;
    std::atomic<size_t> write_pos;

    CircularBuffer(size_t cap)
        : capacity(cap), buffer(cap, 0.0f), write_pos(0)
    {
    }

    void push(float sample)
    {
        size_t pos = write_pos.load(std::memory_order_relaxed);
        buffer[pos] = sample;
        // release so the reader sees the sample before the new position
        write_pos.store((pos + 1) % capacity, std::memory_order_release);
    }

    // copies the newest max_samples into dest, oldest first
    size_t read_latest(float *dest, size_t max_samples)
    {
        size_t current_write = write_pos.load(std::memory_order_acquire);
        size_t to_read = (max_samples < capacity) ? max_samples : capacity;

        for (size_t i = 0; i < to_read; ++i)
        {
            size_t idx = (current_write + capacity - to_read + i) % capacity;
            dest[i] = buffer[idx];
        }

        return to_read;
    }
};

static CircularBuffer global_scope(4096);

// what's in the editor on startup
static char code[8192] =
    "float process(float sample)\n"
    "{\n"
    "    // soft clip\n"
    "    float x = sample * 2.0f;\n"
    "    return x / (1.0f + fabsf(x));\n"
    "}\n";

static const char *banner =
    "██╗   ██╗███████╗██╗  ██╗███████╗███████╗██╗     \n"
    "██║   ██║██╔════╝██║ ██╔╝██╔════╝██╔════╝██║     \n"
    "██║   ██║█████╗  █████╔╝ ███████╗█████╗  ██║     \n"
    "╚██╗ ██╔╝██╔══╝  ██╔═██╗ ╚════██║██╔══╝  ██╔═══╝  \n"
    " ╚████╔╝ ███████╗██║  ██╗███████║███████╗███████╗\n"
    "  ╚═══╝  ╚══════╝╚═╝  ╚═╝╚══════╝╚══════╝╚══════╝";

struct AudioDeviceInfo
{
    int id;
    std::string name;
    int max_input_channels;
    int max_output_channels;
};

static int selected_input_id = -1;
static int selected_output_id = -1;
static PaStream *global_stream = nullptr;

// runs on portaudio's realtime thread: no allocs, no locks, no printf
static int audio_callback(const void *inputBuffer, void *outputBuffer,
                          unsigned long framesPerBuffer,
                          const PaStreamCallbackTimeInfo *timeInfo,
                          PaStreamCallbackFlags statusFlags,
                          void *userData)
{
    (void)timeInfo;
    (void)statusFlags;
    (void)userData;

    float *out = (float *)outputBuffer;
    const float *in = (const float *)inputBuffer;

    // grab these once per block instead of per sample
    ProcessFunc current_process = active_process.load();
    float current_vol = volume.load();

    for (unsigned long i = 0; i < framesPerBuffer; i++)
    {
        // no input device means we just feed it silence
        float sample = in ? *in++ : 0.0f;
        float processed = current_process(sample) * current_vol;

        // mono out to both channels
        *out++ = processed;
        *out++ = processed;
        global_scope.push(processed);
    }

    return paContinue;
}

// tear down the old stream and open a new one, used when devices change
static void restart_audio_stream(int input_id, int output_id)
{
    if (global_stream)
    {
        Pa_StopStream(global_stream);
        Pa_CloseStream(global_stream);
        global_stream = nullptr;
    }

    PaStreamParameters inputParameters;
    PaStreamParameters outputParameters;

    // input is optional, plenty of setups have no mic
    inputParameters.device = input_id;
    const PaDeviceInfo *inputInfo = Pa_GetDeviceInfo(input_id);
    bool use_input = (input_id != paNoDevice && inputInfo != nullptr);
    if (use_input)
    {
        inputParameters.channelCount = (inputInfo->maxInputChannels < 2) ? inputInfo->maxInputChannels : 2;
        inputParameters.sampleFormat = paFloat32;
        inputParameters.suggestedLatency = inputInfo->defaultLowInputLatency;
        inputParameters.hostApiSpecificStreamInfo = nullptr;
    }

    // output is not optional, bail if there isn't one
    outputParameters.device = output_id;
    const PaDeviceInfo *outputInfo = Pa_GetDeviceInfo(output_id);
    if (outputInfo == nullptr) return;

    outputParameters.channelCount = (outputInfo->maxOutputChannels < 2) ? outputInfo->maxOutputChannels : 2;
    outputParameters.sampleFormat = paFloat32;
    outputParameters.suggestedLatency = outputInfo->defaultLowOutputLatency;
    outputParameters.hostApiSpecificStreamInfo = nullptr;

    PaError err = Pa_OpenStream(
        &global_stream,
        use_input ? &inputParameters : nullptr,
        &outputParameters,
        44100,
        256,
        paClipOff,
        audio_callback,
        nullptr
    );

    if (err == paNoError)
        Pa_StartStream(global_stream);
    else
        printf("PortAudio restart error: %s\n", Pa_GetErrorText(err));
}

static void draw_ui()
{
    ImGui::Columns(2, "main", true);

    // ---- left column: devices, scope, volume ----
    ImGui::Text("%s", banner);

    ImGui::SeparatorText("Audio Devices");

    // rebuilt every frame, cheap enough for a handful of devices
    std::vector<AudioDeviceInfo> devices;
    int num_devices = Pa_GetDeviceCount();
    for (int i = 0; i < num_devices; ++i)
    {
        const PaDeviceInfo *info = Pa_GetDeviceInfo(i);
        if (info)
            devices.push_back({i, info->name, info->maxInputChannels, info->maxOutputChannels});
    }

    // name shown in the combo while it's closed
    std::string input_name = "None / Default";
    for (const auto &d : devices)
    {
        if (d.id == selected_input_id && d.max_input_channels > 0)
        {
            input_name = d.name;
            break;
        }
    }

    if (ImGui::BeginCombo("Input Device", input_name.c_str()))
    {
        for (const auto &d : devices)
        {
            // skip output-only devices
            if (d.max_input_channels <= 0) continue;

            bool is_selected = (selected_input_id == d.id);
            if (ImGui::Selectable(d.name.c_str(), is_selected))
            {
                selected_input_id = d.id;
                restart_audio_stream(selected_input_id, selected_output_id);
            }
            if (is_selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    std::string output_name = "None / Default";
    for (const auto &d : devices)
    {
        if (d.id == selected_output_id && d.max_output_channels > 0)
        {
            output_name = d.name;
            break;
        }
    }

    if (ImGui::BeginCombo("Output Device", output_name.c_str()))
    {
        for (const auto &d : devices)
        {
            // skip input-only devices
            if (d.max_output_channels <= 0) continue;

            bool is_selected = (selected_output_id == d.id);
            if (ImGui::Selectable(d.name.c_str(), is_selected))
            {
                selected_output_id = d.id;
                restart_audio_stream(selected_input_id, selected_output_id);
            }
            if (is_selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    ImGui::SeparatorText("Visualizer");

    // pull the latest 512 samples out of the ring buffer for the scope
    static std::vector<float> waveform_data(512, 0.0f);
    size_t samples_read = global_scope.read_latest(waveform_data.data(), waveform_data.size());

    // fixed -1..1 range so quiet signals actually look quiet
    ImGui::PlotLines("##waveform", waveform_data.data(), (int)samples_read, 0,
                     "live output", -1.0f, 1.0f, ImVec2(-1, 100));

    ImGui::SeparatorText("Controls");

    // imgui needs a plain float to edit, so copy out, edit, store back
    float current_vol = volume.load();
    if (ImGui::SliderFloat("Volume", &current_vol, 0.0f, 1.0f))
        volume.store(current_vol);

    // ---- right column: editor + console ----
    ImGui::NextColumn();

    ImGui::SeparatorText("Editor");

    // editor takes whatever space is left after the button and console
    const float console_h = 140.0f;
    const ImGuiStyle &st = ImGui::GetStyle();
    const float below = console_h + ImGui::GetFrameHeight() + st.ItemSpacing.y * 2;

    ImGui::InputTextMultiline("##code", code, sizeof code,
                              ImVec2(-FLT_MIN, -below),
                              ImGuiInputTextFlags_AllowTabInput);

    // greyed out while a build is running
    ImGui::BeginDisabled(is_compiling.load());
    if (ImGui::Button("Compile", ImVec2(-FLT_MIN, 0)))
    {
        // set the flag here on the ui thread so a double click can't spawn
        // two builds. the worker clears it when it's done
        if (!is_compiling.exchange(true))
            std::thread(compile_code_async, std::string(code)).detach();
    }
    ImGui::EndDisabled();

    ImGui::BeginChild("##console", ImVec2(-FLT_MIN, console_h), ImGuiChildFlags_Borders);
    {
        // compile thread writes this, so lock while we read
        std::lock_guard<std::mutex> lock(console_mutex);
        ImGui::TextUnformatted(console_output.c_str());
    }
    ImGui::EndChild();

    ImGui::Columns(1);
}

int main()
{
    if (!make_work_dir())
    {
        printf("couldn't make temp dir for builds\n");
        return 1;
    }

    PaError err = Pa_Initialize();
    if (err != paNoError)
    {
        printf("PortAudio error: %s\n", Pa_GetErrorText(err));
        cleanup_work_dir();
        return 1;
    }

    selected_input_id = Pa_GetDefaultInputDevice();
    selected_output_id = Pa_GetDefaultOutputDevice();

    restart_audio_stream(selected_input_id, selected_output_id);

    if (!platform_init("Veksel", 1100, 650))
    {
        if (global_stream)
        {
            Pa_StopStream(global_stream);
            Pa_CloseStream(global_stream);
        }
        Pa_Terminate();
        cleanup_work_dir();
        return 1;
    }

    while (platform_begin_frame())
    {
        draw_ui();
        platform_end_frame();
    }

    platform_shutdown();

    // stop audio before unloading the plugin so nothing calls into it
    if (global_stream)
    {
        Pa_StopStream(global_stream);
        Pa_CloseStream(global_stream);
    }

    if (active_library_handle != nullptr)
        dlclose(active_library_handle);

    err = Pa_Terminate();
    if (err != paNoError)
        printf("PortAudio error: %s\n", Pa_GetErrorText(err));

    cleanup_work_dir();
    return 0;
}