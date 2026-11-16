
#include "DatabaseLocation.h"

#include "config/Paths.h"

#include "sqlite3.h"

#include <cstdlib>
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

namespace
{
    constexpr const char* AppFolder = "BeeKeeper";

    fs::path BaseDataFolder()
    {
#ifdef _WIN32
        wchar_t buffer[32768] = {};
        const DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", buffer, 32768);

        if (length > 0 && length < 32768)
            return fs::path(buffer);
#else
        if (const char* xdg = std::getenv("XDG_DATA_HOME"); xdg && *xdg)
            return fs::u8path(xdg);

        if (const char* home = std::getenv("HOME"); home && *home)
            return fs::u8path(home) / ".local" / "share";
#endif
        // Last resort: next to the executable.
        return fs::u8path(Paths::ExecutableDirectory());
    }

    // Copies 'source' into 'target' with the SQLite online
    // backup API (consistent snapshot, includes WAL content).
    bool BackupDatabase(const std::string& source, const std::string& target, std::string& error)
    {
        sqlite3* from = nullptr;
        sqlite3* to = nullptr;

        // Read-only: the legacy database is never modified
        // (not even checkpointed).
        bool ok = sqlite3_open_v2(source.c_str(), &from, SQLITE_OPEN_READONLY, nullptr) == SQLITE_OK
               && sqlite3_open_v2(target.c_str(), &to, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) == SQLITE_OK;

        if (ok)
        {
            sqlite3_backup* backup = sqlite3_backup_init(to, "main", from, "main");
            ok = backup != nullptr;

            if (ok)
            {
                const int step = sqlite3_backup_step(backup, -1);
                ok = (sqlite3_backup_finish(backup) == SQLITE_OK) && step == SQLITE_DONE;
            }
        }

        if (!ok)
            error = (to && sqlite3_errcode(to) != SQLITE_OK) ? sqlite3_errmsg(to)
                  : (from ? sqlite3_errmsg(from) : "Could not open the database.");

        sqlite3_close(to);
        sqlite3_close(from);
        return ok;
    }
}

namespace DatabaseLocation
{
    std::string DataDirectory()
    {
        const fs::path folder = BaseDataFolder() / AppFolder;

        std::error_code error;
        fs::create_directories(folder, error);

        return fs::is_directory(folder, error) ? folder.u8string() : std::string();
    }

    std::string DatabasePath()
    {
        const std::string directory = DataDirectory();

        return directory.empty()
            ? std::string(FileName)
            : (fs::u8path(directory) / FileName).u8string();
    }

    bool AdoptLegacyDatabase(std::string& copiedFrom, std::string& error)
    {
        copiedFrom.clear();
        error.clear();

        const fs::path target = fs::u8path(DatabasePath());
        std::error_code fsError;

        if (fs::exists(target, fsError))
            return true;                        // already in place

        std::vector<fs::path> candidates { fs::current_path(fsError) / FileName };

        const std::string exeDir = Paths::ExecutableDirectory();

        if (!exeDir.empty())
        {
            candidates.push_back(fs::u8path(exeDir) / FileName);
            candidates.push_back(fs::u8path(exeDir) / ".." / FileName);
        }

        for (const fs::path& candidate : candidates)
        {
            if (!fs::is_regular_file(candidate, fsError))
                continue;

            // Copy to a temporary file first and rename, so a
            // failed copy never leaves a half-written database
            // in the fixed location.
            fs::path temporary = target;
            temporary += ".copying";
            fs::remove(temporary, fsError);

            if (!BackupDatabase(candidate.u8string(), temporary.u8string(), error))
            {
                fs::remove(temporary, fsError);
                error = "Could not copy the existing database from " + candidate.u8string() + ": " + error;
                return false;
            }

            fs::rename(temporary, target, fsError);

            if (fsError)
            {
                fs::remove(temporary, fsError);
                error = "Could not place the copied database at " + target.u8string();
                return false;
            }

            copiedFrom = fs::weakly_canonical(candidate, fsError).u8string();
            return true;
        }

        return true;                            // nothing to adopt: a new database will be created
    }
}
