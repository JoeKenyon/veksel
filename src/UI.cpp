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

    // pick a colour from the tag at the start of a log line
    ImVec4 lineColor(const std::string& line)
    {
        if (line.find("[error]") != std::string::npos) return {1.0f, 0.4f, 0.4f, 1.0f};
        if (line.find("[ok]")    != std::string::npos) return {0.4f, 0.9f, 0.4f, 1.0f};
        if (line.find("[warn]")  != std::string::npos) return {1.0f, 0.85f, 0.3f, 1.0f};
        if (line.find("[sys]")   != std::string::npos) return ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
        return ImGui::GetStyleColorVec4(ImGuiCol_Text);
    }

    // combo box for any list. getLabel turns an item into the text to show
    template <typename Items, typename LabelFn>
    bool combo(const char* id, int& current, const Items& items, LabelFn getLabel, float width)
    {
        const int count = static_cast<int>(items.size());
        const std::string preview = current < count ? getLabel(items[current]) : "";

        bool changed = false;
        ImGui::SetNextItemWidth(width);

        if (ImGui::BeginCombo(id, preview.c_str()))
        {
            for (int i = 0; i < count; ++i)
            {
                ImGui::PushID(i);

                if (ImGui::Selectable(getLabel(items[i]).c_str(), i == current))
                {
                    current = i;
                    changed = true;
                }

                ImGui::PopID();
            }

            ImGui::EndCombo();
        }

        return changed;
    }

    // index of the first item whose key matches value, or 0 if none do
    template <typename Items, typename KeyFn>
    int indexOf(const Items& items, int value, KeyFn getKey)
    {
        for (size_t i = 0; i < items.size(); ++i)
            if (getKey(items[i]) == value)
                return static_cast<int>(i);

        return 0;
    }

    // "1", "2", ... one label per input channel
    std::vector<std::string> inputChannelLabels(int count)
    {
        std::vector<std::string> labels;

        for (int c = 1; c <= count; ++c)
            labels.push_back(std::to_string(c));

        return labels;
    }

    // outputs go in pairs ("1-2", "3-4"). an odd channel left over goes out on its own
    std::vector<std::string> outputPairLabels(int count)
    {
        std::vector<std::string> labels;

        for (int c = 1; c <= count; c += 2)
        {
            labels.push_back(c < count
                ? std::to_string(c) + "-" + std::to_string(c + 1)
                : std::to_string(c));
        }

        return labels;
    }

    void applyTheme()
    {
        ImGui::StyleColorsDark();

        const ImVec4 background = {1.00f, 0.41f, 0.71f, 1.0f}; 
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
        colors[ImGuiCol_Border]        = background;

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
        style.WindowRounding = 5.0f;    // main window fills the viewport
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

    if (ImGui::Button("Open", {btnW, btnH}))
        m_openPopup = OpenTitle;

    ImGui::SameLine();
    if (ImGui::Button("Save", {btnW, btnH}))
        m_openPopup = SaveTitle;

    ImGui::SameLine();
    if (ImGui::Button("Compile", {btnW, btnH}))
        compile();

    ImGui::SameLine();
    if (ImGui::Button(m_audio.isRunning() ? "Audio On" : "Audio Off", {btnW, btnH}))
        toggleAudio();

    ImGui::SameLine();
    if (ImGui::Button("Settings", {btnW, btnH}))
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
        const auto lines = m_log.getLines();

        for (const auto& line : lines)
            ImGui::TextColored(lineColor(line), "%s", line.c_str());

        // snap to the bottom whenever new lines show up
        if (lines.size() != m_lastLogCount)
        {
            m_lastLogCount = lines.size();
            ImGui::SetScrollHereY(1.0f);
        }
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

    if (ImGui::Button(saving ? "Save" : "Open", {100.0f, 0.0f}) || enter)
    {
        if (saving) saveFile();
        else        loadFile();

        ImGui::CloseCurrentPopup();
    }

    ImGui::SameLine();

    // modals don't close on escape by themselves
    if (ImGui::Button("Cancel", {100.0f, 0.0f}) || ImGui::IsKeyPressed(ImGuiKey_Escape, false))
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

    // refresh the device list each time the popup opens and
    // preselect whatever the stream is using right now
    if (ImGui::IsWindowAppearing())
    {
        m_devices = m_audio.getDevices();
        const AudioSettings cur = m_audio.getSettings();

        m_inputIdx      = indexOf(m_devices.inputs,      cur.inputDevice,  [](const AudioDevice& d) { return d.index; });
        m_outputIdx     = indexOf(m_devices.outputs,     cur.outputDevice, [](const AudioDevice& d) { return d.index; });
        m_rateIdx       = indexOf(m_devices.sampleRates, cur.sampleRate,   [](int v) { return v; });
        m_bufferIdx     = indexOf(m_devices.bufferSizes, cur.bufferSize,   [](int v) { return v; });
        m_inputChIdx    = cur.inputChannel;
        m_outputPairIdx = cur.outputChannel / 2;
    }

    const float comboW = width - ImGui::GetStyle().WindowPadding.x * 2.0f;

    const auto deviceName = [](const AudioDevice& d) { return d.name; };
    const auto text       = [](const std::string& s) { return s; };
    const auto hz         = [](int v) { return std::to_string(v) + " Hz"; };
    const auto number     = [](int v) { return std::to_string(v); };

    // the channel lists depend on the device above them, so each device combo
    // is followed by its channel combo. a device with fewer channels than the
    // last one means the old channel choice can be out of range, so clamp it
    ImGui::TextUnformatted("Input Device");
    combo("##in", m_inputIdx, m_devices.inputs, deviceName, comboW);

    const AudioDevice* inDevice = m_inputIdx < static_cast<int>(m_devices.inputs.size())
        ? &m_devices.inputs[m_inputIdx] : nullptr;
    const auto inLabels = inputChannelLabels(inDevice ? inDevice->channels : 0);
    m_inputChIdx = std::clamp(m_inputChIdx, 0, std::max(0, static_cast<int>(inLabels.size()) - 1));

    ImGui::TextUnformatted("Input Channel");
    combo("##inch", m_inputChIdx, inLabels, text, comboW);

    ImGui::TextUnformatted("Output Device");
    combo("##out", m_outputIdx, m_devices.outputs, deviceName, comboW);

    const AudioDevice* outDevice = m_outputIdx < static_cast<int>(m_devices.outputs.size())
        ? &m_devices.outputs[m_outputIdx] : nullptr;
    const auto outLabels = outputPairLabels(outDevice ? outDevice->channels : 0);
    m_outputPairIdx = std::clamp(m_outputPairIdx, 0, std::max(0, static_cast<int>(outLabels.size()) - 1));

    ImGui::TextUnformatted("Output Channels");
    combo("##outch", m_outputPairIdx, outLabels, text, comboW);

    ImGui::TextUnformatted("Sample Rate");
    combo("##rate", m_rateIdx, m_devices.sampleRates, hz, comboW);

    ImGui::TextUnformatted("Buffer Size");
    combo("##buffer", m_bufferIdx, m_devices.bufferSizes, number, comboW);

    ImGui::Spacing();

    // what the combos currently say. no device in a list means -1, which lets
    // audio pick the default. rates and buffers are fixed lists so those
    // indices are always valid
    AudioSettings wanted;
    wanted.inputDevice   = inDevice  ? inDevice->index  : -1;
    wanted.inputChannel  = m_inputChIdx;
    wanted.outputDevice  = outDevice ? outDevice->index : -1;
    wanted.outputChannel = m_outputPairIdx * 2;
    wanted.sampleRate    = m_devices.sampleRates[m_rateIdx];
    wanted.bufferSize    = m_devices.bufferSizes[m_bufferIdx];

    // apply is only useful if audio is off (so it can start) or something differs
    const bool changed = !m_audio.isRunning() || wanted != m_audio.getSettings();

    const float btnW = (comboW - ImGui::GetStyle().ItemSpacing.x) * 0.5f;

    ImGui::BeginDisabled(!changed);
    if (ImGui::Button("Apply", {btnW, 0.0f}))
    {
        // audio logs its own errors, and puts the old settings back if this fails
        if (m_audio.start(m_engine, wanted))
            ImGui::CloseCurrentPopup();
    }
    ImGui::EndDisabled();

    ImGui::SameLine();

    if (ImGui::Button("Cancel", {btnW, 0.0f}) || ImGui::IsKeyPressed(ImGuiKey_Escape, false))
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