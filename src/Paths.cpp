#include "Paths.h"

#include <string>
#include <system_error>

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#include <cstdint>
#elif defined(_WIN32)
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace veksel
{
    const fs::path& exeDir()
    {
        static const fs::path dir = []
        {
            std::error_code ec;
            fs::path exe;

#if defined(__APPLE__)
            uint32_t size = 0;
            _NSGetExecutablePath(nullptr, &size);
            std::string buf(size, '\0');
            if (_NSGetExecutablePath(buf.data(), &size) == 0)
                exe = buf.c_str();
#elif defined(__linux__)
            exe = fs::read_symlink("/proc/self/exe", ec);
#elif defined(_WIN32)
            std::wstring buf(MAX_PATH, L'\0');
            DWORD n = 0;
            for (;;)
            {
                n = GetModuleFileNameW(nullptr, buf.data(), static_cast<DWORD>(buf.size()));
                if (n < buf.size()) break;
                buf.resize(buf.size() * 2);
            }
            buf.resize(n);
            exe = buf;
#endif
            if (!exe.empty())
            {
                // resolve symlinks so a link to the binary still finds its files
                fs::path real = fs::weakly_canonical(exe, ec);
                if (!ec && !real.empty())
                    return real.parent_path();
            }

            return fs::current_path(ec);
        }();

        return dir;
    }
}