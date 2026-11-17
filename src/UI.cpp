#include "UI.h"
#include "Theme.h"
#include "imgui.h"

#include <algorithm>
#include <cstring>

using namespace veksel;

static bool custom_button(const char* label, ImVec2 size = ImVec2(0, 0))
{
    ImVec2 resolvedSize = size;
    if (resolvedSize.y <= 0.0f)
        resolvedSize.y = ImGui::GetFrameHeight();
    if (resolvedSize.x <= 0.0f)
        resolvedSize.x = ImGui::CalcTextSize(label).x + ImGui::GetStyle().FramePadding.x * 2.0f;

    // check if hovering
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const bool isHover = ImGui::IsWindowHovered() &&
        ImGui::IsMouseHoveringRect(p, ImVec2(p.x + resolvedSize.x, p.y + resolvedSize.y));

    // check if active
    const bool isActive = isHover && ImGui::IsMouseDown(ImGuiMouseButton_Left);

    // idle look first, then override if hovered or pressed
    ImVec4 buttonText   = Theme::Color::ButtonText;
    ImVec4 buttonBg     = Theme::Color::ButtonBg;
    ImVec4 buttonBorder = Theme::Color::ButtonBorder;

    if (isActive)
    {
        buttonText   = Theme::Color::ButtonTextActive;
        buttonBg     = Theme::Color::ButtonBgActive;
        buttonBorder = Theme::Color::ButtonBorderActive;
    }
    else if (isHover)
    {
        buttonText   = Theme::Color::ButtonTextHover;
        buttonBg     = Theme::Color::ButtonBgHover;
        buttonBorder = Theme::Color::ButtonBorderHover;
    }

    ImGui::PushStyleColor(ImGuiCol_Button,        buttonBg);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, buttonBg);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  buttonBg);
    ImGui::PushStyleColor(ImGuiCol_Text,          buttonText);
    ImGui::PushStyleColor(ImGuiCol_Border,        buttonBorder);
    const bool pressed = ImGui::Button(label, resolvedSize);
    ImGui::PopStyleColor(5);

    return pressed;
}


static bool custom_combo(const char* label, int* current, const char* items, float width = 300.0f)
{
    // Find selected item
    const char* preview = "";
    int count = 0;

    for (const char* it = items; *it; it += std::strlen(it) + 1, ++count)
    {
        if (count == *current)
        {
            preview = it;
            break;
        }
    }

    // Calculate actual frame height, including custom padding and border
    const float comboHeight =
        ImGui::GetFontSize() +
        Theme::Size::ComboBoxPadding.y * 2.0f +
        Theme::Size::ComboBoxBorder * 2.0f;

    const ImVec2 p = ImGui::GetCursorScreenPos();

    const bool isHover =
        ImGui::IsWindowHovered() &&
        ImGui::IsMouseHoveringRect(
            p,
            ImVec2(p.x + width, p.y + comboHeight)
        );

    const ImVec4 comboText   = isHover ? Theme::Color::ComboTextHover    : Theme::Color::ComboText;
    const ImVec4 comboIconBg = isHover ? Theme::Color::ComboIconBgHover  : Theme::Color::ComboIconBg;
    const ImVec4 comboBorder = isHover ? Theme::Color::ComboBorderHover  : Theme::Color::ComboBorder;
    const ImVec4 comboTextBg = isHover ? Theme::Color::ComboTextBgHover  : Theme::Color::ComboTextBg;

    ImGui::SetNextItemWidth(width);

    ImGui::PushStyleColor(ImGuiCol_Text, comboText);
    ImGui::PushStyleColor(ImGuiCol_Border, comboBorder);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, comboTextBg);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, comboTextBg);
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, comboTextBg);
    ImGui::PushStyleColor(ImGuiCol_Button, comboIconBg);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, comboIconBg);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, comboIconBg);

    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, Theme::Size::ComboBoxBorder);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, Theme::Size::ComboBoxPadding);

    const bool open = ImGui::BeginCombo(label, preview);

    ImGui::PopStyleColor(8);
    ImGui::PopStyleVar(2);

    bool changed = false;

    if (open)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, Theme::Color::ComboOptionText);
        ImGui::PushStyleColor(ImGuiCol_Header, Theme::Color::ComboOptionBg);
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, Theme::Color::ComboOptionBgHover);
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, Theme::Color::ComboOptionBgActive);
        ImGui::PushStyleColor(ImGuiCol_PopupBg, Theme::Color::ComboPopupBg);
        ImGui::PushStyleColor(ImGuiCol_Border, Theme::Color::ComboPopupBorder);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, Theme::Size::ComboPopupPadding);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, Theme::Size::ComboPopupSpacing);

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

            if (selected)
                ImGui::SetItemDefaultFocus();

            ImGui::PopID();
        }

        ImGui::PopStyleColor(6);
        ImGui::PopStyleVar(2);
        ImGui::EndCombo();
    }

    return changed;
}



UI::UI(Engine& engine, Audio& audio)
    : m_engine(engine), m_audio(audio)
{
    ImGuiIO& io = ImGui::GetIO();

    static const ImWchar ranges[] =
    {
        0x20, 0x7F,
        0x2500, 0x259F,
        0
    };

    m_mainFont = io.Fonts->AddFontFromFileTTF(
        "/System/Library/Fonts/Menlo.ttc", Theme::Size::TextFontSize, nullptr, ranges);

    m_logoFont = io.Fonts->AddFontFromFileTTF(
        "/System/Library/Fonts/Menlo.ttc", Theme::Size::LogoFontSize, nullptr, ranges);

    m_iconFont = io.Fonts->AddFontFromFileTTF(
        "/System/Library/Fonts/Menlo.ttc", Theme::Size::IconFontSize, nullptr, ranges);

    io.FontDefault = m_mainFont;

    // editor highlights as C since the modules are c
    m_textEditor.SetLanguage(TextEditor::LookupLanguage("C"));

    // language is const so cast it away to add our own identifiers
    auto cLang = const_cast<TextEditor::Language*>(m_textEditor.GetLanguage());

    // so veksel_main and input get highlighted like known names
    if (cLang)
    {
        cLang->identifiers.insert("veksel_main");
        cLang->identifiers.insert("input");
    }

    // dark palette but pure black bg, white text, grey comments
    TextEditor::Palette p = TextEditor::GetDarkPalette();

    m_textEditor.SetPalette(p);

    // starter code, passthrough
    m_textEditor.SetText(
        "veksel_main\n"
        "{\n"
        "    return input;\n"
        "}\n"
    );

    engine.submitSource(m_textEditor.GetText());

    ImGuiStyle& style = ImGui::GetStyle();

    style.WindowRounding = Theme::Size::MainWindowRounding;
    style.ChildRounding = Theme::Size::PanelRounding;
    style.FrameRounding = Theme::Size::ButtonRounding;
    style.PopupRounding = Theme::Size::ComboPopupRounding;
}



void UI::render()
{
    ImGuiViewport* viewport = ImGui::GetMainViewport();

    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);

    ImGui::PushStyleColor(ImGuiCol_WindowBg, Theme::Color::MainWindowBg);
    ImGui::PushStyleColor(ImGuiCol_Border, Theme::Color::MainWindowBorder);
    ImGui::PushStyleColor(ImGuiCol_Text, Theme::Color::Text);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, Theme::Size::MainWindowBorder);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, Theme::Size::MainWindowPadding);
    ImGui::Begin(
        "VekselMain",
        nullptr,
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoBringToFrontOnFocus
    );
    // pop all three colors pushed above now that Begin() has consumed them.
    // keeping the color stack balanced at this point avoids the fragile
    // push-here/pop-at-end pattern.
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(3);

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, Theme::Size::PanelSpacing);

    drawHeader();
    drawEditor();
    drawLog();

    ImGui::End();

    ImGui::PopStyleVar();
}

void UI::drawSettingsPopup(ImVec2 anchor)
{
    static int outputDevice = 0, inputDevice = 0, sampleRate = 1, bufferSize = 2;

    static AudioDevices devices;

    // width of each item
    const float width  = 320.0f;
    const float comboWidth = width - Theme::Size::SettingsPadding.x * 2.0f;

    // set position and styles etc.
    ImGui::SetNextWindowPos({anchor.x - width, anchor.y + 2.0f}, ImGuiCond_Appearing);
    ImGui::SetNextWindowSize(ImVec2(width, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, Theme::Size::SettingsPadding);
    ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, Theme::Size::SettingsBorder);
    ImGui::PushStyleColor(ImGuiCol_PopupBg, Theme::Color::SettingsBg);
    ImGui::PushStyleColor(ImGuiCol_Border, Theme::Color::SettingsBorder);
    const bool open = ImGui::BeginPopup("##settings");
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);

    // if we are not open dont draw anything
    if (!open)
        return;

    // IsWindowAppearing() must be checked after BeginPopup, otherwise it refers
    // to the parent window and the device list would never refresh.
    static bool hasChanges = false;
    if (ImGui::IsWindowAppearing())
    {
        devices = m_audio.getDevices();
        hasChanges = false;

        // clamp the persisted selection indices to the available lists
        auto clamp = [](int& v, size_t n) { if (n == 0) v = 0; else if (v >= (int)n) v = (int)n - 1; };
        clamp(inputDevice,  devices.inputIndices.size());
        clamp(outputDevice, devices.outputIndices.size());
        clamp(sampleRate,   devices.sampleIndices.size());
        clamp(bufferSize,   devices.bufferIndices.size());
    }

    // lambda for drawing each combo and title
    auto field = [&](const char* label, const char* id, int* v, const char* items)
    {
        ImGui::TextColored(Theme::Color::Text, "%s", label);
        ImGui::SetNextItemWidth(comboWidth);
        // use the custom comboBox
        return custom_combo(id, v, items, comboWidth);
    };

    const char* inItems  = devices.inputNames.c_str();
    const char* outItems = devices.outputNames.c_str();
    const char* srItems  = devices.sampleRates.c_str();
    const char* bufItems = devices.bufferSizes.c_str();

    bool changedIn  = field("Input Device",  "##input_device",  &inputDevice,  inItems);
    bool changedOut = field("Output Device", "##output_device", &outputDevice, outItems);
    bool changedSr  = field("Sample Rate",   "##sample_rate",   &sampleRate,   srItems);
    bool changedBuf = field("Buffer Size",   "##buffer_size",   &bufferSize,   bufItems);

    if (changedIn || changedOut || changedSr || changedBuf) hasChanges = true;

    ImGui::Spacing();

    // If there are no changes, dim the button or disable interactions
    if (!hasChanges)
    {
        ImGui::BeginDisabled();
    }

    if (custom_button("Apply", ImVec2(comboWidth, 0.0f)))
    {
        int selectedInIdx  = (!devices.inputIndices.empty() && inputDevice < (int)devices.inputIndices.size()) ? devices.inputIndices[inputDevice] : -1;
        int selectedOutIdx = (!devices.outputIndices.empty() && outputDevice < (int)devices.outputIndices.size()) ? devices.outputIndices[outputDevice] : -1;

        int finalSr  = sampleRate   < (int)devices.sampleIndices.size() ? devices.sampleIndices[sampleRate]   : -1;
        int finalBuf = bufferSize   < (int)devices.bufferIndices.size() ? devices.bufferIndices[bufferSize]   : -1;

        if (finalSr < 0 || finalBuf < 0)
        {
            m_engine.writeLog("[error] Invalid sample rate or buffer size selection.");
        }
        else if (!m_audio.start(m_engine, selectedInIdx, selectedOutIdx, finalSr, finalBuf))
        {
            m_engine.writeLog("[error] Failed to restart audio with the selected devices.");
        }
        else
        {
            ImGui::CloseCurrentPopup();
        }
    }

    if (!hasChanges)
    {
        ImGui::EndDisabled();
    }

    ImGui::EndPopup();
}

void UI::drawHeader()
{
    ImGui::PushStyleColor(ImGuiCol_ChildBg, Theme::Color::PanelBg);
    ImGui::PushStyleColor(ImGuiCol_Border, Theme::Color::PanelBorder);

    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, Theme::Size::PanelBorder);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, Theme::Size::PanelPadding);

    if(m_logoFont) ImGui::PushFont(m_logoFont);
    const ImVec2 logoSize = ImGui::CalcTextSize(m_logo);
    const float headerHeight = logoSize.y + Theme::Size::PanelPadding.y * 2.0f;
    const float buttonSize = std::min(headerHeight * 0.5f, headerHeight - Theme::Size::PanelPadding.y * 2);

    ImGui::BeginChild("##header",
        ImVec2(0.0f, headerHeight),
        ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding);

    const float bothBtnsW = buttonSize * 2.0f + Theme::Size::PanelPadding.x;
    const float bothBtnsX = ImGui::GetWindowWidth() - bothBtnsW - Theme::Size::PanelPadding.x;
    const float bothBtnsY = (ImGui::GetWindowHeight() - buttonSize) * 0.5f;

    ImGui::PushStyleColor(ImGuiCol_Text, Theme::Color::Text);
    // Header contents
    ImGui::TextUnformatted(m_logo);
    if(m_logoFont) ImGui::PopFont();

    ImGui::PopStyleColor();

    // padding as spacing between buttons
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, Theme::Size::PanelPadding);
    ImGui::SetCursorPos(ImVec2(bothBtnsX, bothBtnsY));
    if(m_iconFont) ImGui::PushFont(m_iconFont);

    // compile button
    if (custom_button("▶", {buttonSize,buttonSize}))
    {
        m_engine.submitSource(m_textEditor.GetText());
    };

    ImGui::SameLine();

    // settings button
    if (custom_button("⚙", ImVec2(buttonSize, buttonSize)))
        ImGui::OpenPopup("##settings");

    if (m_iconFont) ImGui::PopFont();

    drawSettingsPopup(ImGui::GetItemRectMax());

    ImGui::PopStyleVar();   // item spacing

    ImGui::EndChild();

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}

void UI::drawEditor()
{
    ImGui::PushStyleColor(ImGuiCol_ChildBg, Theme::Color::PanelBg);
    ImGui::PushStyleColor(ImGuiCol_Border, Theme::Color::PanelBorder);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, Theme::Size::PanelBorder);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, Theme::Size::PanelPadding);
    ImGui::BeginChild( "##editor", {-1,ImGui::GetContentRegionAvail().y * 0.75f},ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, Theme::Size::EditorPadding);
    m_textEditor.Render("##source", ImVec2(-1, -1), true);
    ImGui::PopStyleVar();
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}

void UI::drawLog()
{
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {Theme::Size::TextFontSize * 0.5f, Theme::Size::TextFontSize * 0.5f});
    ImGui::PushStyleColor(ImGuiCol_ChildBg, Theme::Color::PanelBg);
    ImGui::PushStyleColor(ImGuiCol_Border, Theme::Color::PanelBorder);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, Theme::Size::PanelBorder);
    ImGui::PushStyleColor(ImGuiCol_Text, Theme::Color::Text);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, Theme::Size::PanelPadding);

    if (ImGui::BeginChild("##log", {-1, -1}, ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding))
    {
        const auto log = m_engine.getLog();

        // if new lines arrived since last frame, snap to the bottom so the
        // user sees them even if they had scrolled up.
        static size_t lastCount = 0;
        const bool hasNew = !log.empty() && log.size() != lastCount;
        lastCount = log.size();

        for (const auto& line : log)
        {
            if (line.find("[error]") != std::string::npos)
                ImGui::TextColored(Theme::Color::pink, "%s", line.c_str());
            else if (line.find("[ok]") != std::string::npos)
                ImGui::TextColored(Theme::Color::green, "%s", line.c_str());
            else if (line.find("[warn]") != std::string::npos)
                ImGui::TextColored(Theme::Color::yellow, "%s", line.c_str());
            else if (line.find("[sys]") != std::string::npos)
                ImGui::TextColored(Theme::Color::grey_light, "%s", line.c_str());
            else
                ImGui::TextUnformatted(line.c_str());
        }

        if (hasNew || ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f)
            ImGui::SetScrollHereY(1.0f);
    }

    ImGui::EndChild();

    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(3);
}
