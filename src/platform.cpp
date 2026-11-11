#include "platform.h"
#include <GLFW/glfw3.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"



static void apply_style()
{
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    style.WindowRounding    = Theme::border_radius;
    style.ChildRounding     = Theme::border_radius;
    style.FrameRounding     = Theme::border_radius;
    style.PopupRounding     = Theme::border_radius;
    style.ScrollbarRounding = Theme::border_radius;
    style.GrabRounding      = Theme::border_radius;

    // Borders
    style.WindowBorderSize = 0.0f;
    style.ChildBorderSize  = 0.0f;
    style.FrameBorderSize  = 1.0f;
    style.PopupBorderSize  = 0.0f;

    // Spacing
    style.WindowPadding = ImVec2(2.0f, 2.0f);
    style.FramePadding  = ImVec2(12.0f, 12.0f);
    style.ItemSpacing   = ImVec2(1.f, 1.f);

    colors[ImGuiCol_WindowBg] = Theme::accent;
    colors[ImGuiCol_ChildBg]  = Theme::accent_neg;
    colors[ImGuiCol_PopupBg]  = Theme::accent_neg;
    colors[ImGuiCol_FrameBg]  = Theme::accent_neg;

    colors[ImGuiCol_Text]         = Theme::accent;

    colors[ImGuiCol_Button]        = Theme::accent;
    colors[ImGuiCol_ButtonHovered] = Theme::accent_neg;
    colors[ImGuiCol_ButtonActive]  = Theme::accent;

    // Combo field
    colors[ImGuiCol_FrameBgHovered] = Theme::accent_low;
    colors[ImGuiCol_FrameBgActive]  = Theme::accent;

    colors[ImGuiCol_Header]         = Theme::accent_low;
    colors[ImGuiCol_HeaderHovered]  = Theme::accent_med;;
    colors[ImGuiCol_HeaderActive]   = Theme::accent_low;

    colors[ImGuiCol_Separator]        = Theme::border;
    colors[ImGuiCol_SeparatorHovered] = Theme::accent_high;
    colors[ImGuiCol_SeparatorActive]  = Theme::accent;

    colors[ImGuiCol_ScrollbarBg]          = Theme::accent_low;  // track
    colors[ImGuiCol_ScrollbarGrab]        = Theme::accent_med;  // thumb, idle
    colors[ImGuiCol_ScrollbarGrabHovered] = Theme::accent_high;  // thumb, hovered
    colors[ImGuiCol_ScrollbarGrabActive]  = Theme::accent;      // thumb, dragged

    colors[ImGuiCol_Border] = Theme::border;
    colors[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);
}

static GLFWwindow *win;
ImFont* g_small_font = nullptr;

bool platform_init(const char *title, int w, int h)
{
    if (!glfwInit()) return false;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);

    win = glfwCreateWindow(w, h, title, nullptr, nullptr);
    if (!win)
    {
        glfwTerminate();
        return false;
    }
    glfwMakeContextCurrent(win);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    static const ImWchar ranges[] = { 0x20, 0x7F, 0x2500, 0x259F, 0 };

    // 1. Load the main font (16px) for the code editor and other stuff
    ImGui::GetIO().Fonts->AddFontFromFileTTF(
        "/System/Library/Fonts/Menlo.ttc", 19.0f, nullptr, ranges);

    // 2. Load the smaller font (12px) for the header logo
    g_small_font = ImGui::GetIO().Fonts->AddFontFromFileTTF(
        "/System/Library/Fonts/Menlo.ttc", 10.0f, nullptr, ranges);

    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(win, true);
    ImGui_ImplOpenGL3_Init("#version 150");
    return true;
}

bool platform_begin_frame()
{
    if (glfwWindowShouldClose(win)) return false;

    glfwPollEvents();
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    const ImGuiViewport *vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);

    ImGui::Begin("root", nullptr,
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoBringToFrontOnFocus
    );

    apply_style();

    return true;
}

void platform_end_frame()
{
    ImGui::End();

    ImGui::Render();

    int fw, fh;
    glfwGetFramebufferSize(win, &fw, &fh);
    glViewport(0, 0, fw, fh);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(win);
}

void platform_shutdown()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(win);
    glfwTerminate();
}
