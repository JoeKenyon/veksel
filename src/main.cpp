#include <cfloat>
#include "imgui.h"
#include "platform.h"
#include <cmath>

static char code[8192] =
    "// write your code here\n"
    "int add(int a, int b) {\n"
    "    return a + b;\n"
    "}\n";

static const char *banner =
    "██╗   ██╗███████╗██╗  ██╗███████╗███████╗██╗     \n"
    "██║   ██║██╔════╝██║ ██╔╝██╔════╝██╔════╝██║     \n"
    "██║   ██║█████╗  █████╔╝ ███████╗█████╗  ██║     \n"
    "╚██╗ ██╔╝██╔══╝  ██╔═██╗ ╚════██║██╔══╝  ██║     \n"
    " ╚████╔╝ ███████╗██║  ██╗███████║███████╗███████╗\n"
    "  ╚═══╝  ╚══════╝╚═╝  ╚═╝╚══════╝╚══════╝╚══════╝";

static float gain   = 0.5f;
static float param1 = 0.5f;
static float param2 = 0.5f;
static float param3 = 0.5f;
static float param4 = 0.5f;
static float param5 = 0.5f;

static void draw_ui()
{
    ImGui::Columns(2, "main", true);

    ImGui::Text("%s", banner);

    // number of frequency bins to show on screen
    const int num_bins = 64;
    float fft_bins[num_bins];

    float samples[100];
    for (int n = 0; n < 100; n++)
    {
        samples[n] = std::sinf(n * 0.2f + ImGui::GetTime() * 1.5f);
    }

    // make up some fake frequency magnitudes that wiggle around
    for (int n = 0; n < num_bins; n++)
    {
        float freq = (float)n / (float)num_bins;
        float val = std::sinf(freq * 15.0f + ImGui::GetTime() * 3.0f) * 0.5f + 0.5f;
        fft_bins[n] = val * (1.0f - freq); // roll off highs a bit
    }

    ImGui::SeparatorText("Visualizer");

    ImGui::PlotLines("##wave", samples, 100, 0, nullptr, -1.0f, 1.0f, ImVec2(-FLT_MIN, 80));
    ImGui::PlotLines("##fft", fft_bins, num_bins, 0, nullptr, 0.0f, 1.0f, ImVec2(-1.0f, 80.0f));

    // right side: sliders and stuff
    ImGui::SeparatorText("Controls");

    ImGui::SliderFloat("Gain"   , &gain  ,  0.0f,  1.0f);
    ImGui::SliderFloat("Param 1", &param1,  0.0f,  1.0f);
    ImGui::SliderFloat("Param 2", &param2,  0.0f,  1.0f);
    ImGui::SliderFloat("Param 3", &param3,  0.0f,  1.0f);
    ImGui::SliderFloat("Param 4", &param4,  0.0f,  1.0f);
    ImGui::SliderFloat("Param 5", &param5,  0.0f,  1.0f);

    ImGui::NextColumn();
    ImGui::SeparatorText("Editor");

    const float console_h = 140.0f;
    const ImGuiStyle &st = ImGui::GetStyle();
    const float below = console_h + ImGui::GetFrameHeight() + st.ItemSpacing.y * 2;

    ImGui::InputTextMultiline("##code", code, sizeof code,
                          ImVec2(-FLT_MIN, -below),
                          ImGuiInputTextFlags_AllowTabInput);

    if (ImGui::Button("Compile", ImVec2(-FLT_MIN, 0)))
    {
        // todo: hook up actual compilation later
    }

    ImGui::BeginChild("##console", ImVec2(-FLT_MIN, console_h), ImGuiChildFlags_Borders);
    ImGui::TextUnformatted("[info] ready.\n[ok] waiting for changes");
    ImGui::EndChild();
}

int main()
{
    if (!platform_init("Veksel", 1100, 650))
    {
        return 1;
    }

    while (platform_begin_frame())
    {
        draw_ui();
        platform_end_frame();
    }

    platform_shutdown();
    return 0;
}