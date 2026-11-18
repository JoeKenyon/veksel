#pragma once

#include "Engine.h"
#include "Log.h"
#include "TextEditor.h"
#include "Audio.h"

namespace veksel
{
    class UI
    {
        public:
            explicit UI(Engine& engine, Audio& audio, Log& log);
            void render();

        private:

            const char* m_logo =
                "██╗   ██╗███████╗██╗  ██╗███████╗███████╗██╗     \n"
                "██║   ██║██╔════╝██║ ██╔╝██╔════╝██╔════╝██║     \n"
                "██║   ██║█████╗  █████╔╝ ███████╗█████╗  ██║     \n"
                "╚██╗ ██╔╝██╔══╝  ██╔═██╗ ╚════██║██╔══╝  ██║     \n"
                " ╚████╔╝ ███████╗██║  ██╗███████║███████╗███████╗\n"
                "  ╚═══╝  ╚══════╝╚═╝  ╚═╝╚══════╝╚══════╝╚══════╝";

            ImFont* m_logoFont = nullptr;
            ImFont* m_mainFont = nullptr;
            ImFont* m_iconFont = nullptr;

            void drawHeader();
            void drawEditor();
            void drawLog();
            void drawSettingsPopup(ImVec2 anchor);

            Engine& m_engine;
            Audio& m_audio;
            Log& m_log;
            TextEditor m_textEditor;
    };
}