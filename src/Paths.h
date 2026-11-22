#pragma once

#include <filesystem>

namespace veksel
{
    // directory containing the running executable
    const std::filesystem::path& exeDir();
}