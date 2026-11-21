#include "UI.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <string>

using namespace veksel;

namespace
{
    namespace fs = std::filesystem;

    constexpr float kMinEditor       = 80.0f;
    constexpr float kLogHeight       = 250.0f;
    constexpr float kHeaderHeight    = 20.0f;
    constexpr const char* kPresetDir = "presets";

    // rgb is packed as 0x00rrggbb
    ImVec4 RgbToVec4(ImU32 rgb)
    {
        return ImVec4(((rgb >> 16) & 0xff) / 255.0f, ((rgb >> 8) & 0xff) / 255.0f, (rgb & 0xff) / 255.0f, 1.0f);
    }

    // texteditor palette expects 0xaabbggrr
    ImU32 RgbToAbgr(ImU32 rgb)
    {
        return 0xff000000 | ((rgb & 0xff) << 16) | (rgb & 0xff00) | ((rgb >> 16) & 0xff);
    }

    const AudioDevice* findDevice(const std::vector<AudioDevice>& list, int index)
    {
        auto it = std::find_if(list.begin(), list.end(), [&](const AudioDevice& d) { return d.index == index; });
        return it == list.end() ? nullptr : &*it;
    }

    bool deviceCombo(const char* id, const std::vector<AudioDevice>& list, int& index)
    {
        const AudioDevice* cur = findDevice(list, index);
        bool changed = false;

        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo(id, cur ? cur->name.c_str() : "Default"))
        {
            if (ImGui::Selectable("Default", index < 0))
            {
                index = -1;
                changed = true;
            }
            for (const auto& d : list)
            {
                ImGui::PushID(d.index);   // names can repeat
                if (ImGui::Selectable(d.name.c_str(), d.index == index))
                {
                    index = d.index;
                    changed = true;
                }
                ImGui::PopID();
            }
            ImGui::EndCombo();
        }
        return changed;
    }

    // pair: output channels are chosen as stereo pairs (1-2, 3-4, ...)
    void channelCombo(const char* id, int& channel, int count, bool pair)
    {
        auto label = [&](int c)
        {
            std::string s = std::to_string(c + 1);
            return (pair && c + 1 < count) ? s + "-" + std::to_string(c + 2) : s;
        };

        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo(id, label(channel).c_str()))
        {
            for (int c = 0; c < std::max(count, 1); c += pair ? 2 : 1)
            {
                if (ImGui::Selectable(label(c).c_str(), c == channel))
                {
                    channel = c;
                }
            }
            ImGui::EndCombo();
        }
    }

    void intCombo(const char* id, int& value, const std::vector<int>& options)
    {
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo(id, std::to_string(value).c_str()))
        {
            for (int o : options)
            {
                if (ImGui::Selectable(std::to_string(o).c_str(), o == value))
                {
                    value = o;
                }
            }
            ImGui::EndCombo();
        }
    }

    ImVec4 logLineColor(const std::string& line, const ImVec4& defaultColor)
    {
        if (line.find("error") != std::string::npos)
        {
            return {0.95f, 0.30f, 0.30f, 1.0f};
        }
        if (line.find("[ok]") != std::string::npos)
        {
            return {0.30f, 0.85f, 0.40f, 1.0f};
        }
        return defaultColor;
    }

    std::string sanitizePresetName(const std::string& rawName)
    {
        std::string out;
        for (unsigned char ch : rawName)
        {
            if (std::isalnum(ch) || ch == '-' || ch == '_')
            {
                out.push_back(static_cast<char>(ch));
            }
            else if (std::isspace(ch))
            {
                out.push_back('_');
            }
        }
        return out.empty() ? "untitled" : out;
    }

    std::string trimLeft(const std::string& s)
    {
        const auto i = s.find_first_not_of(' ');
        return i == std::string::npos ? "" : s.substr(i);
    }

    bool savePresetFile(const std::string& name, const std::string& description, const std::string& code)
    {
        std::error_code ec;
        fs::create_directories(kPresetDir, ec);
        if (ec)
        {
            return false;
        }

        std::ofstream file(fs::path(kPresetDir) / (sanitizePresetName(name) + ".veksel"));
        if (!file)
        {
            return false;
        }

        // keep preset metadata in comments so files stay valid c code
        file << "// name: " << name << "\n// description: " << description << "\n\n" << code;
        if (!code.empty() && code.back() != '\n')
        {
            file << '\n';
        }
        return file.good();
    }

    std::vector<PresetEntry> loadPresetList()
    {
        std::vector<PresetEntry> entries;
        std::error_code ec;

        for (const auto& entry : fs::directory_iterator(kPresetDir, ec))
        {
            std::ifstream file(entry.path());
            if (!entry.is_regular_file(ec) || entry.path().extension() != ".veksel" || !file)
            {
                continue;
            }

            PresetEntry preset;
            preset.path = entry.path();
            preset.name = entry.path().stem().string();

            std::string line;
            while (std::getline(file, line))
            {
                if (line.rfind("// name:", 0) == 0)
                {
                    preset.name = trimLeft(line.substr(8));
                }
                else if (line.rfind("// description:", 0) == 0)
                {
                    preset.description = trimLeft(line.substr(15));
                }
                else if (!(preset.code.empty() && line.empty()))   // skip blanks after header
                {
                    preset.code += line + "\n";
                }
            }
            entries.push_back(std::move(preset));
        }
        return entries;
    }

    // opens (when flagged) and begins a centered modal; pair with endModal()
    bool beginModal(const char* name, bool& openFlag)
    {
        if (openFlag)
        {
            ImGui::OpenPopup(name);
            openFlag = false;
        }

        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, {0.5f, 0.5f});
        ImGui::SetNextWindowSize({400.f, 0}, ImGuiCond_Appearing);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {20, 20});
        const bool shown = ImGui::BeginPopupModal(name, nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
        ImGui::PopStyleVar();

        if (shown)
        {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 5});
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {10, 10});
        }
        return shown;
    }

    void endModal()
    {
        ImGui::PopStyleVar(2);
        ImGui::EndPopup();
    }

    // optional action button next to a close button; returns true when the action was clicked
    bool actionRow(const char* action)
    {
        bool clicked = false;
        if (action)
        {
            const float w = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x * 2.0f) * 0.5f;
            clicked = ImGui::Button(action, {w, 25.0f});
            ImGui::SameLine();
        }
        if (ImGui::Button("Close", {-FLT_MIN, 25.0f}))
        {
            ImGui::CloseCurrentPopup();
        }
        return clicked;
    }
}

UI::UI(Engine& e, Audio& a, Log& l) : m_engine(e), m_audio(a), m_log(l)
{
    ImGui::GetStyle().FontSizeBase = 13.0f;
    ImGui::GetIO().Fonts->AddFontDefaultBitmap();
    ImGui::StyleColorsDark();

    constexpr ImU32 panelHex  = 0x141414;
    constexpr ImU32 borderHex = 0x383838;

    m_textEditor.SetLanguage(TextEditor::LookupLanguage("C"));
    TextEditor::Palette p = TextEditor::GetDarkPalette();
    p[static_cast<int>(TextEditor::Color::background)] = RgbToAbgr(panelHex);
    m_textEditor.SetPalette(p);
    m_textEditor.SetShowWhitespacesEnabled(false);

    const char* defaultCode = "float process_sample(float input)\n{\n    return input;\n}";
    m_textEditor.SetText(defaultCode);
    m_engine.submitSource(defaultCode);

    ImGuiStyle& style = ImGui::GetStyle();
    style.Colors[ImGuiCol_Border]   = RgbToVec4(borderHex);
    style.Colors[ImGuiCol_WindowBg] = RgbToVec4(panelHex);
    style.WindowBorderSize = 0.0f;
    style.ChildBorderSize  = 1.0f;
    style.ChildRounding    = 0.0f;
}

void UI::render()
{
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {0, 0});
    ImGui::Begin("VekselMain", nullptr, ImGuiWindowFlags_NoDecoration);
    drawHeader();
    drawEditor();
    drawLog();
    drawSettings();
    drawSavePreset();
    drawLoadPreset();
    ImGui::End();
    ImGui::PopStyleVar(3);
}

void UI::drawSettings()
{
    if (m_openSettings)
    {
        m_devices = m_audio.getDevices();
        m_wanted  = m_audio.getSettings();
    }

    if (!beginModal("Settings", m_openSettings))
    {
        return;
    }

    const AudioDevice* in  = findDevice(m_devices.inputs,  m_wanted.inputDevice);
    const AudioDevice* out = findDevice(m_devices.outputs, m_wanted.outputDevice);

    ImGui::TextUnformatted("Input device");
    if (deviceCombo("##indev", m_devices.inputs, m_wanted.inputDevice))
    {
        m_wanted.inputChannel = 0;
    }

    ImGui::Spacing();
    ImGui::TextUnformatted("Input channel");
    channelCombo("##inch", m_wanted.inputChannel, in ? in->channels : 0, false);

    ImGui::Spacing();
    ImGui::TextUnformatted("Output device");
    if (deviceCombo("##outdev", m_devices.outputs, m_wanted.outputDevice))
    {
        m_wanted.outputChannel = 0;
    }

    ImGui::Spacing();
    ImGui::TextUnformatted("Output channels");
    channelCombo("##outch", m_wanted.outputChannel, out ? out->channels : 0, true);

    ImGui::Spacing();
    ImGui::TextUnformatted("Sample rate");
    intCombo("##sr", m_wanted.sampleRate, m_devices.sampleRates);

    ImGui::Spacing();
    ImGui::TextUnformatted("Buffer size");
    intCombo("##buf", m_wanted.bufferSize, m_devices.bufferSizes);

    ImGui::Spacing();
    if (actionRow("Apply"))
    {
        m_audio.start(m_engine, m_wanted);
    }
    endModal();
}

void UI::drawSavePreset()
{
    if (!beginModal("Save Preset", m_openSavePreset))
    {
        return;
    }

    ImGui::TextUnformatted("Name");
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputText("##preset_name", m_presetNameBuf, sizeof(m_presetNameBuf));

    ImGui::Spacing();
    ImGui::TextUnformatted("Description");
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputText("##preset_desc", m_presetDescBuf, sizeof(m_presetDescBuf));

    ImGui::Spacing();
    if (actionRow("Save"))
    {
        const bool ok = savePresetFile(m_presetNameBuf, m_presetDescBuf, m_textEditor.GetText());
        m_log.write(ok ? "[ok] saved preset" : "[error] failed to save preset");
        m_logCollapsed = false;
        ImGui::CloseCurrentPopup();
    }
    endModal();
}

void UI::drawLoadPreset()
{
    if (m_openLoadPreset)
    {
        m_cachedPresets = loadPresetList();
    }

    if (!beginModal("Load Preset", m_openLoadPreset))
    {
        return;
    }

    if (m_cachedPresets.empty())
    {
        ImGui::TextUnformatted("No .veksel presets found in presets/");
    }
    else
    {
        ImGui::BeginChild("##preset_list", {-FLT_MIN, 180.0f}, ImGuiChildFlags_Borders);
        for (const auto& preset : m_cachedPresets)
        {
            if (ImGui::Selectable(preset.name.c_str()))
            {
                m_textEditor.SetText(preset.code);
                m_engine.submitSource(preset.code);
                std::snprintf(m_presetNameBuf, sizeof(m_presetNameBuf), "%s", preset.name.c_str());
                std::snprintf(m_presetDescBuf, sizeof(m_presetDescBuf), "%s", preset.description.c_str());
                m_log.write("[sys] loaded " + preset.path.filename().string());
                ImGui::CloseCurrentPopup();
            }
            if (!preset.description.empty() && ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("%s", preset.description.c_str());
            }
        }
        ImGui::EndChild();
    }

    ImGui::Spacing();
    actionRow(nullptr);
    endModal();
}

// allman style, lowercase comments
void UI::drawHeader()
{
    ImGui::BeginChild("Header", {-FLT_MIN, 25}, ImGuiChildFlags_Borders);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {10, 0});

    if (ImGui::Button("Settings", {0, -FLT_MIN}))
    {
        m_openSettings = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Save", {0, -FLT_MIN}))
    {
        m_openSavePreset = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Load", {0, -FLT_MIN}))
    {
        m_openLoadPreset = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Compile", {0, -FLT_MIN}))
    {
        m_engine.submitSource(m_textEditor.GetText());
        m_logCollapsed = false;
    }

    ImGui::PopStyleVar();
    ImGui::EndChild();
}

void UI::drawEditor()
{
    // +1.0f compensates for overlapping 1.0f with header bottom border
    const float logH    = m_logCollapsed ? kHeaderHeight : kLogHeight;
    const float editorH = std::max(kMinEditor, ImGui::GetContentRegionAvail().y - logH + 1.0f);

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 10});
    ImGui::BeginChild("Editor", {-FLT_MIN, editorH}, ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding);
    m_textEditor.Render("TextEditor", {-FLT_MIN, -FLT_MIN}, false);
    ImGui::EndChild();
    ImGui::PopStyleVar();
}

void UI::drawLog()
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
    // overlap top border by 1.0f with editor bottom border; -FLT_MIN fills remaining height
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 1.0f);
    ImGui::BeginChild("Log", {-FLT_MIN, -FLT_MIN}, ImGuiChildFlags_Borders);

    ImGui::BeginChild("LogHeader", {-FLT_MIN, kHeaderHeight});
    if (ImGui::Button(m_logCollapsed ? "^" : "v", {-1, kHeaderHeight}))
    {
        m_logCollapsed = !m_logCollapsed;
    }
    if (!m_logCollapsed)
    {
        const ImVec2 p = ImGui::GetWindowPos();
        const float  y = p.y + kHeaderHeight - 0.5f;
        ImGui::GetWindowDrawList()->AddLine({p.x, y}, {p.x + ImGui::GetWindowWidth(), y}, ImGui::GetColorU32(ImGuiCol_Border));
    }
    ImGui::EndChild();

    if (!m_logCollapsed)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {10, 10});
        ImGui::BeginChild("LogBody", {-FLT_MIN, -FLT_MIN}, ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_HorizontalScrollbar);

        const auto lines = m_log.getLines();
        const ImVec4 defaultColor = ImGui::GetStyleColorVec4(ImGuiCol_Text);

        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(lines.size()));
        while (clipper.Step())
        {
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, logLineColor(lines[i], defaultColor));
                ImGui::TextUnformatted(lines[i].c_str());
                ImGui::PopStyleColor();
            }
        }
        clipper.End();

        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
        {
            ImGui::SetScrollHereY(1.0f);
        }

        ImGui::EndChild();
        ImGui::PopStyleVar();
    }

    ImGui::EndChild();
    ImGui::PopStyleVar();
}