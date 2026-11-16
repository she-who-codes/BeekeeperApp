
#pragma once

#include "models/Settings.h"
#include "database/DatabaseManager.h"

#include <string>

// ==================================================
// SettingsRepository
// --------------------------------------------------
// Loads / saves SettingsData. Missing values fall back
// to SettingsDefaults, and any built-in method or task
// missing from the database is added on load, so newer
// versions can introduce built-ins safely.
// ==================================================

class SettingsRepository
{
public:
    explicit SettingsRepository(DatabaseManager& database);

    bool Load(SettingsData& out);

    // Saves everything in one transaction and assigns
    // database ids to new methods / tasks in 'data'.
    bool Save(SettingsData& data);

    std::string GetLastError() const;

private:
    DatabaseManager& m_database;
    std::string m_lastError;

    bool LoadAppSettings(SettingsData& data);
    bool LoadMiteMethods(SettingsData& data);
    bool LoadTaskTypes(SettingsData& data);

    bool SaveAppSettings(const AppSettings& app);
    bool SaveMiteMethods(std::vector<MiteCheckMethod>& methods);
    bool SaveTaskTypes(std::vector<MaintenanceTaskType>& tasks);
};
