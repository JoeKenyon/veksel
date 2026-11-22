#include "UI.h"

#include "FontMono.h"
#include "FontAwesome.h"
#include "Paths.h"

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

using namespace veksel;

// fontawesome 6 solid, utf-8 encoded
#define ICON_GEAR   "\xef\x80\x93"
#define ICON_SAVE   "\xef\x83\x87"
#define ICON_FOLDER "\xef\x81\xbc"
#define ICON_PLAY   "\xef\x81\x8b"
#define ICON_UP     "\xef\x81\xb7"
#define ICON_DOWN   "\xef\x81\xb8"
#define ICON_X      "\xef\x80\x8d"
#define ICON_CHECK  "\xef\x80\x8c"
#define ICON_VOL    "\xef\x80\xa8"
#define ICON_MUTE   "\xef\x9a\xa9"

namespace
{
    namespace fs = std::filesystem;

    constexpr float kMinEditor = 80, kLogH = 250, kLogHeaderH = 24, kHeaderH = 48, kBtnH = 28;
    constexpr float kIconSize = 13, kLogoSize = 6, kIconGap = 8;
    fs::path presetDir() { return veksel::exeDir() / "presets"; }

    // theme as 0xrrggbb
    constexpr ImU32 kPanel = 0x141414, kPanelAlt = 0x1b1b1b, kBorder = 0x2e2e2e;
    constexpr ImU32 kBtn = 0x262626, kBtnHi = 0x323232, kBtnLo = 0x3c3c3c;
    constexpr ImU32 kAccent = 0xff2d95, kAccentHi = 0xff5aae, kAccentLo = 0xd91e7a;
    constexpr ImU32 kText = 0xe0e0e0, kDim = 0x7d7d7d;

    ImFont* g_icons = nullptr;
    ImFont* g_logo  = nullptr;

    ImVec4 vec(ImU32 rgb, float a = 1.0f)
    {
        return {((rgb >> 16) & 0xff) / 255.0f, ((rgb >> 8) & 0xff) / 255.0f, (rgb & 0xff) / 255.0f, a};
    }

    // drawlist + texteditor want 0xaabbggrr
    ImU32 abgr(ImU32 rgb)
    {
        return 0xff000000 | ((rgb & 0xff) << 16) | (rgb & 0xff00) | ((rgb >> 16) & 0xff);
    }

    void applyTheme()
    {
        ImGuiStyle& s = ImGui::GetStyle();
        ImGui::StyleColorsDark();

        s.ChildBorderSize = s.PopupBorderSize = 1;
        s.WindowBorderSize = s.FrameBorderSize = 0;
        s.WindowRounding = s.ChildRounding = 0;
        s.PopupRounding = 6;
        s.FrameRounding = s.GrabRounding = 4;
        s.ScrollbarRounding = 6;
        s.FramePadding = {10, 5};
        s.ScrollbarSize = 12;

        struct C { int id; ImU32 rgb; float a; };
        static const C colors[] =
        {
            {ImGuiCol_Text, kText, 1},             {ImGuiCol_TextDisabled, kDim, 1},
            {ImGuiCol_WindowBg, kPanel, 1},        {ImGuiCol_ChildBg, kPanel, 1},
            {ImGuiCol_PopupBg, kPanelAlt, 1},      {ImGuiCol_Border, kBorder, 1},
            {ImGuiCol_Separator, kBorder, 1},      {ImGuiCol_FrameBg, kPanel, 1},
            {ImGuiCol_FrameBgHovered, kBtn, 1},    {ImGuiCol_FrameBgActive, kBtnHi, 1},
            {ImGuiCol_Button, kBtn, 1},            {ImGuiCol_ButtonHovered, kBtnHi, 1},
            {ImGuiCol_ButtonActive, kBtnLo, 1},    {ImGuiCol_Header, kAccent, 0.30f},
            {ImGuiCol_HeaderHovered, kAccent, 0.50f}, {ImGuiCol_HeaderActive, kAccent, 0.70f},
            {ImGuiCol_ScrollbarBg, kPanel, 1},     {ImGuiCol_ScrollbarGrab, kBtnHi, 1},
            {ImGuiCol_ScrollbarGrabHovered, kBtnLo, 1}, {ImGuiCol_ScrollbarGrabActive, kAccent, 1},
            {ImGuiCol_TextSelectedBg, kAccent, 0.35f}, {ImGuiCol_NavCursor, kAccent, 1},
            {ImGuiCol_TitleBg, kAccent, 1},        {ImGuiCol_TitleBgActive, kAccent, 1},
            {ImGuiCol_TitleBgCollapsed, kAccent, 1},
        };
        for (const C& c : colors)
        {
            s.Colors[c.id] = vec(c.rgb, c.a);
        }
        s.Colors[ImGuiCol_ModalWindowDimBg] = {0, 0, 0, 0.6f};
    }

    // icon width, 0 if the font is missing
    float iconW(const char* icon)
    {
        if (!g_icons)
        {
            return 0;
        }
        ImGui::PushFont(g_icons, kIconSize);
        const float w = ImGui::CalcTextSize(icon).x;
        ImGui::PopFont();
        return w;
    }

    float contentW(const char* icon, const char* label)
    {
        const float i = iconW(icon);
        return i + (i > 0 ? kIconGap : 0.0f) + ImGui::CalcTextSize(label).x;
    }

    float btnW(const char* icon, const char* label)
    {
        return contentW(icon, label) + ImGui::GetStyle().FramePadding.x * 2.0f;
    }

    // button with icon + label drawn by hand so both sit dead centre vertically
    bool iconButton(const char* icon, const char* label, ImVec2 size, ImU32 col = 0, bool left = false)
    {
        const float w    = contentW(icon, label);
        const float padX = ImGui::GetStyle().FramePadding.x;
        if (size.x == 0)
        {
            size.x = btnW(icon, label);
        }

        const bool hit = ImGui::Button((std::string("##") + label).c_str(), size);

        const ImVec2 a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
        const float midY = (a.y + b.y) * 0.5f;
        float x = left ? a.x + padX : a.x + std::floor((b.x - a.x - w) * 0.5f);
        if (!col)
        {
            col = ImGui::GetColorU32(ImGuiCol_Text);
        }

        ImDrawList* dl = ImGui::GetWindowDrawList();
        if (const float iw = iconW(icon); iw > 0)
        {
            dl->AddText(g_icons, kIconSize, {std::floor(x), std::floor(midY - kIconSize * 0.5f)}, col, icon);
            x += iw + kIconGap;
        }
        dl->AddText({std::floor(x), std::floor(midY - ImGui::GetFontSize() * 0.5f)}, col, label);
        return hit;
    }

    bool accentButton(const char* icon, const char* label, ImVec2 size)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, vec(kAccent));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, vec(kAccentHi));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, vec(kAccentLo));
        const bool hit = iconButton(icon, label, size, IM_COL32_WHITE);
        ImGui::PopStyleColor(3);
        return hit;
    }

    // multiline ascii logo, centred vertically in the header
    void drawLogo(const char* text, ImU32 col)
    {
        if (!g_logo)
        {
            ImGui::TextUnformatted("VEKSEL");   // no font, plain text
            return;
        }
        ImGui::PushFont(g_logo, kLogoSize);
        const ImVec2 sz  = ImGui::CalcTextSize(text);
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        const float  h   = std::max(ImGui::GetContentRegionAvail().y, sz.y);
        ImGui::Dummy({sz.x, h});
        ImGui::GetWindowDrawList()->AddText(g_logo, kLogoSize, {pos.x, pos.y + std::floor((h - sz.y) * 0.5f)}, col, text);
        ImGui::PopFont();
    }

    void dimLabel(const char* text)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, vec(kDim));
        ImGui::TextUnformatted(text);
        ImGui::PopStyleColor();
    }

    // full width combo, caller fills in the items
    template <class F>
    void combo(const char* id, const std::string& preview, F items)
    {
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo(id, preview.c_str()))
        {
            items();
            ImGui::EndCombo();
        }
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

        auto pick = [&](const char* name, int idx)
        {
            ImGui::PushID(idx);   // names can repeat
            if (ImGui::Selectable(name, idx == index))
            {
                index = idx;
                changed = true;
            }
            ImGui::PopID();
        };

        combo(id, cur ? cur->name : "Default", [&]
        {
            pick("Default", -1);
            for (const auto& d : list)
            {
                pick(d.name.c_str(), d.index);
            }
        });
        return changed;
    }

    // pair = stereo pairs (1-2, 3-4...)
    void channelCombo(const char* id, int& ch, int count, bool pair)
    {
        auto name = [&](int c)
        {
            const std::string s = std::to_string(c + 1);
            return pair && c + 1 < count ? s + "-" + std::to_string(c + 2) : s;
        };

        combo(id, name(ch), [&]
        {
            for (int c = 0; c < std::max(count, 1); c += pair ? 2 : 1)
            {
                if (ImGui::Selectable(name(c).c_str(), c == ch))
                {
                    ch = c;
                }
            }
        });
    }

    void intCombo(const char* id, int& value, const std::vector<int>& options)
    {
        combo(id, std::to_string(value), [&]
        {
            for (int o : options)
            {
                if (ImGui::Selectable(std::to_string(o).c_str(), o == value))
                {
                    value = o;
                }
            }
        });
    }

    ImVec4 logColor(const std::string& line)
    {
        if (line.find("error") != std::string::npos) { return {0.95f, 0.35f, 0.35f, 1}; }
        if (line.find("[ok]") != std::string::npos)  { return {0.35f, 0.85f, 0.45f, 1}; }
        if (line.find("[sys]") != std::string::npos) { return vec(kDim); }
        return vec(kText);
    }

    // presets
    std::string replaceAll(std::string s, const std::string& from, const std::string& to)
    {
        for (size_t i = 0; (i = s.find(from, i)) != std::string::npos; i += to.size())
        {
            s.replace(i, from.size(), to);
        }
        return s;
    }

    std::string cleanName(const std::string& raw)
    {
        std::string out;
        for (unsigned char c : raw)
        {
            if (std::isalnum(c) || c == '-' || c == '_') { out += static_cast<char>(c); }
            else if (std::isspace(c))                    { out += '_'; }
        }
        return out.empty() ? "untitled" : out;
    }

    bool savePreset(const std::string& name, const std::string& desc, const std::string& code)
    {
        std::error_code ec;
        fs::create_directories(presetDir(), ec);
        std::ofstream f(fs::path(presetDir()) / (cleanName(name) + ".veksel"));
        if (ec || !f)
        {
            return false;
        }

        // header lives in comments so the file is still valid c. newlines in desc get stored as \n
        f << "// name: " << name << "\n// description: " << replaceAll(replaceAll(desc, "\r", ""), "\n", "\\n") << "\n\n" << code;
        if (!code.empty() && code.back() != '\n')
        {
            f << '\n';
        }
        return f.good();
    }

    std::vector<PresetEntry> loadPresets()
    {
        std::vector<PresetEntry> out;
        std::error_code ec;

        for (const auto& e : fs::directory_iterator(presetDir(), ec))
        {
            std::ifstream f(e.path());
            if (e.path().extension() != ".veksel" || !f)
            {
                continue;
            }

            PresetEntry p;
            p.path = e.path();
            p.name = e.path().stem().string();

            std::string line;
            auto value = [&](size_t skip)
            {
                const std::string s = line.substr(skip);
                return s.substr(std::min(s.find_first_not_of(' '), s.size()));
            };

            while (std::getline(f, line))
            {
                if (line.rfind("// name:", 0) == 0)             { p.name = value(8); }
                else if (line.rfind("// description:", 0) == 0) { p.description = replaceAll(value(15), "\\n", "\n"); }
                else if (!(p.code.empty() && line.empty()))     { p.code += line + "\n"; }   // skip blanks after header
            }
            out.push_back(std::move(p));
        }

        std::sort(out.begin(), out.end(), [](const PresetEntry& a, const PresetEntry& b) { return a.name < b.name; });
        return out;
    }

    // centred modal with pink title, pair with endModal
    bool beginModal(const char* name, bool& openFlag)
    {
        if (openFlag)
        {
            ImGui::OpenPopup(name);
            openFlag = false;
        }

        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, {0.5f, 0.5f});
        ImGui::SetNextWindowSize({420, 0}, ImGuiCond_Appearing);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {20, 20});
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32_WHITE);   // only for the title, popped right after
        const bool shown = ImGui::BeginPopupModal(name, nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
        ImGui::PopStyleColor();
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

    // optional pink action button + close, returns true if action clicked
    bool actionRow(const char* icon, const char* action)
    {
        ImGui::Separator();
        ImGui::Spacing();

        bool hit = false;
        if (action)
        {
            hit = accentButton(icon, action, {(ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f, kBtnH});
            ImGui::SameLine();
        }
        if (iconButton(ICON_X, "Close", {-FLT_MIN, kBtnH}))
        {
            ImGui::CloseCurrentPopup();
        }
        return hit;
    }
}

UI::UI(Engine& e, Audio& a, Log& l)
    : m_engine(e), m_audio(a), m_log(l)
{
    ImGui::GetStyle().FontSizeBase = 13.0f;

    ImGuiIO& io = ImGui::GetIO();

    // default bitmap font for all normal text
    io.Fonts->AddFontDefaultBitmap();

    // logo font
    ImFontConfig logoConfig;
    logoConfig.FontDataOwnedByAtlas = false;

    g_logo = io.Fonts->AddFontFromMemoryTTF(
        const_cast<unsigned char*>(assets_DejaVuSansMono_ttf),
        assets_DejaVuSansMono_ttf_len,
        kLogoSize,
        &logoConfig
    );

    // font awesome icons
    static const ImWchar iconRanges[] = {
        0xf000, 0xf8ff,
        0
    };

    ImFontConfig iconConfig;
    iconConfig.FontDataOwnedByAtlas = false;

    g_icons = io.Fonts->AddFontFromMemoryTTF(
        const_cast<unsigned char*>(assets_fa_solid_900_ttf),
        assets_fa_solid_900_ttf_len,
        kIconSize,
        &iconConfig,
        iconRanges
    );

    applyTheme();

    m_textEditor.SetLanguage(TextEditor::LookupLanguage("C"));

    TextEditor::Palette p = TextEditor::GetDarkPalette();
    p[static_cast<int>(TextEditor::Color::background)] = abgr(kPanel);

    m_textEditor.SetPalette(p);
    m_textEditor.SetShowWhitespacesEnabled(false);

    const char* code =
        "float process_sample(float input)\n"
        "{\n"
        "    return input;\n"
        "}";

    m_textEditor.SetText(code);
    m_engine.submitSource(code);
}

void UI::render()
{
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);

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

    const AudioDevice* in  = findDevice(m_devices.inputs, m_wanted.inputDevice);
    const AudioDevice* out = findDevice(m_devices.outputs, m_wanted.outputDevice);

    dimLabel("INPUT DEVICE");
    if (deviceCombo("##indev", m_devices.inputs, m_wanted.inputDevice))
    {
        m_wanted.inputChannel = 0;   // channel list changed
    }
    dimLabel("INPUT CHANNEL");
    channelCombo("##inch", m_wanted.inputChannel, in ? in->channels : 0, false);

    ImGui::Spacing();
    dimLabel("OUTPUT DEVICE");
    if (deviceCombo("##outdev", m_devices.outputs, m_wanted.outputDevice))
    {
        m_wanted.outputChannel = 0;
    }
    dimLabel("OUTPUT CHANNELS");
    channelCombo("##outch", m_wanted.outputChannel, out ? out->channels : 0, true);

    ImGui::Spacing();
    dimLabel("SAMPLE RATE");
    intCombo("##sr", m_wanted.sampleRate, m_devices.sampleRates);
    dimLabel("BUFFER SIZE");
    intCombo("##buf", m_wanted.bufferSize, m_devices.bufferSizes);

    ImGui::Spacing();
    if (actionRow(ICON_CHECK, "Apply"))
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

    dimLabel("NAME");
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputText("##name", m_presetNameBuf, sizeof(m_presetNameBuf));

    dimLabel("DESCRIPTION");
    ImGui::InputTextMultiline("##desc", m_presetDescBuf, sizeof(m_presetDescBuf), {-FLT_MIN, 90});

    ImGui::Spacing();
    if (actionRow(ICON_SAVE, "Save"))
    {
        const bool ok = savePreset(m_presetNameBuf, m_presetDescBuf, m_textEditor.GetText());
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
        m_cachedPresets = loadPresets();
    }
    if (!beginModal("Load Preset", m_openLoadPreset))
    {
        return;
    }

    if (m_cachedPresets.empty())
    {
        dimLabel("No .veksel presets found in presets/");
    }
    else
    {
        constexpr float rowH = 38, padX = 12;

        ImGui::BeginChild("##list", {-FLT_MIN, 220}, ImGuiChildFlags_Borders);
        ImDrawList* dl = ImGui::GetWindowDrawList();

        for (size_t i = 0; i < m_cachedPresets.size(); ++i)
        {
            const auto& p = m_cachedPresets[i];

            ImGui::PushID(static_cast<int>(i));
            const ImVec2 pos = ImGui::GetCursorScreenPos();
            const bool clicked = ImGui::Selectable("##row", false, 0, {0, rowH});
            const bool hovered = ImGui::IsItemHovered();
            const ImVec2 rowMax = ImGui::GetItemRectMax();
            ImGui::PopID();

            // row shows first desc line, clipped. full text in the tooltip
            const bool hasDesc = !p.description.empty();
            const size_t nl = p.description.find('\n');
            const std::string first = p.description.substr(0, nl) + (nl == std::string::npos ? "" : " ...");

            dl->PushClipRect(pos, {rowMax.x - padX, rowMax.y}, true);
            dl->AddText({pos.x + padX, pos.y + (hasDesc ? 4.0f : (rowH - ImGui::GetFontSize()) * 0.5f)}, ImGui::GetColorU32(ImGuiCol_Text), p.name.c_str());
            if (hasDesc)
            {
                dl->AddText({pos.x + padX, pos.y + 20}, ImGui::GetColorU32(ImGuiCol_TextDisabled), first.c_str());
            }
            dl->PopClipRect();

            if (hovered && hasDesc)
            {
                ImGui::BeginTooltip();
                ImGui::PushTextWrapPos(ImGui::GetFontSize() * 24.0f);
                ImGui::TextUnformatted(p.description.c_str());
                ImGui::PopTextWrapPos();
                ImGui::EndTooltip();
            }

            if (clicked)
            {
                m_textEditor.SetText(p.code);
                m_engine.submitSource(p.code);
                std::snprintf(m_presetNameBuf, sizeof(m_presetNameBuf), "%s", p.name.c_str());
                std::snprintf(m_presetDescBuf, sizeof(m_presetDescBuf), "%s", p.description.c_str());
                m_log.write("[sys] loaded " + p.path.filename().string());
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndChild();
    }

    ImGui::Spacing();
    actionRow(nullptr, nullptr);
    endModal();
}

void UI::drawHeader()
{
    ImGui::PushStyleColor(ImGuiCol_ChildBg, vec(kPanelAlt));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {8, 4});
    ImGui::BeginChild("Header", {-FLT_MIN, kHeaderH}, ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {6, 0});
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {14, 4});

    drawLogo(LOGO, abgr(kAccent));
    ImGui::SameLine(0, 18);

    // left side
    if (iconButton(ICON_GEAR, "Settings", {0, -FLT_MIN}))  { m_openSettings = true; }
    ImGui::SameLine();
    if (iconButton(ICON_SAVE, "Save", {0, -FLT_MIN}))      { m_openSavePreset = true; }
    ImGui::SameLine();
    if (iconButton(ICON_FOLDER, "Load", {0, -FLT_MIN}))    { m_openLoadPreset = true; }

    // right side: audio + compile, clamped so it never overlaps the left buttons
    const bool  on     = m_audio.isRunning();
    const float wA     = btnW(ICON_VOL, "Audio");
    const float wC     = btnW(ICON_PLAY, "Compile");
    const float right  = ImGui::GetWindowWidth() - ImGui::GetStyle().WindowPadding.x;
    ImGui::SameLine(std::max(ImGui::GetCursorPosX() + 18.0f, right - wA - wC - ImGui::GetStyle().ItemSpacing.x));

    if (iconButton(on ? ICON_VOL : ICON_MUTE, "Audio", {wA, -FLT_MIN}, abgr(on ? kAccent : kDim)))
    {
        if (on) { m_audio.stop(); }
        else    { m_audio.start(m_engine, m_audio.getSettings()); }
        m_logCollapsed = false;
    }
    ImGui::SameLine();

    ImGui::BeginDisabled(m_engine.isCompiling());
    if (accentButton(ICON_PLAY, "Compile", {wC, -FLT_MIN}))
    {
        m_engine.submitSource(m_textEditor.GetText());
        m_logCollapsed = false;
    }
    ImGui::EndDisabled();

    ImGui::PopStyleVar(2);
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

void UI::drawEditor()
{
    // the +1 / -1 makes borders overlap with neighbours
    const float h = std::max(kMinEditor, ImGui::GetContentRegionAvail().y - (m_logCollapsed ? kLogHeaderH : kLogH) + 1.0f);

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 10});
    ImGui::BeginChild("Editor", {-FLT_MIN, h}, ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding);
    m_textEditor.Render("TextEditor", {-FLT_MIN, -FLT_MIN}, false);
    ImGui::EndChild();
    ImGui::PopStyleVar();
}

void UI::drawLog()
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 1.0f);   // overlap editor border
    ImGui::BeginChild("Log", {-FLT_MIN, -FLT_MIN}, ImGuiChildFlags_Borders);

    // flat toggle bar
    ImGui::PushStyleColor(ImGuiCol_ChildBg, vec(kPanelAlt));
    ImGui::BeginChild("LogHeader", {-FLT_MIN, kLogHeaderH});
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {10, 0});
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));

    const float clearW = btnW(ICON_X, "Clear");
    const float logW   = std::max(1.0f, ImGui::GetContentRegionAvail().x - clearW);
    if (iconButton(m_logCollapsed ? ICON_UP : ICON_DOWN, "LOG", {logW, kLogHeaderH}, abgr(kDim), true))
    {
        m_logCollapsed = !m_logCollapsed;
    }
    ImGui::SameLine(0, 0);
    if (iconButton(ICON_X, "Clear", {clearW, kLogHeaderH}, abgr(kDim)))
    {
        m_log.clear();
    }

    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);

    if (!m_logCollapsed)
    {
        // divider under the toggle bar
        const ImVec2 p = ImGui::GetWindowPos();
        const float y = p.y + kLogHeaderH - 0.5f;
        ImGui::GetWindowDrawList()->AddLine({p.x, y}, {p.x + ImGui::GetWindowWidth(), y}, ImGui::GetColorU32(ImGuiCol_Border));
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    if (!m_logCollapsed)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {12, 8});
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {0, 3});
        ImGui::BeginChild("LogBody", {-FLT_MIN, -FLT_MIN}, ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_HorizontalScrollbar);

        const auto lines = m_log.getLines();
        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(lines.size()));
        while (clipper.Step())
        {
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, logColor(lines[i]));
                ImGui::TextUnformatted(lines[i].c_str());
                ImGui::PopStyleColor();
            }
        }
        clipper.End();

        // stick to bottom if already there
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
        {
            ImGui::SetScrollHereY(1.0f);
        }
        ImGui::EndChild();
        ImGui::PopStyleVar(2);
    }

    ImGui::EndChild();
    ImGui::PopStyleVar();
}