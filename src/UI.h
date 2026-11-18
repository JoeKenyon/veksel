#pragma once

#include "Audio.h"
#include "Engine.h"
#include "Log.h"
#include "TextEditor.h"
#include "imgui.h"

#include <cstddef>

namespace veksel
{
    class UI
    {
    public:
        explicit UI(Engine& engine, Audio& audio, Log& log);
        void render();

    private:
        void drawHeader();
        void drawEditor();
        void drawLog();
        void drawFileModal(const char* title, bool saving);
        void drawSettingsModal();

        void compile();
        void toggleAudio();
        void loadFile();
        void saveFile();

        ImFont* m_logoFont = nullptr;

        Engine& m_engine;
        Audio& m_audio;
        Log& m_log;
        TextEditor m_textEditor;

        // popup to open on the next frame. the header buttons live in a child
        // window so they can't open a popup that the main window draws
        const char* m_openPopup = nullptr;

        char m_path[256] = "patch.c";
        size_t m_lastLogCount = 0;

        // settings modal state
        AudioDevices m_devices;
        int m_inputIdx = 0;
        int m_inputChIdx = 0;
        int m_outputIdx = 0;
        int m_outputPairIdx = 0;
        int m_rateIdx = 0;
        int m_bufferIdx = 0;
    };
}