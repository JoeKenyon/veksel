#pragma once

#include <imgui.h>

namespace Theme
{
    // Greys
    inline const ImVec4 black       {0.000f, 0.000f, 0.000f, 1.0f};
    inline const ImVec4 grey_dark   {0.100f, 0.100f, 0.100f, 1.0f};
    inline const ImVec4 grey_darker {0.080f, 0.080f, 0.080f, 1.0f};
    inline const ImVec4 grey        {0.200f, 0.200f, 0.200f, 1.0f};
    inline const ImVec4 grey_light  {0.650f, 0.650f, 0.650f, 1.0f};

    // Whites
    inline const ImVec4 white       {0.900f, 0.900f, 0.900f, 1.0f};
    inline const ImVec4 white_hot   {1.000f, 1.000f, 1.000f, 1.0f};

    // Hot pink palette
    inline const ImVec4 pink        {1.000f, 0.078f, 0.576f, 1.0f};
    inline const ImVec4 pink_light  {1.000f, 0.412f, 0.706f, 1.0f};
    inline const ImVec4 pink_hot    {1.000f, 0.000f, 0.498f, 1.0f};
    inline const ImVec4 pink_neon   {1.000f, 0.000f, 1.000f, 1.0f};
    inline const ImVec4 pink_dark   {0.788f, 0.000f, 0.408f, 1.0f};
    inline const ImVec4 pink_deep   {0.400f, 0.000f, 0.200f, 1.0f};
    inline const ImVec4 pink_muted  {0.600f, 0.180f, 0.380f, 1.0f};

    // Accent colours
    inline const ImVec4 accent      = pink_dark;
    inline const ImVec4 accent_neg  = black;
    inline const ImVec4 accent_text = white;

    inline const ImVec4 accent_low  {accent.x, accent.y, accent.z, 0.2f};
    inline const ImVec4 accent_med  {accent.x, accent.y, accent.z, 0.4f};
    inline const ImVec4 accent_high {accent.x, accent.y, accent.z, 0.8f};

    inline const ImVec4 accent_neg_low  {accent_neg.x, accent_neg.y, accent_neg.z, 0.5f};
    inline const ImVec4 accent_neg_med  {accent_neg.x, accent_neg.y, accent_neg.z, 0.7f};
    inline const ImVec4 accent_neg_high {accent_neg.x, accent_neg.y, accent_neg.z, 0.9f};
}

inline void apply_theme()
{
    ImGuiStyle& s = ImGui::GetStyle();

    // Rounding
    s.WindowRounding = 0.0f;
    s.ChildRounding  = 0.0f;
    s.PopupRounding  = 0.0f;
    s.FrameRounding  = 0.0f;
    s.ScrollbarRounding = 0.0f;
    s.GrabRounding      = 0.0f;

    // Borders
    s.WindowBorderSize = 0.0f;
    s.ChildBorderSize  = 0.0f;
    s.FrameBorderSize  = 1.0f;
    s.PopupBorderSize  = 1.0f;

    // Spacing
    s.WindowPadding = ImVec2(10.0f, 10.0f);
    s.FramePadding  = ImVec2(10.0f, 10.0f);
    s.ItemSpacing   = ImVec2(0.0f, 0.0f);

    ImVec4* c = s.Colors;

    c[ImGuiCol_WindowBg]            = Theme::black;
    c[ImGuiCol_ChildBg]             = Theme::black;
    c[ImGuiCol_PopupBg]             = Theme::accent_neg;
    c[ImGuiCol_Border]              = Theme::accent;
    c[ImGuiCol_BorderShadow]        = ImVec4(0, 0, 0, 0);

    c[ImGuiCol_Text]                = Theme::accent_text;
    c[ImGuiCol_TextDisabled]        = Theme::grey_light;

    c[ImGuiCol_FrameBg]             = Theme::accent_neg;
    c[ImGuiCol_FrameBgHovered]      = Theme::accent;
    c[ImGuiCol_FrameBgActive]       = Theme::accent;

    c[ImGuiCol_Button]              = Theme::accent;
    c[ImGuiCol_ButtonHovered]       = Theme::accent_neg;
    c[ImGuiCol_ButtonActive]        = Theme::accent_neg;

    c[ImGuiCol_Header]              = Theme::accent_med;
    c[ImGuiCol_HeaderHovered]       = Theme::accent_low;
    c[ImGuiCol_HeaderActive]        = Theme::accent_high;

    c[ImGuiCol_Separator]            = Theme::accent;
    c[ImGuiCol_ScrollbarBg]          = Theme::accent_low;
    c[ImGuiCol_ScrollbarGrab]        = Theme::accent_neg_low;
    c[ImGuiCol_ScrollbarGrabHovered] = Theme::accent_neg_med;
    c[ImGuiCol_ScrollbarGrabActive]  = Theme::accent_neg;
}
