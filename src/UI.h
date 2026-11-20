#pragma once

#include "Audio.h"
#include "Engine.h"
#include "Log.h"
#include "TextEditor.h"
#include "imgui.h"

namespace veksel
{
    class UI
    {
    public:
        explicit UI(Engine& engine, Audio& audio, Log& log);
        void render();

    private:

        // main drawing stuff
        void drawHeader();
        void drawEditor();
        void drawLog();

        // draw internal things
        bool m_openSettings = false;
        void drawSettings();

        bool  m_logCollapsed = false;
        float m_logHeight    = 150.0f;   // user-set, only changed by dragging
        float m_logLayoutH   = 25.0f;    // height actually used this frame

        ImFont* m_logoFont = nullptr;
        ImFont* m_iconFont = nullptr;

        Engine& m_engine;
        Audio& m_audio;
        Log& m_log;
        TextEditor m_textEditor;

        // settings modal: device lists, and the settings being edited
        AudioDevices m_devices;
        AudioSettings m_wanted;
    };
}