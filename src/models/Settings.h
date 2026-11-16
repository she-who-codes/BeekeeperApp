
#pragma once

#include <array>
#include <optional>
#include <string>
#include <vector>

// ==================================================
// Settings model
// --------------------------------------------------
// Everything the keeper configures in first-run setup
// and the Settings view. Stored in the database
// (migration 3). Built-in defaults live in
// settings/SettingsDefaults.* (single source).
//
// Enumerations are stored as stable text keys so the
// database never depends on translated text.
// ==================================================

// Calendar month/day without a year ("MM-DD" in the DB).
struct MonthDay
{
    int month = 1;  // 1..12
    int day = 1;    // 1..31 (validated against month)

    bool operator==(const MonthDay& other) const
    {
        return month == other.month && day == other.day;
    }

    bool operator!=(const MonthDay& other) const { return !(*this == other); }
};

enum class Hemisphere
{
    Northern,
    Southern
};

enum class TemperatureUnit
{
    Fahrenheit,     // default
    Celsius
};

enum class Season
{
    Spring,
    Summer,
    Fall,
    Winter
};

constexpr int SeasonCount = 4;

enum class SetupStatus
{
    NotStarted,     // first run: show the setup wizard
    Deferred,       // "Use defaults and finish later"
    Completed
};

// --------------------------------------------------
// App-wide settings
// --------------------------------------------------

struct AppSettings
{
    // "auto" = follow the Windows region, otherwise a
    // locale tag such as "en" or "en-GB".
    std::string locale = "auto";

    // "auto" = locale default, otherwise a pattern
    // such as "MM/DD/YYYY" (see DateFormat.h).
    std::string datePattern = "auto";

    TemperatureUnit temperatureUnit = TemperatureUnit::Fahrenheit;

    Hemisphere hemisphere = Hemisphere::Northern;

    // Start date of each season, indexed by Season.
    std::array<MonthDay, SeasonCount> seasonStarts {};

    // Off-season: ALL recurring maintenance tasks are paused
    // from offSeasonStart until (not including) offSeasonEnd.
    bool offSeasonPauseEnabled = true;
    MonthDay offSeasonStart {};
    MonthDay offSeasonEnd {};

    // Some keepers do not monitor mites. When false, mite
    // checks are hidden throughout the app.
    bool miteTrackingEnabled = true;

    SetupStatus setupStatus = SetupStatus::NotStarted;

    bool operator==(const AppSettings& o) const
    {
        return locale == o.locale
            && datePattern == o.datePattern
            && temperatureUnit == o.temperatureUnit
            && hemisphere == o.hemisphere
            && seasonStarts == o.seasonStarts
            && offSeasonPauseEnabled == o.offSeasonPauseEnabled
            && offSeasonStart == o.offSeasonStart
            && offSeasonEnd == o.offSeasonEnd
            && miteTrackingEnabled == o.miteTrackingEnabled
            && setupStatus == o.setupStatus;
    }
};

// --------------------------------------------------
// Mite check methods
// --------------------------------------------------

enum class MiteUnit
{
    PercentBees,    // mites per 100 bees (wash, roll, CO2)
    MitesPerDay,    // natural mite fall (sticky board)
    PercentCells    // % of drone cells infested (uncapping)
};

struct MiteThreshold
{
    // Empty = no alert for this season.
    std::optional<double> watch;  // amber
    std::optional<double> act;    // red

    bool operator==(const MiteThreshold& o) const
    {
        return watch == o.watch && act == o.act;
    }
};

struct MiteCheckMethod
{
    int id = 0;                     // 0 = not saved yet

    // Built-in key ("alcohol_wash"...) or empty for a
    // keeper-defined method (then customName is used).
    std::string key;
    std::string customName;

    MiteUnit unit = MiteUnit::PercentBees;

    // Bees per sample, days on board, or cells uncapped
    // depending on 'unit'.
    int sampleSize = 0;

    bool enabled = true;

    // Removed custom methods are archived, never deleted
    // (mite check records will reference them).
    bool archived = false;

    std::array<MiteThreshold, SeasonCount> thresholds {};

    bool IsBuiltIn() const { return !key.empty(); }

    bool operator==(const MiteCheckMethod& o) const
    {
        return id == o.id && key == o.key && customName == o.customName
            && unit == o.unit && sampleSize == o.sampleSize
            && enabled == o.enabled && archived == o.archived
            && thresholds == o.thresholds;
    }
};

// --------------------------------------------------
// Maintenance task types
// --------------------------------------------------

enum class IntervalUnit
{
    Days,
    Weeks,
    Months,
    Years
};

enum class TaskTrigger
{
    Recurring,          // every N units
    AfterTreatment      // N units after a mite treatment
};

struct MaintenanceTaskType
{
    int id = 0;

    std::string key;                // built-in key or empty
    std::string customName;

    bool enabled = true;

    int intervalValue = 1;
    IntervalUnit intervalUnit = IntervalUnit::Months;

    TaskTrigger trigger = TaskTrigger::Recurring;

    bool archived = false;

    bool IsBuiltIn() const { return !key.empty(); }

    bool operator==(const MaintenanceTaskType& o) const
    {
        return id == o.id && key == o.key && customName == o.customName
            && enabled == o.enabled && intervalValue == o.intervalValue
            && intervalUnit == o.intervalUnit && trigger == o.trigger
            && archived == o.archived;
    }
};

// --------------------------------------------------
// Everything together (what the editors work on)
// --------------------------------------------------

struct SettingsData
{
    AppSettings app;
    std::vector<MiteCheckMethod> miteMethods;
    std::vector<MaintenanceTaskType> taskTypes;

    bool operator==(const SettingsData& o) const
    {
        return app == o.app && miteMethods == o.miteMethods && taskTypes == o.taskTypes;
    }

    bool operator!=(const SettingsData& o) const { return !(*this == o); }
};
