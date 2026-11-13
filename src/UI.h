#pragma once

#include "VekselEngine.h"
#include "TextEditor.h"

class UI
{
public:
    explicit UI(veksel::Engine& engine);
    void render();

private:

    const char* logo =
        "██╗   ██╗███████╗██╗  ██╗███████╗███████╗██╗     \n"
        "██║   ██║██╔════╝██║ ██╔╝██╔════╝██╔════╝██║     \n"
        "██║   ██║█████╗  █████╔╝ ███████╗█████╗  ██║     \n"
        "╚██╗ ██╔╝██╔══╝  ██╔═██╗ ╚════██║██╔══╝  ██║     \n"
        " ╚████╔╝ ███████╗██║  ██╗███████║███████╗███████╗\n"
        "  ╚═══╝  ╚══════╝╚═╝  ╚═╝╚══════╝╚══════╝╚══════╝";

    // make these uinque porterieers maybe
    ImFont* logoFont = nullptr;
    ImFont* mainFont  = nullptr;
    ImFont* iconFont  = nullptr;

    // settings page and editor share the same spot, this picks which one shows
    bool show_settings = false;

    void draw_header();
    void draw_editor();
    void draw_log();

    veksel::Engine& engine;
    TextEditor text_editor;
};
