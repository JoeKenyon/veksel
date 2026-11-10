
#include "VekselEngine.h"

#include <algorithm>
#include <cmath>
#include <csignal>
#include <atomic>

#include "imgui.h"
#include "platform.h"
#include "portaudio.h"
#include "TextEditor.h"

// used to tell the main loop when the application should close
static std::atomic<bool> quit{false};

// handle ctrl+c from the terminal
static void on_sigint(int)
{
    quit = true;
}

static TextEditor text_editor;

// portaudio callback
// runs on the audio thread and processes each incoming sample
static int callback(
    const void* input,
    void* output,
    unsigned long frames,
    const PaStreamCallbackTimeInfo*,
    PaStreamCallbackFlags,
    void* user_data)
{
    // get the engine passed in when the stream was opened
    auto* engine = static_cast<veksel::Engine*>(user_data);

    const float* in = static_cast<const float*>(input);
    float* out = static_cast<float*>(output);

    // process each sample in the audio buffer
    for (unsigned long i = 0; i < frames; ++i)
    {
        // use silence if there is no input
        float sample = in ? in[i] : 0.0f;

        // run the sample through the active plugin
        sample = engine->process_sample(sample);

        // prevent invalid values from reaching the audio output
        if (!std::isfinite(sample))
            sample = 0.0f;

        // keep the output within the normal audio range
        sample = std::clamp(sample, -1.0f, 1.0f);

        // send the same sample to both output channels
        out[i * 2] = sample;
        out[i * 2 + 1] = sample;
    }

    return paContinue;
}

// veksel logo
static const char* logo =
    "██╗   ██╗███████╗██╗  ██╗███████╗███████╗██╗     \n"
    "██║   ██║██╔════╝██║ ██╔╝██╔════╝██╔════╝██║     \n"
    "██║   ██║█████╗  █████╔╝ ███████╗█████╗  ██║     \n"
    "╚██╗ ██╔╝██╔══╝  ██╔═██╗ ╚════██║██╔══╝  ██║     \n"
    " ╚████╔╝ ███████╗██║  ██╗███████║███████╗███████╗\n"
    "  ╚═══╝  ╚══════╝╚═╝  ╚═╝╚══════╝╚══════╝╚══════╝";

// draw the main application interface
static void draw_ui(veksel::Engine& engine)
{
    // create window and fill the entire viewport
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->Pos);
    ImGui::SetNextWindowSize(vp->Size);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 10.0f));

    ImGui::Begin("VekselMain", nullptr,
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar);

    // header with logo text, use the small font
    ImGui::BeginChild("##header_child", ImVec2(-1, ImGui::GetTextLineHeightWithSpacing() * 3.750f), true);
    if (g_small_font) ImGui::PushFont(g_small_font);
    float window_width = ImGui::GetContentRegionAvail().x;
    float logo_width = ImGui::CalcTextSize(logo).x;
    ImGui::SetCursorPosX((window_width - logo_width) * 0.5f);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
    ImGui::TextUnformatted(logo);
    ImGui::PopStyleColor();
    if (g_small_font) ImGui::PopFont();
    ImGui::EndChild();

    // add some space
    ImGui::Spacing();

    // calculate the space available for each section
    float log_height  = 160;
    float main_height = ImGui::GetContentRegionAvail().y - log_height;
    float settings_width = ImGui::GetContentRegionAvail().x * 0.33;
    float compile_button_height = 25;

    // split the main area into settings and editor columns
    if (ImGui::BeginTable("##main_split", 2, ImGuiTableColumnFlags_NoResize))
    {
        // set up left and right columns
        // how wide should settings be uuuhhh idk
        ImGui::TableSetupColumn("Left", ImGuiTableColumnFlags_WidthFixed, settings_width);
        ImGui::TableSetupColumn("Right", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableNextRow();

        // start first col
        ImGui::TableSetColumnIndex(0);
        ImGui::BeginChild("##left_col", ImVec2(0, main_height), true);

        // audio settings, configure these correctly later
        static int in_audio_device = 0;
        const char* in_devices[] = { "Default Input", "Mic 1", "Interface 1" };
        ImGui::Combo("Input", &in_audio_device, in_devices, IM_ARRAYSIZE(in_devices));

        static int out_audio_device = 0;
        const char* out_devices[] = { "Default Output", "Speaker 1", "Interface 1" };
        ImGui::Combo("Output", &out_audio_device, out_devices, IM_ARRAYSIZE(out_devices));

        // right col
        ImGui::EndChild();
        ImGui::TableSetColumnIndex(1);
        ImGui::BeginChild("##right_col", ImVec2(0, main_height));

        // text editor, leave space for button
        auto avail = ImGui::GetContentRegionAvail();
        text_editor.Render("##editor", ImVec2(-1, avail.y - compile_button_height), true);

        // submit the editor contents for background compilation
        if (ImGui::Button("Compile & Run", ImVec2(-1, compile_button_height)))
        {
            engine.submit_source(text_editor.GetText());
        }

        ImGui::EndChild();
        ImGui::EndTable();
    }

    // add some space
    ImGui::Spacing();

    // display compiler output and engine messages
    ImGui::BeginChild("##log", ImVec2(-1, -1), true);

    for (const auto& line : engine.get_log())
    {
        ImGui::TextUnformatted(line.c_str());
    }

    // keep the log scrolled to the bottom when already at the bottom
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f)
        ImGui::SetScrollHereY(1.0f);

    ImGui::EndChild();

    // finished ui
    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

int main()
{
    // listen for ctrl+c
    std::signal(SIGINT, on_sigint);

    // create the engine
    veksel::Engine engine;

    // initialise the window and graphics platform
    if (!platform_init("Veksel", 900, 800))
        return 1;

    // initialise portaudio
    if (Pa_Initialize() != paNoError)
    {
        platform_shutdown();
        return 1;
    }

    // configure the text editor language and syntax highlighting
    text_editor.SetLanguage(TextEditor::LookupLanguage("C"));

    auto c_lang = const_cast<TextEditor::Language*>(text_editor.GetLanguage());

    // add veksel-specific names to the editor's recognised identifiers
    if (c_lang)
    {
        c_lang->identifiers.insert("veksel_main");
        c_lang->identifiers.insert("input");
    }

    // customise the editor colours
    TextEditor::Palette p = TextEditor::GetDarkPalette();

    p[static_cast<int>(TextEditor::Color::background)] = 0xff000000;
    p[static_cast<int>(TextEditor::Color::text)]       = 0xffffffff;
    p[static_cast<int>(TextEditor::Color::comment)]    = 0xff888888;

    text_editor.SetPalette(p);

    // set the initial source code shown in the editor
    text_editor.SetText(
        "veksel_main\n"
        "{\n"
        "    return input;\n"
        "}\n"
    );

    // configure the audio input device
    PaStreamParameters in_params = {};
    in_params.device = Pa_GetDefaultInputDevice();
    in_params.channelCount = 1;
    in_params.sampleFormat = paFloat32;
    in_params.suggestedLatency = 0.01;

    // configure the audio output device
    PaStreamParameters out_params = {};
    out_params.device = Pa_GetDefaultOutputDevice();
    out_params.channelCount = 2;
    out_params.sampleFormat = paFloat32;
    out_params.suggestedLatency = 0.01;

    PaStream* stream = nullptr;

    // open the audio stream and pass the engine to the callback
    if (Pa_OpenStream(
            &stream,
            &in_params,
            &out_params,
            44100,
            64,
            paClipOff,
            callback,
            &engine) != paNoError)
    {
        Pa_Terminate();
        platform_shutdown();
        return 1;
    }

    // start the engine's compiler thread
    engine.start();

    // compile the initial source
    engine.submit_source(text_editor.GetText());

    // start audio processing
    if (Pa_StartStream(stream) != paNoError)
    {
        Pa_CloseStream(stream);
        engine.stop();
        Pa_Terminate();
        platform_shutdown();
        return 1;
    }

    // main ui loop
    while (!quit && platform_begin_frame())
    {
        // check for newly compiled plugins and install them
        engine.update();

        // draw the interface
        draw_ui(engine);

        platform_end_frame();
    }

    // stop audio before stopping/destroying the engine
    // this makes sure the audio thread is no longer using the active plugin
    PaError error = Pa_StopStream(stream);

    if (error != paNoError && error != paStreamIsStopped)
    {
        // handle shutdown error
    }

    Pa_CloseStream(stream);

    // stop and join the compiler thread
    engine.stop();

    // clean up portaudio and the graphics platform
    Pa_Terminate();
    platform_shutdown();

    return 0;
}
