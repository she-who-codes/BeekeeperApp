
#include "SettingsRepository.h"

#include "database/Statement.h"
#include "settings/Seasons.h"
#include "settings/SettingsDefaults.h"

#include <map>
#include <utility>

namespace
{
    // ---------------------------------------------
    // Enum <-> stable text keys
    // ---------------------------------------------

    const char* ToKey(MiteUnit unit)
    {
        switch (unit)
        {
            case MiteUnit::PercentBees:  return "percent_bees";
            case MiteUnit::MitesPerDay:  return "mites_per_day";
            case MiteUnit::PercentCells: return "percent_cells";
        }

        return "percent_bees";
    }

    MiteUnit MiteUnitFromKey(const std::string& key)
    {
        if (key == "mites_per_day") return MiteUnit::MitesPerDay;
        if (key == "percent_cells") return MiteUnit::PercentCells;
        return MiteUnit::PercentBees;
    }

    const char* ToKey(IntervalUnit unit)
    {
        switch (unit)
        {
            case IntervalUnit::Days:   return "days";
            case IntervalUnit::Weeks:  return "weeks";
            case IntervalUnit::Months: return "months";
            case IntervalUnit::Years:  return "years";
        }

        return "months";
    }

    IntervalUnit IntervalUnitFromKey(const std::string& key)
    {
        if (key == "days")  return IntervalUnit::Days;
        if (key == "weeks") return IntervalUnit::Weeks;
        if (key == "years") return IntervalUnit::Years;
        return IntervalUnit::Months;
    }

    const char* ToKey(TaskTrigger trigger)
    {
        return trigger == TaskTrigger::AfterTreatment ? "after_treatment" : "recurring";
    }

    const char* ToKey(SetupStatus status)
    {
        switch (status)
        {
            case SetupStatus::NotStarted: return "not_started";
            case SetupStatus::Deferred:   return "deferred";
            case SetupStatus::Completed:  return "completed";
        }

        return "not_started";
    }

    SetupStatus SetupStatusFromKey(const std::string& key)
    {
        if (key == "deferred")  return SetupStatus::Deferred;
        if (key == "completed") return SetupStatus::Completed;
        return SetupStatus::NotStarted;
    }

    // Setting keys (app_settings.key)
    namespace Keys
    {
        constexpr const char* Locale           = "locale";
        constexpr const char* DatePattern      = "date_pattern";
        constexpr const char* TemperatureUnit  = "temperature_unit";
        constexpr const char* Hemisphere       = "hemisphere";
        constexpr const char* SeasonStartPrefix = "season_start.";
        constexpr const char* OffSeasonEnabled = "off_season.enabled";
        constexpr const char* OffSeasonStart   = "off_season.start";
        constexpr const char* OffSeasonEnd     = "off_season.end";
        constexpr const char* MiteTracking     = "mite_tracking.enabled";
        constexpr const char* SetupStatus      = "setup.status";
    }
}

SettingsRepository::SettingsRepository(DatabaseManager& database)
    : m_database(database)
{
}

std::string SettingsRepository::GetLastError() const
{
    return m_lastError;
}

// --------------------------------------------------
// Load
// --------------------------------------------------

bool SettingsRepository::Load(SettingsData& out)
{
    if (!m_database.GetHandle())
    {
        m_lastError = "Database is not open.";
        return false;
    }

    SettingsData data = SettingsDefaults::Create(Hemisphere::Northern);
    data.miteMethods.clear();
    data.taskTypes.clear();

    if (!LoadAppSettings(data) || !LoadMiteMethods(data) || !LoadTaskTypes(data))
        return false;

    // First run (nothing stored yet): use the complete
    // defaults, including which method starts selected.
    if (data.miteMethods.empty())
        data.miteMethods = SettingsDefaults::Create(data.app.hemisphere).miteMethods;

    // Add built-ins missing from the database (a newer
    // version with additional built-ins; these start
    // unticked).
    int count = 0;
    const SettingsDefaults::MiteMethodInfo* methods = SettingsDefaults::MiteMethods(count);

    for (int i = 0; i < count; ++i)
    {
        bool found = false;

        for (const MiteCheckMethod& method : data.miteMethods)
            found = found || method.key == methods[i].key;

        if (!found)
            data.miteMethods.push_back(SettingsDefaults::CreateMiteMethod(methods[i]));
    }

    const SettingsDefaults::TaskInfo* tasks = SettingsDefaults::Tasks(count);

    for (int i = 0; i < count; ++i)
    {
        bool found = false;

        for (const MaintenanceTaskType& task : data.taskTypes)
            found = found || task.key == tasks[i].key;

        if (!found)
            data.taskTypes.push_back(SettingsDefaults::CreateTask(tasks[i]));
    }

    out = std::move(data);
    m_lastError.clear();
    return true;
}

bool SettingsRepository::LoadAppSettings(SettingsData& data)
{
    sqlite3* db = m_database.GetHandle();
    Statement statement(db, "SELECT key, value FROM app_settings;");

    if (!statement.Ok())
    {
        m_lastError = sqlite3_errmsg(db);
        return false;
    }

    std::map<std::string, std::string> values;
    int result = SQLITE_OK;

    while ((result = statement.Step()) == SQLITE_ROW)
        values[statement.ColumnText(0)] = statement.ColumnText(1);

    if (result != SQLITE_DONE)
    {
        m_lastError = sqlite3_errmsg(db);
        return false;
    }

    AppSettings& app = data.app;

    auto get = [&values](const char* key, std::string& out)
    {
        const auto it = values.find(key);

        if (it == values.end())
            return false;

        out = it->second;
        return true;
    };

    std::string value;

    if (get(Keys::Locale, value) && !value.empty())
        app.locale = value;

    if (get(Keys::DatePattern, value) && !value.empty())
        app.datePattern = value;

    if (get(Keys::TemperatureUnit, value))
        app.temperatureUnit = value == "C" ? TemperatureUnit::Celsius : TemperatureUnit::Fahrenheit;

    if (get(Keys::Hemisphere, value))
    {
        app.hemisphere = value == "southern" ? Hemisphere::Southern : Hemisphere::Northern;

        // Defaults follow the stored hemisphere; explicit
        // values below override them.
        app.seasonStarts = SettingsDefaults::SeasonStarts(app.hemisphere);
        SettingsDefaults::OffSeason(app.hemisphere, app.offSeasonStart, app.offSeasonEnd);
    }

    for (int s = 0; s < SeasonCount; ++s)
    {
        MonthDay start;

        if (get((std::string(Keys::SeasonStartPrefix) + Seasons::Key(static_cast<Season>(s))).c_str(), value)
            && Seasons::FromText(value, start))
        {
            app.seasonStarts[s] = start;
        }
    }

    if (get(Keys::OffSeasonEnabled, value))
        app.offSeasonPauseEnabled = value == "1";

    MonthDay day;

    if (get(Keys::OffSeasonStart, value) && Seasons::FromText(value, day))
        app.offSeasonStart = day;

    if (get(Keys::OffSeasonEnd, value) && Seasons::FromText(value, day))
        app.offSeasonEnd = day;

    if (get(Keys::MiteTracking, value))
        app.miteTrackingEnabled = value == "1";

    if (get(Keys::SetupStatus, value))
        app.setupStatus = SetupStatusFromKey(value);

    return true;
}

bool SettingsRepository::LoadMiteMethods(SettingsData& data)
{
    sqlite3* db = m_database.GetHandle();

    Statement methods(db,
        "SELECT id, method_key, custom_name, unit, sample_size, is_enabled, is_archived "
        "FROM mite_check_methods ORDER BY sort_order, id;");

    Statement thresholds(db,
        "SELECT season, watch_level, act_level "
        "FROM mite_method_thresholds WHERE method_id = ?;");

    if (!methods.Ok() || !thresholds.Ok())
    {
        m_lastError = sqlite3_errmsg(db);
        return false;
    }

    int result = SQLITE_OK;

    while ((result = methods.Step()) == SQLITE_ROW)
    {
        MiteCheckMethod method;
        method.id = methods.ColumnInt(0);
        method.key = methods.ColumnText(1);
        method.customName = methods.ColumnText(2);
        method.unit = MiteUnitFromKey(methods.ColumnText(3));
        method.sampleSize = methods.ColumnInt(4);
        method.enabled = methods.ColumnInt(5) != 0;
        method.archived = methods.ColumnInt(6) != 0;

        thresholds.Reset();
        thresholds.Int(1, method.id);

        int thresholdResult = SQLITE_OK;

        while ((thresholdResult = thresholds.Step()) == SQLITE_ROW)
        {
            Season season;

            if (Seasons::FromKey(thresholds.ColumnText(0), season))
            {
                MiteThreshold& level = method.thresholds[static_cast<int>(season)];
                level.watch = thresholds.ColumnOptionalDouble(1);
                level.act = thresholds.ColumnOptionalDouble(2);
            }
        }

        if (thresholdResult != SQLITE_DONE)
        {
            m_lastError = sqlite3_errmsg(db);
            return false;
        }

        data.miteMethods.push_back(method);
    }

    if (result != SQLITE_DONE)
    {
        m_lastError = sqlite3_errmsg(db);
        return false;
    }

    return true;
}

bool SettingsRepository::LoadTaskTypes(SettingsData& data)
{
    sqlite3* db = m_database.GetHandle();

    Statement statement(db,
        "SELECT id, task_key, custom_name, is_enabled, interval_value, interval_unit, "
        "task_trigger, is_archived "
        "FROM maintenance_task_types ORDER BY sort_order, id;");

    if (!statement.Ok())
    {
        m_lastError = sqlite3_errmsg(db);
        return false;
    }

    int result = SQLITE_OK;

    while ((result = statement.Step()) == SQLITE_ROW)
    {
        MaintenanceTaskType task;
        task.id = statement.ColumnInt(0);
        task.key = statement.ColumnText(1);
        task.customName = statement.ColumnText(2);
        task.enabled = statement.ColumnInt(3) != 0;
        task.intervalValue = statement.ColumnInt(4);
        task.intervalUnit = IntervalUnitFromKey(statement.ColumnText(5));
        task.trigger = statement.ColumnText(6) == "after_treatment"
            ? TaskTrigger::AfterTreatment
            : TaskTrigger::Recurring;
        task.archived = statement.ColumnInt(7) != 0;

        data.taskTypes.push_back(task);
    }

    if (result != SQLITE_DONE)
    {
        m_lastError = sqlite3_errmsg(db);
        return false;
    }

    return true;
}

// --------------------------------------------------
// Save
// --------------------------------------------------

bool SettingsRepository::Save(SettingsData& data)
{
    if (!m_database.GetHandle())
    {
        m_lastError = "Database is not open.";
        return false;
    }

    // Work on a copy so ids are only assigned if the
    // whole transaction commits.
    SettingsData working = data;

    if (!m_database.Execute("BEGIN IMMEDIATE;"))
    {
        m_lastError = m_database.GetLastError();
        return false;
    }

    const bool ok =
        SaveAppSettings(working.app)
        && SaveMiteMethods(working.miteMethods)
        && SaveTaskTypes(working.taskTypes);

    if (!ok)
    {
        m_database.Execute("ROLLBACK;");
        return false;
    }

    if (!m_database.Execute("COMMIT;"))
    {
        m_lastError = m_database.GetLastError();
        m_database.Execute("ROLLBACK;");
        return false;
    }

    data = std::move(working);
    m_lastError.clear();
    return true;
}

bool SettingsRepository::SaveAppSettings(const AppSettings& app)
{
    sqlite3* db = m_database.GetHandle();

    Statement upsert(db,
        "INSERT INTO app_settings (key, value, updated_at) "
        "VALUES (?, ?, CURRENT_TIMESTAMP) "
        "ON CONFLICT(key) DO UPDATE SET "
        "value = excluded.value, updated_at = CURRENT_TIMESTAMP "
        "WHERE value <> excluded.value;");

    if (!upsert.Ok())
    {
        m_lastError = sqlite3_errmsg(db);
        return false;
    }

    std::vector<std::pair<std::string, std::string>> values =
    {
        { Keys::Locale, app.locale },
        { Keys::DatePattern, app.datePattern },
        { Keys::TemperatureUnit, app.temperatureUnit == TemperatureUnit::Celsius ? "C" : "F" },
        { Keys::Hemisphere, app.hemisphere == Hemisphere::Southern ? "southern" : "northern" },
        { Keys::OffSeasonEnabled, app.offSeasonPauseEnabled ? "1" : "0" },
        { Keys::OffSeasonStart, Seasons::ToText(app.offSeasonStart) },
        { Keys::OffSeasonEnd, Seasons::ToText(app.offSeasonEnd) },
        { Keys::MiteTracking, app.miteTrackingEnabled ? "1" : "0" },
        { Keys::SetupStatus, ToKey(app.setupStatus) },
    };

    for (int s = 0; s < SeasonCount; ++s)
    {
        values.emplace_back(
            std::string(Keys::SeasonStartPrefix) + Seasons::Key(static_cast<Season>(s)),
            Seasons::ToText(app.seasonStarts[s])
        );
    }

    for (const auto& [key, value] : values)
    {
        upsert.Reset();
        upsert.Text(1, key);
        upsert.Text(2, value);

        if (upsert.Step() != SQLITE_DONE)
        {
            m_lastError = sqlite3_errmsg(db);
            return false;
        }
    }

    return true;
}

bool SettingsRepository::SaveMiteMethods(std::vector<MiteCheckMethod>& methods)
{
    sqlite3* db = m_database.GetHandle();

    Statement insert(db,
        "INSERT INTO mite_check_methods "
        "(method_key, custom_name, unit, sample_size, is_enabled, is_archived, sort_order) "
        "VALUES (?, ?, ?, ?, ?, ?, ?);");

    Statement update(db,
        "UPDATE mite_check_methods SET "
        "custom_name = ?, unit = ?, sample_size = ?, is_enabled = ?, is_archived = ?, "
        "sort_order = ?, updated_at = CURRENT_TIMESTAMP "
        "WHERE id = ?;");

    Statement clearThresholds(db, "DELETE FROM mite_method_thresholds WHERE method_id = ?;");

    Statement insertThreshold(db,
        "INSERT INTO mite_method_thresholds (method_id, season, watch_level, act_level) "
        "VALUES (?, ?, ?, ?);");

    if (!insert.Ok() || !update.Ok() || !clearThresholds.Ok() || !insertThreshold.Ok())
    {
        m_lastError = sqlite3_errmsg(db);
        return false;
    }

    int sortOrder = 0;

    for (MiteCheckMethod& method : methods)
    {
        // A custom method added and removed before saving.
        if (method.id == 0 && method.archived)
            continue;

        if (method.id == 0)
        {
            insert.Reset();
            insert.OptionalText(1, method.key);
            insert.OptionalText(2, method.customName);
            insert.Text(3, ToKey(method.unit));
            insert.Int(4, method.sampleSize);
            insert.Int(5, method.enabled ? 1 : 0);
            insert.Int(6, method.archived ? 1 : 0);
            insert.Int(7, sortOrder);

            if (insert.Step() != SQLITE_DONE)
            {
                m_lastError = sqlite3_errmsg(db);
                return false;
            }

            method.id = static_cast<int>(sqlite3_last_insert_rowid(db));
        }
        else
        {
            update.Reset();
            update.OptionalText(1, method.customName);
            update.Text(2, ToKey(method.unit));
            update.Int(3, method.sampleSize);
            update.Int(4, method.enabled ? 1 : 0);
            update.Int(5, method.archived ? 1 : 0);
            update.Int(6, sortOrder);
            update.Int(7, method.id);

            if (update.Step() != SQLITE_DONE)
            {
                m_lastError = sqlite3_errmsg(db);
                return false;
            }
        }

        ++sortOrder;

        clearThresholds.Reset();
        clearThresholds.Int(1, method.id);

        if (clearThresholds.Step() != SQLITE_DONE)
        {
            m_lastError = sqlite3_errmsg(db);
            return false;
        }

        for (int s = 0; s < SeasonCount; ++s)
        {
            const MiteThreshold& level = method.thresholds[s];

            if (!level.watch && !level.act)
                continue;

            insertThreshold.Reset();
            insertThreshold.Int(1, method.id);
            insertThreshold.Text(2, Seasons::Key(static_cast<Season>(s)));
            insertThreshold.OptionalDouble(3, level.watch);
            insertThreshold.OptionalDouble(4, level.act);

            if (insertThreshold.Step() != SQLITE_DONE)
            {
                m_lastError = sqlite3_errmsg(db);
                return false;
            }
        }
    }

    // Drop unsaved-and-removed entries from the model.
    std::vector<MiteCheckMethod> kept;

    for (MiteCheckMethod& method : methods)
    {
        if (method.id != 0)
            kept.push_back(method);
    }

    methods = std::move(kept);
    return true;
}

bool SettingsRepository::SaveTaskTypes(std::vector<MaintenanceTaskType>& tasks)
{
    sqlite3* db = m_database.GetHandle();

    Statement insert(db,
        "INSERT INTO maintenance_task_types "
        "(task_key, custom_name, is_enabled, interval_value, interval_unit, task_trigger, "
        "is_archived, sort_order) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?);");

    Statement update(db,
        "UPDATE maintenance_task_types SET "
        "custom_name = ?, is_enabled = ?, interval_value = ?, interval_unit = ?, "
        "task_trigger = ?, is_archived = ?, sort_order = ?, "
        "updated_at = CURRENT_TIMESTAMP "
        "WHERE id = ?;");

    if (!insert.Ok() || !update.Ok())
    {
        m_lastError = sqlite3_errmsg(db);
        return false;
    }

    int sortOrder = 0;
    std::vector<MaintenanceTaskType> kept;

    for (MaintenanceTaskType& task : tasks)
    {
        if (task.id == 0 && task.archived)
            continue;

        Statement& statement = task.id == 0 ? insert : update;
        statement.Reset();

        int index = 1;

        if (task.id == 0)
            statement.OptionalText(index++, task.key);

        statement.OptionalText(index++, task.customName);
        statement.Int(index++, task.enabled ? 1 : 0);
        statement.Int(index++, task.intervalValue);
        statement.Text(index++, ToKey(task.intervalUnit));
        statement.Text(index++, ToKey(task.trigger));
        statement.Int(index++, task.archived ? 1 : 0);
        statement.Int(index++, sortOrder);

        if (task.id != 0)
            statement.Int(index++, task.id);

        if (statement.Step() != SQLITE_DONE)
        {
            m_lastError = sqlite3_errmsg(db);
            return false;
        }

        if (task.id == 0)
            task.id = static_cast<int>(sqlite3_last_insert_rowid(db));

        ++sortOrder;
        kept.push_back(task);
    }

    tasks = std::move(kept);
    return true;
}
