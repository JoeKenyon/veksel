#include "platform.h"
#include <GLFW/glfw3.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

static GLFWwindow *win;

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
    ImGui::GetIO().Fonts->AddFontFromFileTTF(
        "/System/Library/Fonts/Menlo.ttc", 16.0f, nullptr, ranges);

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
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus);
    return true;
}

void platform_end_frame()
{
    ImGui::End();
    ImGui::Render();

    int fw, fh;
    glfwGetFramebufferSize(win, &fw, &fh);
    glViewport(0, 0, fw, fh);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
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
