#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "Audio.h"
#include "Engine.h"
#include "Log.h"
#include "TextEditor.h"
#include "imgui.h"

namespace veksel
{
    struct PresetEntry
    {
        std::string name;
        std::string description;
        std::string code;
        std::filesystem::path path;
    };

    class UI
    {
    public:
        UI(Engine& e, Audio& a, Log& l);
        void render();

    private:
        void drawHeader();
        void drawEditor();
        void drawLog();
        void drawSettings();
        void drawSavePreset();
        void drawLoadPreset();

        Engine& m_engine;
        Audio&  m_audio;
        Log&    m_log;

        TextEditor m_textEditor;

        bool m_openSettings = false;
        AudioDevices m_devices;
        AudioSettings m_wanted;

        bool m_logCollapsed = false;
        float m_logLayoutH  = 250.0f;

        bool m_openSavePreset = false;
        bool m_openLoadPreset = false;
        char m_presetNameBuf[128] = "untitled";
        char m_presetDescBuf[256] = "";
        std::vector<PresetEntry> m_cachedPresets;
    };
}