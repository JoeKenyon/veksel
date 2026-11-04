#include <cfloat>
#include "imgui.h"
#include "platform.h"
#include "portaudio.h"
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

struct AudioDeviceInfo
{
    int id;
    std::string name;
    int max_input_channels;
    int max_output_channels;
};

static int selected_input_id = -1;
static int selected_output_id = -1;

static void draw_ui()
{
    ImGui::Columns(2, "main", true);

    // --- LEFT COLUMN: Controls & Visualizer ---
    ImGui::Text("%s", banner);

    ImGui::SeparatorText("Audio Devices");

    std::vector<AudioDeviceInfo> devices;
    int num_devices = Pa_GetDeviceCount();
    for (int i = 0; i < num_devices; ++i)
    {
        const PaDeviceInfo *info = Pa_GetDeviceInfo(i);
        if (info)
        {
            devices.push_back({i, info->name, info->maxInputChannels, info->maxOutputChannels});
        }
    }

    std::string input_name = "None / Default";
    for (const auto &d : devices)
    {
        if (d.id == selected_input_id && d.max_input_channels > 0)
        {
            input_name = d.name;
            break;
        }
    }

    // TODO: put input device combo layout and selection logic here
    static int Selecteditem = 0;
    if (ImGui::BeginCombo("Input Device", input_name.c_str()))
    {
        for (const auto &d : devices)
        {
            if (d.max_input_channels > 0)
            {
                bool is_selected = (selected_input_id == d.id);
                if (ImGui::Selectable(d.name.c_str(), is_selected))
                {
                    // initialize device
                    selected_input_id = d.id;
                }
                if (is_selected) ImGui::SetItemDefaultFocus();
            }
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

    // TODO: put output device combo layout and selection logic here
    if (ImGui::BeginCombo("Output Device", output_name.c_str()))
    {
        for (const auto &d : devices)
        {
            if (d.max_output_channels > 0)
            {
                bool is_selected = (selected_output_id == d.id);
                if (ImGui::Selectable(d.name.c_str(), is_selected))
                {
                    // initialize device
                    selected_output_id = d.id;
                }
                if (is_selected) ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }




    ImGui::SeparatorText("Visualizer");

    // TODO: read latest samples from ring buffer and draw waveform lines
    static std::vector<float> waveform_data(512, 0.0f);
    ImGui::PlotLines("##waveform", waveform_data.data(), (int)waveform_data.size(), 0, "live output", -1.0f, 1.0f, ImVec2(-1, 100));


    // TODO: draw frequency spectrum plot lines
    static std::vector<float> freq_data(256, 0.0f);
    ImGui::SeparatorText("Spectrum Analyzer (FFT)");
    ImGui::PlotLines("##fft", freq_data.data(), (int)freq_data.size(), 0, "frequency spectrum", 0.0f, 1.0f, ImVec2(-1, 100));

    ImGui::SeparatorText("Controls");

    // TODO: hook up volume and parameter sliders here
    if(ImGui::SliderFloat("Volume", &volume, 0.0f, 1.0f))
    {
        // set volume, its already being set
    }
    ImGui::SliderFloat("Param 1", &param1, 0.0f, 1.0f);
    ImGui::SliderFloat("Param 2", &param2, 0.0f, 1.0f);
    ImGui::SliderFloat("Param 3", &param3, 0.0f, 1.0f);
    ImGui::SliderFloat("Param 4", &param4, 0.0f, 1.0f);
    ImGui::SliderFloat("Param 5", &param5, 0.0f, 1.0f);

    // --- RIGHT COLUMN: Editor & Console ---
    ImGui::NextColumn();
    
    ImGui::SeparatorText("Editor");

    const float console_h = 140.0f;
    const ImGuiStyle &st = ImGui::GetStyle();
    const float below = console_h + ImGui::GetFrameHeight() + st.ItemSpacing.y * 2;

    // TODO: add multiline text input widget for code editing
    ImGui::InputTextMultiline("##code", code, sizeof code, ImVec2(-FLT_MIN, -below), ImGuiInputTextFlags_AllowTabInput);

    // TODO: add compile button click handler
    if (ImGui::Button("Compile", ImVec2(-FLT_MIN, 0))) 
    { 

    }

    ImGui::BeginChild("##console", ImVec2(-FLT_MIN, console_h), ImGuiChildFlags_Borders);
    
    // TODO: display console text output
    ImGui::TextUnformatted("Debug output goes here");

    ImGui::EndChild();
    
    ImGui::Columns(1);
}

int main()
{
    PaError err;
    err = Pa_Initialize();
    if( err != paNoError )
    {
        printf(  "PortAudio error: %s\n", Pa_GetErrorText( err ) );
        return 1;
    }

    selected_input_id = Pa_GetDefaultInputDevice();
    selected_output_id = Pa_GetDefaultOutputDevice();

    if (!platform_init("Veksel", 1100, 650))
    {
        return 1;
    }

    while (platform_begin_frame())
    {
        draw_ui();
        platform_end_frame();
    }

    platform_shutdown();

    err = Pa_Terminate();
    if( err != paNoError )
    {
        printf(  "PortAudio error: %s\n", Pa_GetErrorText( err ) );
        return 1;
    }

    return 0;
}