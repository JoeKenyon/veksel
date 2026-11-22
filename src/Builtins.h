
#pragma once

#include <string>

struct TCCState;

namespace veksel
{
    // declarations prepended to user source
    const std::string& builtinDeclarations();

    // register functions that plugins are allowed to call
    void addBuiltinSymbols(TCCState* state);
}
