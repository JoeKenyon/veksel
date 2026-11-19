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
    {
        std::scoped_lock lock{m_mutex};
        m_source = std::move(source);
    }
    m_wake.notify_one();
}

std::unique_ptr<Plugin> Compiler::take()
{
    std::scoped_lock lock{m_mutex};
    return std::move(m_result);
}

void Compiler::loop()
{
    for (;;)
    {
        std::string source;

        {
            std::unique_lock lock{m_mutex};
            m_wake.wait(lock, [this] { return m_stopping || m_source; });

            if (m_stopping)
                return;

            source = std::move(*m_source);
            m_source.reset();
        }

        if (auto plugin = compile(source))
        {
            std::scoped_lock lock{m_mutex};
            if (!m_stopping)
                m_result = std::move(plugin);
        }
    }
}

std::unique_ptr<Plugin> Compiler::compile(std::string_view source)
{
    m_log.write("[sys] Compiling (" + std::to_string(source.size()) + " bytes).");

    auto plugin = std::make_unique<Plugin>();
    plugin->state.reset(tcc_new());
    TCCState* const state = plugin->state.get();

    if (!state)
    {
        m_log.write("[error] tcc_new() failed.");
        return nullptr;
    }

    std::string errors;
    tcc_set_error_func(state, &errors, onTccError);
    tcc_set_lib_path(state, VEKSEL_TCC_LIB_PATH);
    tcc_set_output_type(state, TCC_OUTPUT_MEMORY);

    const auto fail = [&](const char* what) -> std::unique_ptr<Plugin>
    {
        m_log.write(std::string{"[error] "} + what);
        logErrors(errors);
        return nullptr;
    };

    // #line 1 makes error line numbers match the editor
    std::string full;
    full.append(Prelude).append("\n#line 1\n").append(source);

    if (tcc_compile_string(state, full.c_str()) == -1)
        return fail("Compilation failed.");

    if (tcc_relocate(state) < 0)
        return fail("Relocation failed.");

    plugin->function = reinterpret_cast<ProcessFunc>(tcc_get_symbol(state, "veksel_process_mono"));

    if (!plugin->function)
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