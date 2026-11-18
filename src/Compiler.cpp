#include "Compiler.h"
#include "libtcc.h"

#include <string>
#include <utility>

using namespace veksel;

namespace
{
    constexpr std::string_view Prelude =
        "#include <math.h>\n"
        "#include <stdlib.h>\n"
        "#include <stdio.h>\n"
        "#ifndef M_PI\n"
        "#define M_PI 3.14159265358979323846\n"
        "#endif\n"
        "float veksel_process_mono(float input);\n"
        "#define veksel_main float veksel_process_mono(float input)\n";

    // collects tcc diagnostics into the std::string passed as `opaque`
    void onTccError(void* opaque, const char* message)
    {
        if (!opaque || !message)
            return;

        std::string text = message;

        // tcc calls the source "<string>:", show it as "line N:" instead
        constexpr std::string_view needle = "<string>:";
        for (auto pos = text.find(needle); pos != std::string::npos; pos = text.find(needle, pos))
            text.replace(pos, needle.size(), "line ");

        auto& errors = *static_cast<std::string*>(opaque);
        errors += text;
        errors += '\n';
    }
}

Compiler::Compiler(Log& log)
    : m_log(log)
    , m_thread{&Compiler::loop, this}
{
}

Compiler::~Compiler()
{
    {
        std::scoped_lock lock{m_mutex};
        m_stopping = true;
    }

    m_wake.notify_one();
    m_thread.join();
}

void TccDeleter::operator()(TCCState* state) const noexcept
{
    tcc_delete(state);
}

void Compiler::submit(std::string source)
{
    m_log.write("[sys] Compilation requested (" + std::to_string(source.size()) + " bytes).");
    
    {
        std::scoped_lock lock{m_mutex};
        m_source = std::move(source);
    }

    // tell compiler loop we have some new source code
    m_wake.notify_one();
}

std::optional<Plugin> Compiler::take()
{
    // lock the mutex
    // get the compiled plugin and swap it with nothing
    // so we have space for the new plugin
    std::scoped_lock lock{m_mutex};
    return std::exchange(m_result, std::nullopt);
}

void Compiler::loop()
{
    for (;;)
    {
        std::string source;

        {
            // lock on mutex
            std::unique_lock lock{m_mutex};

            // wait condition
            m_wake.wait(lock, [this] { return m_stopping || m_source; });

            if (m_stopping) return;

            // move that source
            source = std::move(*m_source);
            m_source.reset();
        }

        // if we have a plugin
        if (auto plugin = compile(source))
        {
            // move the plugin to result
            std::scoped_lock lock{m_mutex};
            if (!m_stopping) m_result = std::move(plugin);
        }
    }
}

// compile the user's c source into executable memory
std::optional<Plugin> Compiler::compile(std::string_view source)
{
    m_log.write("[sys] Starting TCC compilation.");
    m_log.write("[sys] Source size: " + std::to_string(source.size()) + " bytes.");

    Plugin plugin;
    plugin.state.reset(tcc_new());
    TCCState* const state = plugin.state.get();

    if (!state)
    {
        m_log.write("[error] tcc_new() failed.");
        return std::nullopt;
    }

    std::string errors;
    tcc_set_error_func(state, &errors, onTccError);
    tcc_set_lib_path(state, VEKSEL_TCC_LIB_PATH);

    // memory output - no files written
    tcc_set_output_type(state, TCC_OUTPUT_MEMORY);

    // logs what went wrong plus tcc's own diagnostics. returning frees the
    // plugin, and with it the tcc state.
    const auto fail = [&](std::string_view what)
    {
        m_log.write("[error] " + std::string{what});
        logErrors(errors);
        return std::optional<Plugin>{std::nullopt};
    };

    // #line 1 makes error line numbers match the user's editor
    std::string full;
    full.append(Prelude).append("\n#line 1\n").append(source);

    if (tcc_compile_string(state, full.c_str()) == -1)
        return fail("Compilation failed.");

    if (tcc_relocate(state) < 0)
        return fail("Relocation failed.");

    plugin.function = reinterpret_cast<ProcessFunc>(tcc_get_symbol(state, "veksel_process_mono"));

    if (!plugin.function)
        return fail("Symbol 'veksel_process_mono' not found.");

    m_log.write("[ok] Compilation successful.");
    return plugin;
}

// writes each line of `text` to the log as an error
void Compiler::logErrors(std::string_view text)
{
    while (!text.empty())
    {
        const auto end = text.find('\n');
        const auto line = text.substr(0, end);

        if (!line.empty())
            m_log.write("[error] " + std::string{line});

        text.remove_prefix(end == std::string_view::npos ? text.size() : end + 1);
    }
}