#include <cfloat>
#include "imgui.h"
#include "platform.h"
#include "portaudio.h"
#include "fft.h"
#include "audio.h"
#include <cmath>
#include <iostream>
#include <vector>

static char code[8192] =
    "// write your code here\n"
    "int add(int a, int b) {\n"
    "    return a + b;\n"
    "}\n";

static const char *banner =
    "██╗   ██╗███████╗██╗  ██╗███████╗███████╗██╗     \n"
    "██║   ██║██╔════╝██║ ██╔╝██╔════╝██╔════╝██║     \n"
    "██║   ██║█████╗  █████╔╝ ███████╗█████╗  ██║     \n"
    "╚██╗ ██╔╝██╔══╝  ██╔═██╗ ╚════██║██╔══╝  ██║     \n"
    " ╚████╔╝ ███████╗██║  ██╗███████║███████╗███████╗\n"
    "  ╚═══╝  ╚══════╝╚═╝  ╚═╝╚══════╝╚══════╝╚══════╝";

static float volume = 0.5f;
static float param1 = 0.5f;
static float param2 = 0.5f;
static float param3 = 0.5f;
static float param4 = 0.5f;
static float param5 = 0.5f;

static AudioEngine audio;

static void draw_ui()
{
    volume = audio.load_volume();

    // make static so it persists between frames and doesn't re-allocate
    static std::vector<float> plot_data(512, 0.0f);

    ImGui::Columns(2, "main", true);


    // --- LEFT COLUMN: Controls & Visualizer ---
    ImGui::Text("%s", banner);


   ImGui::SeparatorText("Audio Devices");

    auto devices = audio.get_devices();

    // --- Input Device Combo ---
    std::string input_name = "None / Default";
    for (const auto &d : devices)
    {
        if (d.id == audio.selected_input_device && d.max_input_channels > 0)
        {
            input_name = d.name;
            break;
        }
    }

    if (ImGui::BeginCombo("Input Device", input_name.c_str()))
    {   
        for (const auto &d : devices)
        {
            if (d.max_input_channels > 0)
            {
                bool is_selected = (audio.selected_input_device == d.id);
                if (ImGui::Selectable(d.name.c_str(), is_selected))
                {
                    audio.restart(d.id, audio.selected_output_device);
                }
                if (is_selected) ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    // --- Output Device Combo ---
    std::string output_name = "Default";
    for (const auto &d : devices)
    {
        if (d.id == audio.selected_output_device && d.max_output_channels > 0)
        {
            output_name = d.name;
            break;
        }
    }

    if (ImGui::BeginCombo("Output Device", output_name.c_str()))
    {
        for (const auto &d : devices)
        {
            if (d.max_output_channels > 0)
            {
                bool is_selected = (audio.selected_output_device == d.id);
                if (ImGui::Selectable(d.name.c_str(), is_selected))
                {
                    audio.restart(audio.selected_input_device, d.id);
                }
                if (is_selected) ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    ImGui::SeparatorText("Visualizer");

    // pull latest samples from the thread-safe ring buffer
    size_t samples_read = audio.scope_buffer.read_latest(plot_data.data(), plot_data.size());
    
    // render the live waveform display
    ImGui::PlotLines("##waveform", plot_data.data(), (int)samples_read, 0, "live output", -1.0f, 1.0f, ImVec2(-1, 100));

    // create a persistent analyzer and raw sample buffer
    static SpectrumAnalyzer analyzer(512);
    static std::vector<float> time_samples(512, 0.0f);
    static std::vector<float> freq_data(256, 0.0f);

    // inside draw_ui():
    ImGui::SeparatorText("Spectrum Analyzer (FFT)");

    // grab latest chunk from scope buffer
    audio.scope_buffer.read_latest(time_samples.data(), time_samples.size());

    // compute spectrum
    analyzer.compute(time_samples.data(), 44100.0f); // variable sample rate maybe

    // copy to render vector
    for (size_t i = 0; i < analyzer.get_bin_count(); ++i)
    {
        freq_data[i] = analyzer.get_magnitudes()[i];
    }

    // plot the frequency bins
    ImGui::PlotLines("##fft", freq_data.data(), (int)freq_data.size(), 0, "frequency spectrum", 0.0f, 1.0f, ImVec2(-1, 100));

    ImGui::SeparatorText("Controls");

    if(ImGui::SliderFloat("Volume", &volume, 0.0f,  1.0f))
    {
        audio.set_volume(volume);
    }
    ImGui::SliderFloat("Param 1", &param1,  0.0f,  1.0f);
    ImGui::SliderFloat("Param 2", &param2,  0.0f,  1.0f);
    ImGui::SliderFloat("Param 3", &param3,  0.0f,  1.0f);
    ImGui::SliderFloat("Param 4", &param4,  0.0f,  1.0f);
    ImGui::SliderFloat("Param 5", &param5,  0.0f,  1.0f);

    // --- RIGHT COLUMN: Editor & Console ---
    ImGui::NextColumn();
    
    ImGui::SeparatorText("Editor");

    const float console_h = 140.0f;
    const ImGuiStyle &st = ImGui::GetStyle();
    const float below = console_h + ImGui::GetFrameHeight() + st.ItemSpacing.y * 2;

    ImGui::InputTextMultiline("##code", code, sizeof code,
                          ImVec2(-FLT_MIN, -below),
                          ImGuiInputTextFlags_AllowTabInput);

    if (ImGui::Button("Compile", ImVec2(-FLT_MIN, 0)))
    {
        // todo: hook up actual compilation later
    }

    ImGui::BeginChild("##console", ImVec2(-FLT_MIN, console_h), ImGuiChildFlags_Borders);
    ImGui::TextUnformatted("[info] ready.\n[ok] waiting for changes");
    ImGui::EndChild();
    
    // clean up columns state for the frame
    ImGui::Columns(1);
}

int main()
{

    if (!platform_init("Veksel", 1100, 650))
    {
        return 1;
    }

    // initialize audio after platform windowing is up
    audio.init();

    while (platform_begin_frame())
    {
        draw_ui();
        platform_end_frame();
    }

    platform_shutdown();
    audio.shutdown();
    return 0;
}