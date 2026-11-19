#include "UI.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace veksel;

namespace
{
    // flat button with an accent colour
    bool CustomButton(const char* label, const ImVec2& size = ImVec2(0, 0))
    {
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 2.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.12f, 0.14f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.0f, 0.5f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.8f, 0.0f, 0.4f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.95f, 0.95f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.3f, 0.3f, 0.35f, 1.0f));

        const bool clicked = ImGui::Button(label, size);

        ImGui::PopStyleColor(5);
        ImGui::PopStyleVar(2);

        return clicked;
    }

    constexpr const char* Logo =
        "██╗   ██╗███████╗██╗  ██╗███████╗███████╗██╗     \n"
        "██║   ██║██╔════╝██║ ██╔╝██╔════╝██╔════╝██║     \n"
        "██║   ██║█████╗  █████╔╝ ███████╗█████╗  ██║     \n"
        "╚██╗ ██╔╝██╔══╝  ██╔═██╗ ╚════██║██╔══╝  ██║     \n"
        " ╚████╔╝ ███████╗██║  ██╗███████║███████╗███████╗\n"
        "  ╚═══╝  ╚══════╝╚═╝  ╚═╝╚══════╝╚══════╝╚══════╝";

    // what a fresh editor starts with, just passes audio through
    constexpr const char* StarterCode =
        "veksel_main\n"
        "{\n"
        "    return input;\n"
        "}\n";

    constexpr const char* OpenTitle     = "Open File";
    constexpr const char* SaveTitle     = "Save File";
    constexpr const char* SettingsTitle = "Audio Settings";

    // pick a colour from the tag in a log line
    ImVec4 lineColor(const std::string& line)
    {
        if (line.find("[error]") != std::string::npos) return {1.0f, 0.4f, 0.4f, 1.0f};
        if (line.find("[ok]")    != std::string::npos) return {0.4f, 0.9f, 0.4f, 1.0f};
        if (line.find("[warn]")  != std::string::npos) return {1.0f, 0.85f, 0.3f, 1.0f};
        if (line.find("[sys]")   != std::string::npos) return ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
        return ImGui::GetStyleColorVec4(ImGuiCol_Text);
    }

    // combo over any list. value is the setting being edited, getValue and
    // getLabel turn an item into its value and its text. if value isn't in
    // the list (device changed, nothing chosen yet) it snaps to the first item
    template <typename Items, typename ValueFn, typename LabelFn>
    void combo(const char* id, int& value, const Items& items,
               ValueFn getValue, LabelFn getLabel, float width)
    {
        if (items.empty())
            return;

        auto it = std::find_if(items.begin(), items.end(),
            [&](const auto& item) { return getValue(item) == value; });

        if (it == items.end())
        {
            it = items.begin();
            value = getValue(*it);
        }

        ImGui::SetNextItemWidth(width);

        if (ImGui::BeginCombo(id, getLabel(*it).c_str()))
        {
            for (const auto& item : items)
            {
                ImGui::PushID(getValue(item));

                if (ImGui::Selectable(getLabel(item).c_str(), getValue(item) == value))
                    value = getValue(item);

                ImGui::PopID();
            }

            ImGui::EndCombo();
        }
    }

    // 0, step, 2*step ... below count
    std::vector<int> range(int count, int step)
    {
        std::vector<int> v;
        for (int i = 0; i < count; i += step)
            v.push_back(i);
        return v;
    }

    // channel count of the device with this portaudio index, 0 if not found
    int channelsOf(const std::vector<AudioDevice>& list, int index)
    {
        for (const auto& d : list)
            if (d.index == index)
                return d.channels;
        return 0;
    }

    void applyTheme()
    {
        ImGui::StyleColorsDark();

        const ImVec4 background = {0.02f, 0.02f, 0.02f, 1.0f};
        const ImVec4 panel      = {0.08f, 0.08f, 0.08f, 1.0f};
        const ImVec4 control    = {0.14f, 0.14f, 0.14f, 1.0f};
        const ImVec4 border     = {0.22f, 0.22f, 0.22f, 1.0f};
        const ImVec4 accent     = {1.00f, 0.41f, 0.71f, 1.0f};   // hotpink, #ff69b4

        // same accent, just dimmer, for things that are only hovered
        const ImVec4 accentDim  = {accent.x * 0.6f, accent.y * 0.6f, accent.z * 0.6f, 1.0f};

        ImVec4* colors = ImGui::GetStyle().Colors;
        colors[ImGuiCol_WindowBg]      = background;
        colors[ImGuiCol_ChildBg]       = panel;
        colors[ImGuiCol_PopupBg]       = panel;
        colors[ImGuiCol_TitleBg]       = panel;
        colors[ImGuiCol_TitleBgActive] = panel;
        colors[ImGuiCol_Border]        = border;

        colors[ImGuiCol_FrameBg]        = control;
        colors[ImGuiCol_FrameBgHovered] = control;
        colors[ImGuiCol_FrameBgActive]  = control;

        colors[ImGuiCol_Button]        = control;
        colors[ImGuiCol_ButtonHovered] = accent;
        colors[ImGuiCol_ButtonActive]  = accentDim;

        // selected and hovered rows in the combo lists
        colors[ImGuiCol_Header]        = accentDim;
        colors[ImGuiCol_HeaderHovered] = accent;
        colors[ImGuiCol_HeaderActive]  = accent;

        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowRounding = 5.0f;
        style.ChildRounding  = 5.0f;
        style.FrameRounding  = 5.0f;
        style.PopupRounding  = 5.0f;
        style.WindowPadding  = {10.0f, 10.0f};
        style.FramePadding   = {8.0f, 4.0f};
    }
}

UI::UI(Engine& engine, Audio& audio, Log& log)
    : m_engine(engine), m_audio(audio), m_log(log)
{
    ImGuiIO& io = ImGui::GetIO();
    ImGui::GetStyle().FontSizeBase = 13.0f;
    applyTheme();
    io.Fonts->AddFontDefaultVector();

    // just ascii and the box drawing blocks the logo needs
    static const ImWchar ranges[] = { 0x20, 0x7F, 0x2500, 0x26FF, 0 };

    const auto fontPath = std::filesystem::path(VEKSEL_ASSET_DIR) / "DejaVuSansMono.ttf";

    if (std::filesystem::exists(fontPath))
        m_logoFont = io.Fonts->AddFontFromFileTTF(fontPath.string().c_str(), 16.0f, nullptr, ranges);
    else
        m_log.write("[warn] Font not found: " + fontPath.string());

    // modules are c, so highlight them as c
    m_textEditor.SetLanguage(TextEditor::LookupLanguage("C"));

    // language is const, cast it away so veksel_main and input look like known names
    if (auto* lang = const_cast<TextEditor::Language*>(m_textEditor.GetLanguage()))
    {
        lang->identifiers.insert("veksel_main");
        lang->identifiers.insert("input");
    }

    m_textEditor.SetPalette(TextEditor::GetDarkPalette());
    m_textEditor.SetText(StarterCode);
    m_engine.submitSource(StarterCode);
}

void UI::render()
{
    // ctrl+r, or cmd+r on mac (imgui already swaps cmd into KeyCtrl there)
    const ImGuiIO& io = ImGui::GetIO();
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_R, false))
        compile();

    // one window that fills the whole viewport
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);

    ImGui::Begin(
        "VekselMain",
        nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoBringToFrontOnFocus
    );

    // open any popup a button asked for last frame, has to happen here
    // so the popup ids match the ones the modals use below
    if (m_openPopup)
    {
        ImGui::OpenPopup(m_openPopup);
        m_openPopup = nullptr;
    }

    drawHeader();
    drawEditor();
    drawLog();

    drawFileModal(OpenTitle, false);
    drawFileModal(SaveTitle, true);
    drawSettingsModal();

    ImGui::End();
}

void UI::drawHeader()
{
    const ImGuiStyle& style = ImGui::GetStyle();

    // measure the logo with its own font so the header fits it
    if (m_logoFont) ImGui::PushFont(m_logoFont, 9.0f);

    const float headerH = ImGui::CalcTextSize(Logo).y + style.WindowPadding.y * 2.0f;

    ImGui::BeginChild("##header", {0.0f, headerH}, ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding);
    ImGui::TextUnformatted(Logo);

    if (m_logoFont) ImGui::PopFont();

    // five same size buttons, pushed to the right and centred vertically
    constexpr float btnW = 90.0f;
    const float btnH = ImGui::GetFrameHeight();
    const float totalW = btnW * 5.0f + style.ItemSpacing.x * 4.0f;

    ImGui::SetCursorPos({
        ImGui::GetWindowWidth() - totalW - style.WindowPadding.x,
        (ImGui::GetWindowHeight() - btnH) * 0.5f
    });

    if (CustomButton("Open", {btnW, btnH}))
        m_openPopup = OpenTitle;

    ImGui::SameLine();
    if (CustomButton("Save", {btnW, btnH}))
        m_openPopup = SaveTitle;

    ImGui::SameLine();
    if (CustomButton("Compile", {btnW, btnH}))
        compile();

    ImGui::SameLine();
    if (CustomButton(m_audio.isRunning() ? "Audio On" : "Audio Off", {btnW, btnH}))
        toggleAudio();

    ImGui::SameLine();
    if (CustomButton("Settings", {btnW, btnH}))
        m_openPopup = SettingsTitle;

    ImGui::EndChild();
}

void UI::drawEditor()
{
    // editor gets 3/4 of what's left, the log takes the rest
    const float height = ImGui::GetContentRegionAvail().y * 0.75f;

    ImGui::BeginChild("##editor", {-1.0f, height}, ImGuiChildFlags_Borders);
    m_textEditor.Render("##source", ImVec2(-1, -1), false);
    ImGui::EndChild();
}

void UI::drawLog()
{
    if (ImGui::BeginChild("##log", {-1.0f, -1.0f}, ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding))
    {
        for (const auto& line : m_log.getLines())
            ImGui::TextColored(lineColor(line), "%s", line.c_str());

        // follow new lines, unless you've scrolled up to read
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
            ImGui::SetScrollHereY(1.0f);
    }

    ImGui::EndChild();
}

void UI::drawFileModal(const char* title, bool saving)
{
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, {0.5f, 0.5f});

    if (!ImGui::BeginPopupModal(title, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        return;

    ImGui::TextUnformatted("Path");

    // start typing straight away
    if (ImGui::IsWindowAppearing())
        ImGui::SetKeyboardFocusHere();

    ImGui::SetNextItemWidth(360.0f);
    const bool enter = ImGui::InputText("##path", m_path, sizeof(m_path), ImGuiInputTextFlags_EnterReturnsTrue);

    if (CustomButton(saving ? "Save" : "Open", {100.0f, 0.0f}) || enter)
    {
        if (saving) saveFile();
        else        loadFile();

        ImGui::CloseCurrentPopup();
    }

    ImGui::SameLine();

    // modals don't close on escape by themselves
    if (CustomButton("Cancel", {100.0f, 0.0f}) || ImGui::IsKeyPressed(ImGuiKey_Escape, false))
        ImGui::CloseCurrentPopup();

    ImGui::EndPopup();
}

void UI::drawSettingsModal()
{
    constexpr float width = 360.0f;

    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, {0.5f, 0.5f});
    ImGui::SetNextWindowSize({width, 0.0f});

    if (!ImGui::BeginPopupModal(SettingsTitle, nullptr, ImGuiWindowFlags_NoResize))
        return;

    // refresh devices and start from what the stream is using now
    if (ImGui::IsWindowAppearing())
    {
        m_devices = m_audio.getDevices();
        m_wanted  = m_audio.getSettings();
    }

    AudioSettings& w = m_wanted;
    const float comboW = width - ImGui::GetStyle().WindowPadding.x * 2.0f;

    const auto devIndex = [](const AudioDevice& d) { return d.index; };
    const auto devName  = [](const AudioDevice& d) { return d.name; };
    const auto self     = [](int v) { return v; };
    const auto number   = [](int v) { return std::to_string(v); };
    const auto hz       = [](int v) { return std::to_string(v) + " Hz"; };

    ImGui::TextUnformatted("Input Device");
    combo("##in", w.inputDevice, m_devices.inputs, devIndex, devName, comboW);

    ImGui::TextUnformatted("Input Channel");
    combo("##inch", w.inputChannel, range(channelsOf(m_devices.inputs, w.inputDevice), 1),
          self, [](int v) { return std::to_string(v + 1); }, comboW);

    ImGui::TextUnformatted("Output Device");
    combo("##out", w.outputDevice, m_devices.outputs, devIndex, devName, comboW);

    // outputs go in pairs "1-2", "3-4". an odd last channel goes out alone
    const int outCount = channelsOf(m_devices.outputs, w.outputDevice);

    ImGui::TextUnformatted("Output Channels");
    combo("##outch", w.outputChannel, range(outCount, 2), self,
          [outCount](int v)
          {
              return v + 1 < outCount
                  ? std::to_string(v + 1) + "-" + std::to_string(v + 2)
                  : std::to_string(v + 1);
          }, comboW);

    ImGui::TextUnformatted("Sample Rate");
    combo("##rate", w.sampleRate, m_devices.sampleRates, self, hz, comboW);

    ImGui::TextUnformatted("Buffer Size");
    combo("##buffer", w.bufferSize, m_devices.bufferSizes, self, number, comboW);

    ImGui::Spacing();

    // apply is only useful if audio is off (so it can start) or something differs
    const bool changed = !m_audio.isRunning() || w != m_audio.getSettings();
    const float btnW = (comboW - ImGui::GetStyle().ItemSpacing.x) * 0.5f;

    // audio logs its own errors. if it fails, audio stays off
    ImGui::BeginDisabled(!changed);
    if (CustomButton("Apply", {btnW, 0.0f}) && m_audio.start(m_engine, w))
        ImGui::CloseCurrentPopup();
    ImGui::EndDisabled();

    ImGui::SameLine();

    if (CustomButton("Cancel", {btnW, 0.0f}) || ImGui::IsKeyPressed(ImGuiKey_Escape, false))
        ImGui::CloseCurrentPopup();

    ImGui::EndPopup();
}

void UI::compile()
{
    m_engine.submitSource(m_textEditor.GetText());
}

void UI::toggleAudio()
{
    if (m_audio.isRunning())
    {
        m_audio.stop();
        m_log.write("[warn] Audio stopped.");
        return;
    }

    // start again with whatever settings it had last
    m_audio.start(m_engine, m_audio.getSettings());
}

void UI::loadFile()
{
    std::ifstream in(m_path);

    if (!in)
    {
        m_log.write(std::string("[error] Could not open: ") + m_path);
        return;
    }

    std::ostringstream text;
    text << in.rdbuf();

    // compile straight away so what you hear matches what you loaded
    m_textEditor.SetText(text.str());
    compile();

    m_log.write(std::string("[ok] Loaded ") + m_path);
}

void UI::saveFile()
{
    std::ofstream out(m_path);
    out << m_textEditor.GetText();

    if (!out)
    {
        m_log.write(std::string("[error] Could not save: ") + m_path);
        return;
    }

    m_log.write(std::string("[ok] Saved ") + m_path);
}