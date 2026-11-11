#pragma once

#include "imgui.h"

namespace Theme
{
    inline const ImVec4 hot_pink = ImVec4(1.0f, 0.412f, 0.706f, 1.0f);
    inline const ImVec4 hotter_pink = ImVec4(1.0f, 0.078f, 0.576f, 1.0f);

    // Main colours
    inline const ImVec4 accent        = hotter_pink;
    inline const ImVec4 accent_neg    = { 0.f, 0.f, 0.f, 1.f };
    inline const ImVec4 accent_low    = { accent.x, accent.y, accent.z, 0.1f };
    inline const ImVec4 accent_med    = { accent.x, accent.y, accent.z, 0.5f };
    inline const ImVec4 accent_high   = { accent.x, accent.y, accent.z, 0.8f };

    // Borders
    inline const ImVec4 border           = accent;
    inline constexpr float border_radius = 10.f;

    // Interaction colours
    inline const ImVec4 item        = accent_med;
    inline const ImVec4 item_hover  = accent_low;
    inline const ImVec4 item_active = accent_high;
}

struct ImFont;
extern ImFont* g_small_font;

bool platform_init(const char *title, int w, int h);
bool platform_begin_frame();   // false when the window should close; opens a fullscreen "root" window
void platform_end_frame();
void platform_shutdown();
