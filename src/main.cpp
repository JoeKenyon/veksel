#include "VekselEngine.h"

#include <algorithm>
#include <cmath>
#include <csignal>
#include <atomic>

#include "imgui.h"
#include "platform.h"
#include "portaudio.h"
#include "TextEditor.h"

// flips when we need to bail out, main loop checks it every frame
static std::atomic<bool> quit{false};

// settings page and editor share the same spot, this picks which one shows
static bool show_settings = false;

// ctrl+c in the terminal, just set the flag and let main clean up properly
static void on_sigint(int)
{
    quit = true;
}

static TextEditor text_editor;

// portaudio callback
// runs on the audio thread so nothing slow in here, no allocs, no locks
static int callback(
    const void* input,
    void* output,
    unsigned long frames,
    const PaStreamCallbackTimeInfo*,
    PaStreamCallbackFlags,
    void* user_data)
{
    // engine was handed over when the stream opened, just cast it back
    auto* engine = static_cast<veksel::Engine*>(user_data);

    const float* in = static_cast<const float*>(input);
    float* out = static_cast<float*>(output);

    // one sample at a time, simple but fine for now
    for (unsigned long i = 0; i < frames; ++i)
    {
        // no input device -> just feed it silence
        float sample = in ? in[i] : 0.0f;

        // whatever the user compiled does its thing here
        sample = engine->process_sample(sample);

        // nan or inf from a bad plugin would blow up the speakers, so zero it
        if (!std::isfinite(sample))
            sample = 0.0f;

        // hard clamp so nothing goes past full scale
        sample = std::clamp(sample, -1.0f, 1.0f);

        // mono in, so same sample to left and right
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

// button with idle / hover / active colours (text, bg, border)
// imgui only knows hover after the button is drawn so i work it out from the mouse rect first
static bool hover_button(const char* label, ImVec2 size = ImVec2(0, 0),
                         ImVec4 hover_text    = Theme::accent,
                         ImVec4 hover_bg      = Theme::accent_med,
                         ImVec4 hover_border  = Theme::accent_med,
                         ImVec4 active_text   = Theme::accent,
                         ImVec4 active_bg     = Theme::accent_med,
                         ImVec4 active_border = Theme::accent_med)
{
    // size of 0 means fit the label, same as imgui does
    if (size.x <= 0 || size.y <= 0)
    {
        const ImVec2 t = ImGui::CalcTextSize(label, nullptr, true);
        const ImVec2 pad = ImGui::GetStyle().FramePadding;
        if (size.x <= 0) size.x = t.x + pad.x * 2.0f;
        if (size.y <= 0) size.y = t.y + pad.y * 2.0f;
    }

    // cursor is where the button is about to go, so check the mouse against that rect
    // IsWindowHovered stops it lighting up when a popup is covering it
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const bool hover = ImGui::IsWindowHovered() &&
        ImGui::IsMouseHoveringRect(p, ImVec2(p.x + size.x, p.y + size.y));

    // active = hovering and mouse held down
    const bool active = hover && ImGui::IsMouseDown(ImGuiMouseButton_Left);

    // idle look first, then override if hovered or pressed
    ImVec4 button_text   = Theme::accent_neg;
    ImVec4 button_bg     = Theme::accent;
    ImVec4 button_border = button_bg;

    if (active)     { button_text = active_text; button_bg = active_bg; button_border = active_border; }
    else if (hover) { button_text = hover_text; button_bg = hover_bg; button_border = hover_border; }

    // push all three bg colours the same, otherwise imgui swaps in the theme's hovered/active
    // ones and the bg goes black. we already picked the colour so it just has to stay put
    ImGui::PushStyleColor(ImGuiCol_Button,        button_bg);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, button_bg);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  button_bg);
    ImGui::PushStyleColor(ImGuiCol_Text,          button_text);
    ImGui::PushStyleColor(ImGuiCol_Border,        button_border);
    const bool pressed = ImGui::Button(label, size);
    ImGui::PopStyleColor(5);

    return pressed;
}

// combo that changes colour on hover, bg goes accent_neg -> accent_med
// items is "A\0B\0C\0" like ImGui::Combo, returns true when the selection changes
static bool hover_combo(const char* label, int* current, const char* items,
                        float width = 300.0f)
{
    // walk the \0 separated list to find the name of the selected item
    const char* preview = "";
    {
        int i = 0;
        for (const char* it = items; *it; it += std::strlen(it) + 1, ++i)
            if (i == *current)
                preview = it;
    }

    // same mouse rect trick as the button
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const bool hot = ImGui::IsWindowHovered() &&
        ImGui::IsMouseHoveringRect(p, ImVec2(p.x + width, p.y + ImGui::GetFrameHeight()));

    const ImVec4 bg     = hot ? Theme::accent_med : Theme::accent_neg;
    const ImVec4 border = hot ? Theme::accent : Theme::accent;
    const ImVec4 text   = hot ? Theme::accent : Theme::accent;

    ImGui::SetNextItemWidth(width);

    // frame bg and button are both pushed, the arrow box on the right uses the button colours
    // hovered/active get the same colour so imgui doesn't override mine
    ImGui::PushStyleColor(ImGuiCol_Text,           text);
    ImGui::PushStyleColor(ImGuiCol_Border,         border);
    ImGui::PushStyleColor(ImGuiCol_FrameBg,        bg);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, bg);
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive,  bg);
    ImGui::PushStyleColor(ImGuiCol_Button,         bg);  // arrow box
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,  bg);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,   bg);
    const bool open = ImGui::BeginCombo(label, preview);
    // pop right away so the dropdown items keep the normal theme colours
    ImGui::PopStyleColor(8);

    bool changed = false;

    if (open)
    {
        // draw each item, id per item so identical names don't clash
        int i = 0;
        for (const char* it = items; *it; it += std::strlen(it) + 1, ++i)
        {
            const bool selected = (i == *current);

            ImGui::PushID(i);
            if (ImGui::Selectable(it, selected))
            {
                *current = i;
                changed = true;
            }
            // scroll to the selected item when the dropdown opens
            if (selected) ImGui::SetItemDefaultFocus();
            ImGui::PopID();
        }

        ImGui::EndCombo();
    }

    return changed;
}

// draw the main application interface
static void draw_ui(veksel::Engine& engine)
{
    const float spacing = ImGui::GetStyle().FramePadding.x;
    const float log_height = 180.0f;
    const ImVec4 hover_text = Theme::accent;

    // main window, fills the whole viewport and can't be moved or resized
    ImGuiViewport* vp = ImGui::GetMainViewport();

    ImGui::SetNextWindowPos(vp->Pos);
    ImGui::SetNextWindowSize(vp->Size);

    ImGui::Begin(
        "VekselMain",
        nullptr,
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoSavedSettings
    );

    // header, height comes from the logo font size since the logo is the tallest thing in it
    const float font_size = g_small_font
        ? g_small_font->LegacySize
        : ImGui::GetFontSize();

    // logo is 6 lines, 7.5 leaves a bit of room around it
    const float header_height = 7.5f * font_size + (ImGui::GetStyle().FramePadding.x);

    ImGui::BeginChild(
        "##header",
        ImVec2(0, header_height),
        ImGuiChildFlags_FrameStyle,
        ImGuiWindowFlags_NoScrollbar
    );

    const float button_width = 45.0f;
    const float button_height = 45.0f;

    const float padding = ImGui::GetStyle().FramePadding.x;
    const float buttons_width = button_width * 2.0f + padding;

    // logo uses the small font so it fits
    if (g_small_font) ImGui::PushFont(g_small_font);
    ImGui::TextUnformatted(logo);
    if (g_small_font) ImGui::PopFont();

    // buttons sit on the right, vertically centred in the header
    const float buttons_x =
        ImGui::GetWindowWidth() - buttons_width - padding;

    const float buttons_y =
        (ImGui::GetWindowHeight() - button_height) * 0.5f;

    ImGui::SetCursorPos(ImVec2(buttons_x, buttons_y));

    // play, sends whatever's in the editor to the compiler thread
    if (hover_button("▶", ImVec2(button_width, button_height)))
    {
        engine.submit_source(text_editor.GetText());
    }

    ImGui::SameLine(0.0f, padding);

    // settings toggle
    if (hover_button("⚙", ImVec2(button_width, button_height)))
    {
        show_settings = !show_settings;
    }

    ImGui::EndChild();

    // whatever's left under the header minus the log is the main area
    const float available_height = ImGui::GetContentRegionAvail().y;
    const float main_height = available_height - log_height - spacing;

    // settings or editor, same spot so only one shows at a time
    if (show_settings)
    {
        ImGui::BeginChild("##settings", ImVec2(0, main_height), ImGuiChildFlags_FrameStyle);

        // placeholders for now, not hooked up to portaudio yet
        static int output_device = 0;
        static int input_device = 0;
        static int sample_rate = 1;
        static int buffer_size = 2;

        const float combo_width = 300.0f;
        const float combo_height = ImGui::GetFrameHeight();

        // centre the column horizontally
        const float x = (ImGui::GetWindowWidth() - combo_width) * 0.5f;

        // and vertically, 4 groups of label + gap + combo + gap
        // it's an estimate so might be a few pixels off
        const float group_height = ImGui::GetTextLineHeight() + combo_height + padding * 2.0f;
        const float block_height = group_height * 4.0f;
        const float y = (ImGui::GetWindowHeight() - block_height) * 0.5f;
        if (y > 0.0f) ImGui::SetCursorPosY(y);

        // every label and combo needs the x set again, imgui resets it each line
        ImGui::SetCursorPosX(x);
        ImGui::TextUnformatted("Output Device");
        ImGui::Dummy({0,padding});
        ImGui::SetCursorPosX(x);
        hover_combo("##output_device", &output_device,
                    "Default Output\0Device 1\0Device 2\0");

        ImGui::Dummy({0,padding});

        ImGui::SetCursorPosX(x);
        ImGui::TextUnformatted("Input Device");
        ImGui::Dummy({0,padding});
        ImGui::SetCursorPosX(x);
        hover_combo("##input_device", &input_device,
                    "Default Input\0Device 1\0Device 2\0");

        ImGui::Dummy({0,padding});

        ImGui::SetCursorPosX(x);
        ImGui::TextUnformatted("Sample Rate");
        ImGui::Dummy({0,padding});
        ImGui::SetCursorPosX(x);
        hover_combo("##sample_rate", &sample_rate,
                    "44100 Hz\0" "48000 Hz\0" "88200 Hz\0" "96000 Hz\0");

        ImGui::Dummy({0,padding});

        ImGui::SetCursorPosX(x);
        ImGui::TextUnformatted("Buffer Size");
        ImGui::Dummy({0,padding});
        ImGui::SetCursorPosX(x);
        hover_combo("##buffer_size", &buffer_size,
                    "64\0" "128\0" "256\0" "512\0" "1024\0");

        ImGui::EndChild();
    }
    else
    {
        ImGui::BeginChild(
            "##editor",
            ImVec2(0, main_height),
            ImGuiChildFlags_FrameStyle
        );

        // fill the whole child
        text_editor.Render(
            "##source",
            ImVec2(-1, -1),
            false
        );

        ImGui::EndChild();
    }

    // log, takes up the rest of the space at the bottom
    ImGui::BeginChild(
        "##log",
        ImVec2(-1, -1),
        ImGuiChildFlags_FrameStyle
    );

    const auto& logs = engine.get_log();

    for (const auto& line : logs)
    {
        ImGui::TextUnformatted(line.c_str());
    }

    // if we're already at the bottom stay there when new lines come in
    // if the user scrolled up leave them alone
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
        ImGui::SetScrollHereY(1.0f);

    ImGui::EndChild();

    ImGui::End();
}

int main()
{
    // catch ctrl+c
    std::signal(SIGINT, on_sigint);

    // engine first so it outlives the audio stream
    veksel::Engine engine;

    // window and opengl
    if (!platform_init("Veksel", 900, 800))
        return 1;

    // portaudio
    if (Pa_Initialize() != paNoError)
    {
        platform_shutdown();
        return 1;
    }

    // editor highlights as C since the modules are c
    text_editor.SetLanguage(TextEditor::LookupLanguage("C"));

    // language is const so cast it away to add our own identifiers
    auto c_lang = const_cast<TextEditor::Language*>(text_editor.GetLanguage());

    // so veksel_main and input get highlighted like known names
    if (c_lang)
    {
        c_lang->identifiers.insert("veksel_main");
        c_lang->identifiers.insert("input");
    }

    // dark palette but pure black bg, white text, grey comments
    TextEditor::Palette p = TextEditor::GetDarkPalette();

    p[static_cast<int>(TextEditor::Color::background)] = 0xff000000;
    p[static_cast<int>(TextEditor::Color::text)]       = 0xffffffff;
    p[static_cast<int>(TextEditor::Color::comment)]    = 0xff888888;

    text_editor.SetPalette(p);

    // starter code, passthrough
    text_editor.SetText(
        "veksel_main\n"
        "{\n"
        "    return input;\n"
        "}\n"
    );

    // input, mono
    PaStreamParameters in_params = {};
    in_params.device = Pa_GetDefaultInputDevice();
    in_params.channelCount = 1;
    in_params.sampleFormat = paFloat32;
    in_params.suggestedLatency = 0.01;

    // output, stereo
    PaStreamParameters out_params = {};
    out_params.device = Pa_GetDefaultOutputDevice();
    out_params.channelCount = 2;
    out_params.sampleFormat = paFloat32;
    out_params.suggestedLatency = 0.01;

    PaStream* stream = nullptr;

    // open the stream, engine goes in as user data so the callback can reach it
    // 44100 and 64 frames hardcoded until settings are wired up
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

    // compiler thread
    engine.start();

    // compile whatever's in the editor so there's something running straight away
    engine.submit_source(text_editor.GetText());

    // audio goes live
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
        // swap in any freshly compiled plugin
        engine.update();

        draw_ui(engine);

        platform_end_frame();
    }

    // audio has to stop first so the callback isn't touching the plugin
    // while the engine is being torn down
    PaError error = Pa_StopStream(stream);

    if (error != paNoError && error != paStreamIsStopped)
    {
        // todo: do something with this
    }

    Pa_CloseStream(stream);

    // join the compiler thread
    engine.stop();

    // shut everything else down
    Pa_Terminate();
    platform_shutdown();

    return 0;
}
