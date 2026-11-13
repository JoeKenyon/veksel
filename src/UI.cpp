#include "UI.h"
#include "Theme.h"
#include "imgui.h"

static bool custom_button(const char* label, ImVec2 size = ImVec2(0, 0))
{
    // check if hovering
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const bool isHover = ImGui::IsWindowHovered() &&
        ImGui::IsMouseHoveringRect(p, ImVec2(p.x + size.x, p.y + size.y));

    // check if active
    const bool isActive = isHover && ImGui::IsMouseDown(ImGuiMouseButton_Left);

    // idle look first, then override if hovered or pressed
    ImVec4 button_text   = Theme::Color::ButtonText;
    ImVec4 button_bg     = Theme::Color::ButtonBg;
    ImVec4 button_border = Theme::Color::ButtonBorder;

    if (isActive)
    {
        button_text   = Theme::Color::ButtonTextActive;
        button_bg     = Theme::Color::ButtonBgActive;
        button_border = Theme::Color::ButtonBorderActive;
    }
    else if (isHover)
    {
        button_text   = Theme::Color::ButtonTextHover;
        button_bg     = Theme::Color::ButtonBgHover;
        button_border = Theme::Color::ButtonBorderHover;
    }

    ImGui::PushStyleColor(ImGuiCol_Button,        button_bg);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, button_bg);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  button_bg);
    ImGui::PushStyleColor(ImGuiCol_Text,          button_text);
    ImGui::PushStyleColor(ImGuiCol_Border,        button_border);
    const bool pressed = ImGui::Button(label, size);
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
    const float combo_height =
        ImGui::GetFontSize() +
        Theme::Size::ComboBoxPadding.y * 2.0f +
        Theme::Size::ComboBoxBorder * 2.0f;

    const ImVec2 p = ImGui::GetCursorScreenPos();

    const bool isHover =
        ImGui::IsWindowHovered() &&
        ImGui::IsMouseHoveringRect(
            p,
            ImVec2(p.x + width, p.y + combo_height)
        );

    const ImVec4 combo_text    = isHover ? Theme::Color::ComboTextHover    : Theme::Color::ComboText;
    const ImVec4 combo_icon_bg = isHover ? Theme::Color::ComboIconBgHover  : Theme::Color::ComboIconBg;
    const ImVec4 combo_border  = isHover ? Theme::Color::ComboBorderHover  : Theme::Color::ComboBorder;
    const ImVec4 combo_text_bg = isHover ? Theme::Color::ComboTextBgHover  : Theme::Color::ComboTextBg;

    ImGui::SetNextItemWidth(width);

    ImGui::PushStyleColor(ImGuiCol_Text, combo_text);
    ImGui::PushStyleColor(ImGuiCol_Border, combo_border);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, combo_text_bg);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, combo_text_bg);
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, combo_text_bg);
    ImGui::PushStyleColor(ImGuiCol_Button, combo_icon_bg);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, combo_icon_bg);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, combo_icon_bg);

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



UI::UI(veksel::Engine& engine)
    : engine(engine)
{
    ImGuiIO& io = ImGui::GetIO();

    static const ImWchar ranges[] =
    {
        0x20, 0x7F,
        0x2500, 0x259F,
        0
    };

    mainFont = io.Fonts->AddFontFromFileTTF(
        "/System/Library/Fonts/Menlo.ttc", Theme::Size::TextFontSize, nullptr, ranges);

    logoFont = io.Fonts->AddFontFromFileTTF(
        "/System/Library/Fonts/Menlo.ttc", Theme::Size::LogoFontSize, nullptr, ranges);

    iconFont = io.Fonts->AddFontFromFileTTF(
        "/System/Library/Fonts/Menlo.ttc", Theme::Size::IconFontSize, nullptr, ranges);

    io.FontDefault = mainFont;

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

    auto u32 = [](ImVec4 c, float alpha = -1.0f)
    {
        if (alpha >= 0.0f) c.w = alpha;
        return ImGui::ColorConvertFloat4ToU32(c);
    };

    const ImU32 green = IM_COL32( 61, 220, 132, 255);
    const ImU32 amber = IM_COL32(230, 180,  80, 255);
    const ImU32 blue  = IM_COL32( 97, 175, 239, 255);

    auto set = [&](TextEditor::Color c, ImU32 v) { p[static_cast<int>(c)] = v; };

    text_editor.SetPalette(p);

    // starter code, passthrough
    text_editor.SetText(
        "veksel_main\n"
        "{\n"
        "    return input;\n"
        "}\n"
    );

    engine.submit_source(text_editor.GetText());

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
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, Theme::Size::PanelSpacing);


    draw_header();
    draw_editor();
    draw_log();

    ImGui::End();

    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

static void draw_settings_popup(ImVec2 anchor)
{
    static int output_device = 0, input_device = 0, sample_rate = 1, buffer_size = 2;

    const float w  = 320.0f;
    const float cw = w - Theme::Size::SettingsPadding.x * 2.0f;

    ImGui::SetNextWindowPos({anchor.x - w, anchor.y + 2.0f}, ImGuiCond_Appearing);
    ImGui::SetNextWindowSize(ImVec2(w, 0.0f));

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, Theme::Size::SettingsPadding);
    ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, Theme::Size::SettingsBorder);
    ImGui::PushStyleColor(ImGuiCol_PopupBg, Theme::Color::SettingsBg);
    ImGui::PushStyleColor(ImGuiCol_Border, Theme::Color::SettingsBorder);
    const bool open = ImGui::BeginPopup("##settings");
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);

    if (!open)
        return;

    auto field = [&](const char* label, const char* id, int* v, const char* items)
    {
        ImGui::TextColored(Theme::Color::Text, "%s", label);
        ImGui::SetNextItemWidth(cw);
        custom_combo(id, v, items, cw);
    };

    field("Output Device", "##output_device", &output_device, "Default Output\0Device 1\0Device 2\0");
    field("Input Device",  "##input_device",  &input_device,  "Default Input\0Device 1\0Device 2\0");
    field("Sample Rate",   "##sample_rate",   &sample_rate,   "44100 Hz\0" "48000 Hz\0" "88200 Hz\0" "96000 Hz\0");
    field("Buffer Size",   "##buffer_size",   &buffer_size,   "64\0" "128\0" "256\0" "512\0" "1024\0");

    ImGui::EndPopup();
}

void UI::draw_header()
{
    ImGui::PushStyleColor(ImGuiCol_ChildBg, Theme::Color::PanelBg);
    ImGui::PushStyleColor(ImGuiCol_Border, Theme::Color::PanelBorder);

    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, Theme::Size::PanelBorder);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, Theme::Size::PanelPadding);

    if(logoFont) ImGui::PushFont(logoFont);
    const ImVec2 logo_size = ImGui::CalcTextSize(logo);
    const float header_height = logo_size.y + Theme::Size::PanelPadding.y * 2.0f;
    const float button_size = std::min(header_height * 0.5f, header_height - Theme::Size::PanelPadding.y * 2);

    ImGui::BeginChild("##header",
        ImVec2(0.0f, header_height),
        ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding);

    const float both_btns_w = button_size * 2.0f + Theme::Size::PanelPadding.x;
    const float both_btns_x = ImGui::GetWindowWidth() - both_btns_w - Theme::Size::PanelPadding.x;
    const float both_btns_y = (ImGui::GetWindowHeight() - button_size) * 0.5f;

    ImGui::PushStyleColor(ImGuiCol_Text, Theme::Color::Text);
    // Header contents
    ImGui::TextUnformatted(logo);
    if(logoFont) ImGui::PopFont();

    ImGui::PopStyleColor();

    // padding as spacing between buttons
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, Theme::Size::PanelPadding);
    ImGui::SetCursorPos(ImVec2(both_btns_x, both_btns_y));
    if(iconFont) ImGui::PushFont(iconFont);

    // compile button
    if (custom_button("▶", {button_size,button_size}))
    {
        engine.submit_source(text_editor.GetText());
    };

    ImGui::SameLine();

    // settings button
    if (custom_button("⚙", ImVec2(button_size, button_size)))
        ImGui::OpenPopup("##settings");

    if (iconFont) ImGui::PopFont();

    draw_settings_popup(ImGui::GetItemRectMax());

    ImGui::PopStyleVar();   // item spacing

    ImGui::EndChild();

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}

void UI::draw_editor()
{
    ImGui::PushStyleColor(ImGuiCol_ChildBg, Theme::Color::PanelBg);
    ImGui::PushStyleColor(ImGuiCol_Border, Theme::Color::PanelBorder);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, Theme::Size::PanelBorder);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, Theme::Size::PanelPadding);
    ImGui::BeginChild( "##editor", {-1,ImGui::GetContentRegionAvail().y * 0.75f},ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, Theme::Size::EditorPadding);
    text_editor.Render("##source", ImVec2(-1, -1), true);
    ImGui::PopStyleVar();
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}

void UI::draw_log()
{
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {Theme::Size::TextFontSize * 0.5f, Theme::Size::TextFontSize * 0.5f});
    ImGui::PushStyleColor(ImGuiCol_ChildBg, Theme::Color::PanelBg);
    ImGui::PushStyleColor(ImGuiCol_Border, Theme::Color::PanelBorder);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, Theme::Size::PanelBorder);
    ImGui::PushStyleColor(ImGuiCol_Text, Theme::Color::Text);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, Theme::Size::PanelPadding);
    ImGui::BeginChild( "##log", {-1,-1}, ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding);

    for (const auto& line : engine.get_log())
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


    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) ImGui::SetScrollHereY(1.0f);

    ImGui::EndChild();

    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(3);
}
