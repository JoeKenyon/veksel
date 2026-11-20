#include "UI.h"
#include <string>

using namespace veksel;



namespace
{
    const AudioDevice* findDevice(const std::vector<AudioDevice>& list, int index)
    {
        for (const auto& d : list)
            if (d.index == index) return &d;
        return nullptr;
    }

    bool deviceCombo(const char* id, const std::vector<AudioDevice>& list, int& index)
    {
        const AudioDevice* cur = findDevice(list, index);
        bool changed = false;

        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo(id, cur ? cur->name.c_str() : "Default"))
        {
            if (ImGui::Selectable("Default", index < 0))
            {
                index = -1;
                changed = true;
            }
            for (const auto& d : list)
            {
                ImGui::PushID(d.index);   // names can repeat
                if (ImGui::Selectable(d.name.c_str(), d.index == index))
                {
                    index = d.index;
                    changed = true;
                }
                ImGui::PopID();
            }
            ImGui::EndCombo();
        }
        return changed;
    }

    // pair: output channels are chosen as stereo pairs (1-2, 3-4, ...)
    void channelCombo(const char* id, int& channel, int count, bool pair)
    {
        const int step = pair ? 2 : 1;
        auto label = [&](int c)
        {
            std::string s = std::to_string(c + 1);
            if (pair && c + 1 < count) s += "-" + std::to_string(c + 2);
            return s;
        };

        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo(id, label(channel).c_str()))
        {
            for (int c = 0; c < std::max(count, 1); c += step)
                if (ImGui::Selectable(label(c).c_str(), c == channel))
                    channel = c;
            ImGui::EndCombo();
        }
    }

    void intCombo(const char* id, int& value, const std::vector<int>& options)
    {
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo(id, std::to_string(value).c_str()))
        {
            for (int o : options)
                if (ImGui::Selectable(std::to_string(o).c_str(), o == value))
                    value = o;
            ImGui::EndCombo();
        }
    }

    constexpr float kMinEditor    = 80.0f;
    constexpr float kMinLog       = 60.0f;
    constexpr float kHeaderHeight = 20.0f;
    constexpr float kCollapsedLog = kHeaderHeight;
}

UI::UI(Engine& e, Audio& a, Log& l) : m_engine(e), m_audio(a), m_log(l)
{
    // set up font
    ImGuiIO& io = ImGui::GetIO();
    ImGui::GetStyle().FontSizeBase = 13.0f;
    io.Fonts->AddFontDefaultBitmap();

    // stylings
    ImGui::StyleColorsDark();

    const ImVec4 background = {0.02f, 0.02f, 0.02f, 1.0f};
    const ImVec4 panel      = {0.08f, 0.08f, 0.08f, 1.0f};
    const ImVec4 control    = {0.14f, 0.14f, 0.14f, 1.0f};
    const ImVec4 border     = {0.22f, 0.22f, 0.22f, 1.0f};
    const ImVec4 accent     = {1.00f, 0.41f, 0.71f, 1.0f};

    m_textEditor.SetLanguage(TextEditor::LookupLanguage("C"));

    m_textEditor.SetPalette(TextEditor::GetDarkPalette());
    m_textEditor.SetText("float process_sample(float input)\n{\n    return input;\n}");
    m_engine.submitSource("float process_sample(float input)\n{\n    return input;\n}");

    /*
    ImVec4* colors = ImGui::GetStyle().Colors;
    colors[ImGuiCol_Button] = accent;
    colors[ImGuiCol_ButtonHovered] = panel;
    colors[ImGuiCol_WindowBg] = background;
    colors[ImGuiCol_ChildBg] = control;*/
}

void UI::drawHeader()
{
    ImGui::BeginChild("Header", {-FLT_MIN, kHeaderHeight}, ImGuiChildFlags_Borders);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {0,0});
    if (ImGui::Button("Settings", {0, -FLT_MIN})) m_openSettings = true;
    ImGui::SameLine();
    ImGui::Button("Save", {0, -FLT_MIN});
    ImGui::SameLine();
    ImGui::Button("Load", {0, -FLT_MIN});
    ImGui::SameLine();
    if(ImGui::Button("Compile", {0, -FLT_MIN})) m_engine.submitSource(m_textEditor.GetText());
    ImGui::PopStyleVar(1);
    ImGui::EndChild();
}

void UI::render()
{
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);   // default ItemSpacing kept
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {0,5});
    ImGui::Begin("VekselMain", nullptr, ImGuiWindowFlags_NoDecoration);
    drawHeader();
    drawEditor();
    drawLog();
    drawSettings();
    ImGui::End();
    ImGui::PopStyleVar(3);
}

void UI::drawSettings()
{
    if (m_openSettings)
    {
        m_devices = m_audio.getDevices();
        m_wanted = m_audio.getSettings();
        ImGui::OpenPopup("Settings");
        m_openSettings = false;
    }
    
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->GetCenter(), ImGuiCond_Appearing, {0.5f, 0.5f});
    ImGui::SetNextWindowSize({400.f, 0}, ImGuiCond_Appearing);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {20,20});

    if (ImGui::BeginPopupModal("Settings", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove))
    {
        const AudioDevice* in  = findDevice(m_devices.inputs,  m_wanted.inputDevice);
        const AudioDevice* out = findDevice(m_devices.outputs, m_wanted.outputDevice);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0,5});
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {10,10});

        // draw combos 
        ImGui::TextUnformatted("Input device");
        if (deviceCombo("##indev", m_devices.inputs, m_wanted.inputDevice)) m_wanted.inputChannel = 0;

        ImGui::Spacing();

        ImGui::TextUnformatted("Input channel");
        channelCombo("##inch", m_wanted.inputChannel, in ? in->channels : 0, false);

        ImGui::Spacing();

        ImGui::TextUnformatted("Output device");
        if (deviceCombo("##outdev", m_devices.outputs, m_wanted.outputDevice)) m_wanted.outputChannel = 0;

        ImGui::Spacing();

        ImGui::TextUnformatted("Output channels");
        channelCombo("##outch", m_wanted.outputChannel, out ? out->channels : 0, true);

        ImGui::Spacing();

        ImGui::TextUnformatted("Sample rate");
        intCombo("##sr", m_wanted.sampleRate, m_devices.sampleRates);

        ImGui::Spacing();

        ImGui::TextUnformatted("Buffer size");
        intCombo("##buf", m_wanted.bufferSize, m_devices.bufferSizes);

        ImGui::Spacing();

        const float w = (ImGui::GetContentRegionAvail().x - (ImGui::GetStyle().ItemSpacing.x * 2.0f)) * 0.5f;
        if (ImGui::Button("Apply", {w, 25.0f})) m_audio.start(m_engine, m_wanted);

        ImGui::SameLine();
        
        if (ImGui::Button("Close", {-FLT_MIN, 25.0f})) ImGui::CloseCurrentPopup();

        ImGui::PopStyleVar(2);
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar(1);
}

void UI::drawEditor()
{
    const float availY = ImGui::GetContentRegionAvail().y;
    const float sp     = ImGui::GetStyle().ItemSpacing.y;

    if (m_logCollapsed)
    {
        m_logLayoutH = kCollapsedLog;
    }
    else
    {
        const float maxLog = std::max(kMinLog, availY - kMinEditor - sp);
        m_logHeight  = std::clamp(m_logHeight, kMinLog, maxLog);
        m_logLayoutH = m_logHeight;
    }

    const float editorH = std::max(kMinEditor, availY - m_logLayoutH - sp);

    ImGui::BeginChild("Editor", {-FLT_MIN, editorH}, ImGuiChildFlags_Borders);
    m_textEditor.Render("TextEditor", {-FLT_MIN, -FLT_MIN});
    ImGui::EndChild();

    if (!m_logCollapsed)
    {
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - sp);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {0.0f, 0.0f});
        ImGui::InvisibleButton("##splitter", {-FLT_MIN, sp});
        ImGui::PopStyleVar();

        if (ImGui::IsItemHovered() || ImGui::IsItemActive())
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
        if (ImGui::IsItemActive())
            m_logHeight -= ImGui::GetIO().MouseDelta.y;   // applied next frame
    }
}

void UI::drawLog()
{
    ImGui::BeginChild("Log", {-FLT_MIN, m_logLayoutH}, ImGuiChildFlags_Borders);

    ImGui::BeginChild("LogHeader", {-FLT_MIN, kHeaderHeight}, ImGuiChildFlags_None);
    if (ImGui::Button(m_logCollapsed ? "^" : "v", {kHeaderHeight, kHeaderHeight}))
        m_logCollapsed = !m_logCollapsed;

    const ImVec2 p = ImGui::GetWindowPos();
    const float  w = ImGui::GetWindowWidth();
    const float  y = p.y + kHeaderHeight - 0.5f;
    ImGui::GetWindowDrawList()->AddLine({p.x, y}, {p.x + w, y},
                                        ImGui::GetColorU32(ImGuiCol_Border));
    ImGui::EndChild();

    if (!m_logCollapsed)
    {
        ImGui::BeginChild("LogBody", {-FLT_MIN, -FLT_MIN}, ImGuiChildFlags_None,
                          ImGuiWindowFlags_HorizontalScrollbar);

        const auto lines = m_log.getLines();
        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(lines.size()));
        while (clipper.Step())
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
                ImGui::TextUnformatted(lines[i].c_str());
        clipper.End();

        // follow new lines unless the user scrolled up
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
            ImGui::SetScrollHereY(1.0f);

        ImGui::EndChild();
    }

    ImGui::EndChild();
}