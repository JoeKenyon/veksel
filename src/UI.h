#pragma once

#include "VekselEngine.h"
#include "TextEditor.h"
#include "Audio.h"

namespace veksel
{
    class UI
    {
        public:
            explicit UI(Engine& engine, Audio& audio);
            void render();

        private:

            const char* m_logo =
                "██╗   ██╗███████╗██╗  ██╗███████╗███████╗██╗     \n"
                "██║   ██║██╔════╝██║ ██╔╝██╔════╝██╔════╝██║     \n"
                "██║   ██║█████╗  █████╔╝ ███████╗█████╗  ██║     \n"
                "╚██╗ ██╔╝██╔══╝  ██╔═██╗ ╚════██║██╔══╝  ██║     \n"
                " ╚████╔╝ ███████╗██║  ██╗███████║███████╗███████╗\n"
                "  ╚═══╝  ╚══════╝╚═╝  ╚═╝╚══════╝╚══════╝╚══════╝";

            // make these uinque porterieers maybe
            ImFont* m_logoFont = nullptr;
            ImFont* m_mainFont  = nullptr;
            ImFont* m_iconFont  = nullptr;

            // settings page and editor share the same spot, this picks which one shows
            bool m_showSettings = false;

            void drawHeader();
            void drawEditor();
            void drawLog();
            void drawSettingsPopup(ImVec2 anchor);

            Engine& m_engine;
            Audio& m_audio;
            TextEditor m_textEditor;
    };
}
