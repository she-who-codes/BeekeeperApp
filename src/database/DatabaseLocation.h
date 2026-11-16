
#pragma once

#include <string>

// ==================================================
// DatabaseLocation
// --------------------------------------------------
// BeeKeeper keeps its database in a fixed, per-user
// location, independent of the folder it was started
// from:
//
//   Windows : %LOCALAPPDATA%\BeeKeeper\beekeeper.db
//   Other   : $XDG_DATA_HOME/BeeKeeper/beekeeper.db
//             (default ~/.local/share/BeeKeeper)
//
// Earlier versions opened "beekeeper.db" in the start
// folder. On first start, an existing database from there
// is copied (never moved) into the fixed location.
// All paths are UTF-8.
// ==================================================

namespace DatabaseLocation
{
    constexpr const char* FileName = "beekeeper.db";

    // Folder for BeeKeeper's data (created if needed).
    // Empty if it could not be determined or created.
    std::string DataDirectory();

    // Full path of the database file.
    std::string DatabasePath();

    // If no database exists at DatabasePath() yet, copies
    // the first legacy database found (start folder,
    // executable folder, its parent) using SQLite's online
    // backup (includes changes still in the WAL file). The
    // legacy file is left untouched.
    //
    // Returns false on failure (error set); the caller
    // should not continue with an empty database. On
    // success 'copiedFrom' is the legacy path, or empty if
    // nothing had to be copied.
    bool AdoptLegacyDatabase(std::string& copiedFrom, std::string& error);
}
