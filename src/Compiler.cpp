#include "Compiler.h"
#include "Log.h"


//
#if JUCE_MAC
    #define SHARED_EXT ".dylib"
#else
    #define SHARED_EXT ".so"
#endif

namespace
{
    // added to the user's source so they can focus on veksel_main
    const char* PRELUDE =
        "#include <math.h>\n"
        "#include <stdlib.h>\n"
        "#include <stdio.h>\n"
        "#ifndef M_PI\n#define M_PI 3.14159265358979323846\n#endif\n"
        "float veksel_process_mono(float input);\n"
        "#define veksel_main float veksel_process_mono(float input)\n";

    Plugin* build_plugin(const juce::File& dir, const juce::String& source)
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

    Plugin* compile_source(const juce::String& source)
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
}

CompilerThread::CompilerThread(PluginSlots& s)
    : juce::Thread("CompilerThread"), slots(s)
{
}

void CompilerThread::set_code(const juce::String& s)
{
    {
        const juce::ScopedLock sl(code_lock);
        code = s;
    }
    notify();
}

void CompilerThread::run()
{
    while (! threadShouldExit())
    {
        // free whatever the audio thread retired
        delete slots.to_be_deleted.exchange(nullptr);

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
            delete slots.pending.exchange(new_plugin);
            push_log("[ok] compiled and swapped in");
        }
        else
        {
            push_log("[error] keeping current plugin");
        }
    }
}
