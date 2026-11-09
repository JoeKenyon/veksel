#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include <atomic>
#include <cmath>
#include <memory>
#include <optional>

#if JUCE_MAC
    #define SHARED_EXT ".dylib"
#else
    #define SHARED_EXT ".so"
#endif

// ---------------------------------------------------------------------------
// log (capped at 100 lines)
// ---------------------------------------------------------------------------
static juce::CriticalSection log_lock;
static juce::StringArray     log_lines;
static std::atomic<bool>     log_dirty{false};

static void push_log(const juce::String& msg)
{
    const juce::ScopedLock sl(log_lock);
    log_lines.add(msg);
    if (log_lines.size() > 100)
        log_lines.remove(0);
    log_dirty = true;
}

// ---------------------------------------------------------------------------
// plugin
// ---------------------------------------------------------------------------
using PluginFunc = float (*)(float);

struct Plugin
{
    std::unique_ptr<juce::DynamicLibrary> lib; // closes the library on destruction
    PluginFunc function = nullptr;
};

static Plugin*              current_plugin = nullptr; // audio thread only
static std::atomic<Plugin*> pending_plugin{nullptr};
static std::atomic<Plugin*> to_be_deleted_plugin{nullptr};

static const char* PRELUDE =
    "#include <math.h>\n"
    "#include <stdlib.h>\n"
    "#include <stdio.h>\n"
    "#ifndef M_PI\n#define M_PI 3.14159265358979323846\n#endif\n"
    "float veksel_process_mono(float input);\n"
    "#define veksel_main float veksel_process_mono(float input)\n";

static const char* DEFAULT_SOURCE =
    "veksel_main\n"
    "{\n"
    "   return input;\n"
    "}\n";

static const char* LOGO =
    "██╗   ██╗███████╗██╗  ██╗███████╗███████╗██╗     \n"
    "██║   ██║██╔════╝██║ ██╔╝██╔════╝██╔════╝██║     \n"
    "██║   ██║█████╗  █████╔╝ ███████╗█████╗  ██║     \n"
    "╚██╗ ██╔╝██╔══╝  ██╔═██╗ ╚════██║██╔══╝  ██║     \n"
    " ╚████╔╝ ███████╗██║  ██╗███████║███████╗███████╗\n"
    "  ╚═══╝  ╚══════╝╚═╝  ╚═╝╚══════╝╚══════╝╚══════╝";

static float params[6] = {};

// ---------------------------------------------------------------------------
// compile: source -> temp dir -> clang -> DynamicLibrary
// ---------------------------------------------------------------------------
static Plugin* build_plugin(const juce::File& dir, const juce::String& source)
{
    auto source_file = dir.getChildFile("plugin.c");
    auto lib_file    = dir.getChildFile("plugin" SHARED_EXT);

    if (! source_file.replaceWithText(juce::String(PRELUDE) + "#line 1\n" + source))
    {
        push_log("[error] could not write source file");
        return nullptr;
    }

    juce::StringArray cmd { "/usr/bin/clang", "-O3", "-march=native", "-ffast-math", "-shared", "-fPIC" };
   #if JUCE_MAC
    cmd.add("-undefined");
    cmd.add("dynamic_lookup");
   #endif
    cmd.add(source_file.getFullPathName());
    cmd.add("-o");
    cmd.add(lib_file.getFullPathName());

    juce::ChildProcess proc;
    if (! proc.start(cmd, juce::ChildProcess::wantStdOut | juce::ChildProcess::wantStdErr))
    {
        push_log("[error] failed to start clang");
        return nullptr;
    }

    // reads until the child closes its pipe, so large output can't deadlock it
    auto output = proc.readAllProcessOutput();
    proc.waitForProcessToFinish(-1);

    if (proc.getExitCode() != 0)
    {
        juce::StringArray lines;
        lines.addLines(output);
        for (auto& line : lines)
            push_log(line);

        push_log("[error] compile failed!");
        return nullptr;
    }

    auto lib = std::make_unique<juce::DynamicLibrary>();
    if (! lib->open(lib_file.getFullPathName()))
    {
        push_log("dlopen: failed to load " + lib_file.getFullPathName());
        return nullptr;
    }

    auto function = (PluginFunc) lib->getFunction("veksel_process_mono");
    if (! function)
    {
        push_log("dlsym: no veksel_process_mono");
        return nullptr; // lib closes itself
    }

    auto* p     = new Plugin();
    p->lib      = std::move(lib);
    p->function = function;
    return p;
}

static Plugin* compile_source(const juce::String& source)
{
    auto temp_dir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                        .getChildFile("veksel_" + juce::Uuid().toString());

    if (! temp_dir.createDirectory())
    {
        push_log("[error] could not create temp dir");
        return nullptr;
    }

    Plugin* p = build_plugin(temp_dir, source);

    // safe to delete: the library is already mapped into memory
    if (! temp_dir.deleteRecursively())
        push_log("cleanup failed: " + temp_dir.getFullPathName());

    return p;
}

// ---------------------------------------------------------------------------
// compiler thread
// ---------------------------------------------------------------------------
class CompilerThread : public juce::Thread
{
public:
    CompilerThread() : juce::Thread("CompilerThread") {}

    void set_code(const juce::String& s)
    {
        {
            const juce::ScopedLock sl(code_lock);
            code = s;
        }
        notify();
    }

    void run() override
    {
        while (! threadShouldExit())
        {
            // free whatever the audio thread retired
            delete to_be_deleted_plugin.exchange(nullptr);

            // short timeout so retired plugins keep getting drained
            wait(10);
            if (threadShouldExit())
                return;

            std::optional<juce::String> source;
            {
                const juce::ScopedLock sl(code_lock);
                source = std::move(code);
                code.reset();
            }

            if (! source)
                continue;

            if (Plugin* new_plugin = compile_source(*source))
            {
                // whatever was pending is old news
                delete pending_plugin.exchange(new_plugin);
                push_log("[ok] compiled and swapped in");
            }
            else
            {
                push_log("[error] keeping current plugin");
            }
        }
    }

private:
    juce::CriticalSection       code_lock;
    std::optional<juce::String> code;
};

// ---------------------------------------------------------------------------
// audio
// ---------------------------------------------------------------------------
class AudioEngine : public juce::AudioIODeviceCallback
{
public:
    void audioDeviceIOCallbackWithContext(const float* const* in, int num_in,
                                          float* const* out, int num_out,
                                          int num_samples,
                                          const juce::AudioIODeviceCallbackContext&) override
    {
        // swap in a pending plugin if the previous retiree has been collected
        if (to_be_deleted_plugin.load() == nullptr)
        {
            if (Plugin* new_plugin = pending_plugin.exchange(nullptr))
            {
                Plugin* old_plugin = current_plugin;
                current_plugin = new_plugin;
                if (old_plugin)
                    to_be_deleted_plugin.store(old_plugin);
            }
        }

        Plugin* plugin = current_plugin;
        const float* input = (num_in > 0) ? in[0] : nullptr;

        for (int i = 0; i < num_samples; ++i)
        {
            float sample = input ? input[i] : 0.0f;

            if (plugin)
                sample = plugin->function(sample);

            if (! std::isfinite(sample)) sample = 0.0f;
            sample = juce::jlimit(-1.0f, 1.0f, sample);

            for (int ch = 0; ch < num_out; ++ch)
                if (out[ch]) out[ch][i] = sample;
        }
    }

    void audioDeviceAboutToStart(juce::AudioIODevice*) override {}
    void audioDeviceStopped() override {}
};

// ---------------------------------------------------------------------------
// ui
// ---------------------------------------------------------------------------
class MainComponent : public juce::Component, private juce::Timer
{
public:
    MainComponent()
    {
        // editor
        codeEditor.loadContent(DEFAULT_SOURCE);
        codeEditor.setColour(juce::CodeEditorComponent::backgroundColourId,  juce::Colours::black);
        codeEditor.setColour(juce::CodeEditorComponent::defaultTextColourId, juce::Colours::white);
        addAndMakeVisible(codeEditor);

        // compile button
        compileButton.setButtonText("Compile & Run");
        compileButton.onClick = [this]
        {
            compiler.set_code(codeEditor.getDocument().getAllContent());
            push_log("[info] compilation triggered via button");
        };
        addAndMakeVisible(compileButton);

        // log
        logText.setMultiLine(true);
        logText.setReadOnly(true);
        logText.setColour(juce::TextEditor::backgroundColourId, juce::Colours::black);
        addAndMakeVisible(logText);

        // params
        for (int i = 0; i < 6; ++i)
        {
            param_labels[i].setText("Param " + juce::String(i + 1), juce::dontSendNotification);
            addAndMakeVisible(param_labels[i]);

            auto& s = param_sliders[i];
            s.setRange(0.0, 1.0);
            s.setSliderStyle(juce::Slider::LinearHorizontal);
            s.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
            s.onValueChange = [&s, i] { params[i] = (float) s.getValue(); };
            addAndMakeVisible(s);
        }

        // devices
        in_label.setText("Input", juce::dontSendNotification);
        out_label.setText("Output", juce::dontSendNotification);
        addAndMakeVisible(in_label);
        addAndMakeVisible(out_label);
        addAndMakeVisible(in_combo);
        addAndMakeVisible(out_combo);

        // audio: mono in, stereo out, 44.1k, 64 frames
        juce::AudioDeviceManager::AudioDeviceSetup setup;
        setup.sampleRate = 44100.0;
        setup.bufferSize = 64;

        auto error = device_manager.initialise(1, 2, nullptr, true, {}, &setup);
        if (error.isNotEmpty())
        {
            push_log("[error] audio device: " + error);
        }
        else
        {
            device_manager.addAudioCallback(&audio_engine);
            push_log("[ok] audio engine started");
        }

        populate_devices();

        // compiler + initial compile
        compiler.startThread();
        compiler.set_code(DEFAULT_SOURCE);

        setSize(900, 800);
        startTimerHz(10);
    }

    ~MainComponent() override
    {
        stopTimer();

        compiler.signalThreadShouldExit();
        compiler.notify();
        compiler.stopThread(-1); // join

        device_manager.removeAudioCallback(&audio_engine);
        device_manager.closeAudioDevice();

        delete current_plugin;
        current_plugin = nullptr;
        delete pending_plugin.exchange(nullptr);
        delete to_be_deleted_plugin.exchange(nullptr);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colours::black);

        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 10.0f, juce::Font::plain)));
        g.drawFittedText(LOGO, header_bounds, juce::Justification::centred, 6, 1.0f);

        g.setColour(juce::Colours::darkgrey);
        g.drawRect(header_bounds);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(10);

        header_bounds = area.removeFromTop(70);
        area.removeFromTop(8);

        logText.setBounds(area.removeFromBottom(160));
        area.removeFromBottom(8);

        // left: settings
        auto left = area.removeFromLeft(area.getWidth() / 3);
        area.removeFromLeft(8);

        auto row = [&left](juce::Component& label, juce::Component& control)
        {
            auto r = left.removeFromTop(24);
            label.setBounds(r.removeFromLeft(60));
            control.setBounds(r);
            left.removeFromTop(4);
        };

        row(in_label, in_combo);
        row(out_label, out_combo);
        left.removeFromTop(8);
        for (int i = 0; i < 6; ++i)
            row(param_labels[i], param_sliders[i]);

        // right: editor + compile button
        compileButton.setBounds(area.removeFromBottom(25));
        codeEditor.setBounds(area);
    }

private:
    void timerCallback() override
    {
        if (! log_dirty.exchange(false))
            return;

        juce::String text;
        {
            const juce::ScopedLock sl(log_lock);
            text = log_lines.joinIntoString("\n");
        }

        logText.setText(text, false);
        logText.moveCaretToEnd();
    }

    void populate_devices()
    {
        auto* type = device_manager.getCurrentDeviceTypeObject();
        if (! type)
            return;

        auto setup = device_manager.getAudioDeviceSetup();

        in_combo.clear(juce::dontSendNotification);
        out_combo.clear(juce::dontSendNotification);
        in_combo.addItemList(type->getDeviceNames(true), 1);
        out_combo.addItemList(type->getDeviceNames(false), 1);
        in_combo.setText(setup.inputDeviceName, juce::dontSendNotification);
        out_combo.setText(setup.outputDeviceName, juce::dontSendNotification);

        in_combo.onChange  = [this] { change_device(true); };
        out_combo.onChange = [this] { change_device(false); };
    }

    void change_device(bool is_input)
    {
        auto setup = device_manager.getAudioDeviceSetup();

        if (is_input) setup.inputDeviceName  = in_combo.getText();
        else          setup.outputDeviceName = out_combo.getText();

        auto error = device_manager.setAudioDeviceSetup(setup, true);
        if (error.isNotEmpty())
            push_log("[error] audio device: " + error);
    }

    juce::CodeDocument                codeDocument;
    juce::CPlusPlusCodeTokeniser      codeTokeniser;
    juce::CodeEditorComponent         codeEditor{ codeDocument, &codeTokeniser };
    juce::TextButton                  compileButton;
    juce::TextEditor                  logText;

    juce::Label                       in_label, out_label, param_labels[6];
    juce::ComboBox                    in_combo, out_combo;
    juce::Slider                      param_sliders[6];
    juce::Rectangle<int>              header_bounds;

    CompilerThread                    compiler;
    AudioEngine                       audio_engine;
    juce::AudioDeviceManager          device_manager;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};

// ---------------------------------------------------------------------------
// app
// ---------------------------------------------------------------------------
class VekselApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override    { return "Veksel"; }
    const juce::String getApplicationVersion() override { return "1.0.0"; }

    void initialise(const juce::String&) override { main_window = std::make_unique<MainWindow>(getApplicationName()); }
    void shutdown() override                      { main_window = nullptr; }

private:
    class MainWindow : public juce::DocumentWindow
    {
    public:
        MainWindow(juce::String name)
            : DocumentWindow(name, juce::Colours::black, DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar(true);
            setContentOwned(new MainComponent(), true);
            centreWithSize(getWidth(), getHeight());
            setVisible(true);
        }

        void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
    };

    std::unique_ptr<MainWindow> main_window;
};

START_JUCE_APPLICATION(VekselApplication)
