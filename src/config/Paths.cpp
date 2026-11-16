
#include "Paths.h"

#include <filesystem>
#include <system_error>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace Paths
{
    std::string ExecutableDirectory()
    {
        std::error_code error;

#ifdef _WIN32
        wchar_t buffer[MAX_PATH * 4] = {};
        const DWORD length = GetModuleFileNameW(nullptr, buffer, static_cast<DWORD>(sizeof(buffer) / sizeof(buffer[0])));

        if (length == 0)
            return "";

        return fs::path(buffer).parent_path().u8string();
#else
        const fs::path exe = fs::read_symlink("/proc/self/exe", error);
        return error ? std::string() : exe.parent_path().u8string();
#endif
    }

    const std::string& Resources()
    {
        static const std::string resources = []
        {
            std::error_code error;

            std::vector<fs::path> candidates { fs::current_path(error) / "resources" };

            const std::string exeDir = ExecutableDirectory();

            if (!exeDir.empty())
            {
                candidates.push_back(fs::u8path(exeDir) / "resources");
                candidates.push_back(fs::u8path(exeDir) / ".." / "resources");
            }

            for (const fs::path& candidate : candidates)
            {
                if (fs::is_directory(candidate, error))
                    return fs::weakly_canonical(candidate, error).u8string();
            }

            return std::string("resources");
        }();

        return resources;
    }

    std::string Resource(const std::string& relative)
    {
        return (fs::u8path(Resources()) / fs::u8path(relative)).u8string();
    }
}
