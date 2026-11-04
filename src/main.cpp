#include <cfloat>
#include "imgui.h"
#include "platform.h"
#include "portaudio.h"
#include <vector>
#include <cstdio>
#include <string>

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
static PaStream *global_stream = nullptr;

// callback function for port audio
static int audio_callback(const void *inputBuffer, void *outputBuffer,
                          unsigned long framesPerBuffer,
                          const PaStreamCallbackTimeInfo *timeInfo,
                          PaStreamCallbackFlags statusFlags,
                          void *userData)
{
    float *out = (float*)outputBuffer;
    const float *in = (const float*)inputBuffer;
    unsigned int i;
    (void) timeInfo; /* Prevent unused variable warnings. */
    (void) statusFlags;
    (void) userData;
 
    if( inputBuffer == NULL )
    {
        for( i=0; i<framesPerBuffer; i++ )
        {
            *out++ = 0;  /* left - silent */
            *out++ = 0;  /* right - silent */
        }
        //gNumNoInputs += 1;
    }
    else
    {
        for( i=0; i<framesPerBuffer; i++ )
        {
            float sample = *in++; /* MONO input */
            *out++ = sample; /* left - distorted */
            *out++ = sample;       /* right - clean */
        }
    }

    return paContinue;
}

// function to restart the stream when devices change
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
    {
        Pa_StartStream(global_stream);
    }
    else
    {
        printf("PortAudio restart error: %s\n", Pa_GetErrorText(err));
    }
}

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

    // Input Device Combo Selection
    if (ImGui::BeginCombo("Input Device", input_name.c_str()))
    {
        for (const auto &d : devices)
        {
            if (d.max_input_channels > 0)
            {
                bool is_selected = (selected_input_id == d.id);
                if (ImGui::Selectable(d.name.c_str(), is_selected))
                {
                    selected_input_id = d.id;
                    restart_audio_stream(selected_input_id, selected_output_id);
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

    // Output Device Combo Selection
    if (ImGui::BeginCombo("Output Device", output_name.c_str()))
    {
        for (const auto &d : devices)
        {
            if (d.max_output_channels > 0)
            {
                bool is_selected = (selected_output_id == d.id);
                if (ImGui::Selectable(d.name.c_str(), is_selected))
                {
                    selected_output_id = d.id;
                    restart_audio_stream(selected_input_id, selected_output_id);
                }
                if (is_selected) ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    ImGui::SeparatorText("Visualizer");

    static std::vector<float> waveform_data(512, 0.0f);
    ImGui::PlotLines("##waveform", waveform_data.data(), (int)waveform_data.size(), 0, "live output", -1.0f, 1.0f, ImVec2(-1, 100));

    static std::vector<float> freq_data(256, 0.0f);
    ImGui::SeparatorText("Spectrum Analyzer (FFT)");
    ImGui::PlotLines("##fft", freq_data.data(), (int)freq_data.size(), 0, "frequency spectrum", 0.0f, 1.0f, ImVec2(-1, 100));

    ImGui::SeparatorText("Controls");

    if(ImGui::SliderFloat("Volume", &volume, 0.0f, 1.0f)) {}
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

    ImGui::InputTextMultiline("##code", code, sizeof code, ImVec2(-FLT_MIN, -below), ImGuiInputTextFlags_AllowTabInput);

    if (ImGui::Button("Compile", ImVec2(-FLT_MIN, 0))) {}

    ImGui::BeginChild("##console", ImVec2(-FLT_MIN, console_h), ImGuiChildFlags_Borders);
    ImGui::TextUnformatted("Debug output goes here");
    ImGui::EndChild();
    
    ImGui::Columns(1);
}

int main()
{
    PaError err = Pa_Initialize();
    if (err != paNoError)
    {
        printf("PortAudio error: %s\n", Pa_GetErrorText(err));
        return 1;
    }

    selected_input_id = Pa_GetDefaultInputDevice();
    selected_output_id = Pa_GetDefaultOutputDevice();

    // start initial stream
    restart_audio_stream(selected_input_id, selected_output_id);

    if (!platform_init("Veksel", 1100, 650))
    {
        if (global_stream)
        {
            Pa_StopStream(global_stream);
            Pa_CloseStream(global_stream);
        }
        Pa_Terminate();
        return 1;
    }

    while (platform_begin_frame())
    {
        draw_ui();
        platform_end_frame();
    }

    platform_shutdown();

    if (global_stream)
    {
        Pa_StopStream(global_stream);
        Pa_CloseStream(global_stream);
    }

    err = Pa_Terminate();
    if (err != paNoError)
    {
        printf("PortAudio error: %s\n", Pa_GetErrorText(err));
        return 1;
    }

    return 0;
}