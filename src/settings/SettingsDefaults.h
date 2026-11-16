
#pragma once

#include "models/Settings.h"

// ==================================================
// SettingsDefaults
// --------------------------------------------------
// SINGLE SOURCE for built-in mite check methods,
// built-in maintenance tasks and every prefilled value
// shown in first-run setup ("recommended defaults").
//
// Review these values for your region. They are a
// starting point; keepers can change all of them.
// ==================================================

namespace SettingsDefaults
{
    // ---------------- Catalogue ----------------

    struct MiteMethodInfo
    {
        const char* key;          // stored in the DB
        const char* labelKey;     // localization key
        MiteUnit unit;
    };

    struct TaskInfo
    {
        const char* key;
        const char* labelKey;
    };

    constexpr const char* MiteCheckTaskKey = "mite_check";

    const MiteMethodInfo* FindMiteMethod(const std::string& key);
    const TaskInfo* FindTask(const std::string& key);

    // ---------------- Defaults ----------------

    // Season start dates (meteorological seasons).
    std::array<MonthDay, SeasonCount> SeasonStarts(Hemisphere hemisphere);

    // Off-season pause: the winter season (from winter's
    // start until spring's start).
    void OffSeason(Hemisphere hemisphere, MonthDay& start, MonthDay& end);

    // Complete default settings (setup status NotStarted).
    SettingsData Create(Hemisphere hemisphere = Hemisphere::Northern);

    // Default method / task rows (used by Create() and to
    // add any built-in missing from an existing database).
    MiteCheckMethod CreateMiteMethod(const MiteMethodInfo& info);
    MaintenanceTaskType CreateTask(const TaskInfo& info);

    const MiteMethodInfo* MiteMethods(int& count);
    const TaskInfo* Tasks(int& count);
}
