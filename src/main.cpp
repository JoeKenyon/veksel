#include "VekselEngine.h"

#include <algorithm>
#include <cmath>
#include <csignal>
#include <atomic>

#include "imgui.h"
#include "platform.h"
#include "portaudio.h"
#include "TextEditor.h"

static std::atomic<bool> quit{false};

static void on_sigint(int)
{
    quit = true;
}

static TextEditor text_editor;

static int callback(
    const void* input,
    void* output,
    unsigned long frames,
    const PaStreamCallbackTimeInfo*,
    PaStreamCallbackFlags,
    void* user_data)
{
    auto* engine = static_cast<veksel::Engine*>(user_data);

    const float* in = static_cast<const float*>(input);
    float* out = static_cast<float*>(output);

    for (unsigned long i = 0; i < frames; ++i)
    {
        float sample = in ? in[i] : 0.0f;

        sample = engine->process_sample(sample);

        if (!std::isfinite(sample))
            sample = 0.0f;

        sample = std::clamp(sample, -1.0f, 1.0f);

        out[i * 2] = sample;
        out[i * 2 + 1] = sample;
    }

    return paContinue;
}

static const char* logo =
    "██╗   ██╗███████╗██╗  ██╗███████╗███████╗██╗     \n"
    "██║   ██║██╔════╝██║ ██╔╝██╔════╝██╔════╝██║     \n"
    "██║   ██║█████╗  █████╔╝ ███████╗█████╗  ██║     \n"
    "╚██╗ ██╔╝██╔══╝  ██╔═██╗ ╚════██║██╔══╝  ██║     \n"
    " ╚████╔╝ ███████╗██║  ██╗███████║███████╗███████╗\n"
    "  ╚═══╝  ╚══════╝╚═╝  ╚═╝╚══════╝╚══════╝╚══════╝";

static void draw_ui(veksel::Engine& engine)
{
    // create window
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


    float log_height  = 160;
    float main_height = ImGui::GetContentRegionAvail().y - log_height;
    float settings_width = ImGui::GetContentRegionAvail().x * 0.33;
    float compile_button_height = 25;

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

        // add compile button
        if (ImGui::Button("Compile & Run", ImVec2(-1, compile_button_height)))
        {
            engine.submit_source(text_editor.GetText());
        }


        ImGui::EndChild();
        ImGui::EndTable();
    }

    // add some space
    ImGui::Spacing();

    // add the log, use log height or -1 idk
    ImGui::BeginChild("##log", ImVec2(-1, -1), true);

    for (const auto& line : engine.get_log())
    {
        ImGui::TextUnformatted(line.c_str());
    }

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
    std::signal(SIGINT, on_sigint);

    veksel::Engine engine;

    if (!platform_init("Veksel", 900, 800))
        return 1;

    if (Pa_Initialize() != paNoError)
    {
        platform_shutdown();
        return 1;
    }

    text_editor.SetLanguage(TextEditor::LookupLanguage("C"));

    auto c_lang = const_cast<TextEditor::Language*>(text_editor.GetLanguage());

    if (c_lang)
    {
        c_lang->identifiers.insert("veksel_main");
        c_lang->identifiers.insert("input");
    }

    TextEditor::Palette p = TextEditor::GetDarkPalette();

    p[static_cast<int>(TextEditor::Color::background)] = 0xff000000;
    p[static_cast<int>(TextEditor::Color::text)]       = 0xffffffff;
    p[static_cast<int>(TextEditor::Color::comment)]    = 0xff888888;

    text_editor.SetPalette(p);

    text_editor.SetText(
        "veksel_main\n"
        "{\n"
        "    return input;\n"
        "}\n"
    );

    PaStreamParameters in_params = {};
    in_params.device = Pa_GetDefaultInputDevice();
    in_params.channelCount = 1;
    in_params.sampleFormat = paFloat32;
    in_params.suggestedLatency = 0.01;

    PaStreamParameters out_params = {};
    out_params.device = Pa_GetDefaultOutputDevice();
    out_params.channelCount = 2;
    out_params.sampleFormat = paFloat32;
    out_params.suggestedLatency = 0.01;

    PaStream* stream = nullptr;

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

    // Start the engine's compiler thread.
    engine.start();

    // Compile the initial source.
    engine.submit_source(text_editor.GetText());

    if (Pa_StartStream(stream) != paNoError)
    {
        Pa_CloseStream(stream);
        Pa_Terminate();
        platform_shutdown();
        return 1;
    }

    // Main UI loop.
    while (!quit && platform_begin_frame())
    {
        engine.update();

        draw_ui(engine);

        platform_end_frame();
    }

    // Stop audio before stopping/destroying the engine.
    PaError error = Pa_StopStream(stream);

    if (error != paNoError && error != paStreamIsStopped)
    {
        // Handle shutdown error.
    }

    Pa_CloseStream(stream);

    // Stop and join the compiler thread.
    engine.stop();

    Pa_Terminate();
    platform_shutdown();

    return 0;
}
