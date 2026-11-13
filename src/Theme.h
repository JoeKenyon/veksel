#pragma once

#include "theme.h"
#include <imgui.h>

namespace Theme
{

    namespace Color
    {
        using Color = const ImVec4;

        // Greys
        inline Color black       {0.025f, 0.025f, 0.035f, 1.0f};
        inline Color grey_dark   {0.055f, 0.055f, 0.070f, 1.0f};
        inline Color grey_darker {0.035f, 0.035f, 0.045f, 1.0f};
        inline Color grey        {0.120f, 0.120f, 0.145f, 1.0f};
        inline Color grey_light  {0.550f, 0.550f, 0.600f, 1.0f};
        inline Color white       {0.850f, 0.850f, 0.890f, 1.0f};
        inline Color white_hot   {1.000f, 1.000f, 1.000f, 1.0f};

        // Pink palette
        inline Color pink {0.929f, 0.055f, 0.447f, 1.0f};
        inline Color pink_light  {1.000f, 0.300f, 0.600f, 1.0f};
        inline Color pink_hot    {1.000f, 0.100f, 0.500f, 1.0f};
        inline Color pink_neon   {1.000f, 0.000f, 0.800f, 1.0f};
        inline Color pink_dark   {0.650f, 0.025f, 0.280f, 1.0f};
        inline Color pink_darker {0.350f, 0.015f, 0.150f, 1.0f};
        inline Color pink_deep   {0.160f, 0.010f, 0.070f, 1.0f};
        inline Color pink_muted  {0.400f, 0.100f, 0.230f, 1.0f};

        inline Color green  {0.200f, 0.850f, 0.400f, 1.0f};
        inline Color yellow {1.000f, 0.800f, 0.200f, 1.0f};

        // Main
        inline Color Text = white;

        inline Color    Accent = pink;
        inline Color AccentLow = grey_darker;
        inline Color AccentMed = grey_dark;
        inline Color AccentNeg = grey_darker;

        // Main window
        inline Color MainWindowBg     = Accent;
        inline Color MainWindowBorder = black;

        // Panels
        inline Color PanelBg     = AccentNeg;
        inline Color PanelBorder = grey;

        // Buttons - normal
        inline Color     ButtonBg = Accent;
        inline Color ButtonBorder = Accent;
        inline Color   ButtonText = PanelBg;

        // Buttons - hover
        inline Color     ButtonBgHover = PanelBg;
        inline Color ButtonBorderHover = PanelBg;
        inline Color   ButtonTextHover = Accent;

        // Buttons - active
        inline Color     ButtonBgActive = AccentMed;
        inline Color ButtonBorderActive = pink_light;
        inline Color   ButtonTextActive = white_hot;

        // Combo - closed state
        inline Color   ComboText = Text;
        inline Color ComboIconBg = grey;
        inline Color ComboBorder = grey_light;
        inline Color ComboTextBg = grey_dark;

        // Combo - hover state
        inline Color   ComboTextHover = white_hot;
        inline Color ComboIconBgHover = Accent;
        inline Color ComboBorderHover = Accent;
        inline Color ComboTextBgHover = grey;

        // Combo - dropdown options
        inline Color     ComboOptionText = Text;
        inline Color       ComboOptionBg = pink_darker;
        inline Color  ComboOptionBgHover = pink_dark;
        inline Color ComboOptionBgActive = Accent;

        // Combo - dropdown popup
        inline Color     ComboPopupText = Text;
        inline Color   ComboPopupBorder = Accent;
        inline Color       ComboPopupBg = grey_dark;
        inline Color  ComboPopupBgHover = grey;
        inline Color ComboPopupBgActive = pink_darker;

        // Settings popup
        inline Color SettingsBg     = grey_dark;
        inline Color SettingsBorder = Accent;
    }



    namespace Size
    {
        // Fonts
        inline constexpr float LogoFontSize = 9.0f;
        inline constexpr float TextFontSize = 17.0f;
        inline constexpr float IconFontSize = 18.0f;

        // Main window
        inline constexpr ImVec2 MainWindowPadding = {5.0f, 5.0f};
        inline constexpr float MainWindowBorder = 1.0f;

        // Panels
        inline constexpr ImVec2 PanelPadding = {10.0f, 10.0f};
        inline constexpr float PanelBorder = 1.0f;
        inline constexpr ImVec2 PanelSpacing = {5.0f, 5.0f};

        // Header and layout
        inline constexpr float HeaderHeight = 55.0f;
        inline constexpr float LogHeight = 184.0f;

        // editor
        inline constexpr ImVec2 EditorPadding = {0.0f,0.f};

        // Buttons
        inline constexpr ImVec2 ButtonPadding = {10.0f, 6.0f};
        inline constexpr float ButtonBorder = 1.0f;

        // Combo box
        inline constexpr ImVec2 ComboBoxPadding = {10.0f, 7.0f};
        inline constexpr float ComboBoxBorder = 1.0f;

        // Combo dropdown
        inline constexpr ImVec2 ComboPopupPadding = {5.0f, 5.0f};
        inline constexpr ImVec2 ComboPopupSpacing = {2.0f, 3.0f};
        inline constexpr ImVec2 ComboOptionPadding = {8.0f, 5.0f};

        // Settings popup
        inline constexpr ImVec2 SettingsPadding = {12.0f, 12.0f};
        inline constexpr float SettingsBorder = 1.0f;

        // Rounding
        inline constexpr float Rounding = 12.0f;
        inline constexpr float MainWindowRounding = Rounding;
        inline constexpr float PanelRounding = Rounding;
        inline constexpr float ButtonRounding = Rounding;
        inline constexpr float ComboBoxRounding = Rounding;
        inline constexpr float ComboPopupRounding = Rounding;
        inline constexpr float SettingsRounding = Rounding;
    }

}
