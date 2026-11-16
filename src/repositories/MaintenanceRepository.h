
#pragma once

#include "database/DatabaseManager.h"
#include "settings/MaintenanceSchedule.h"

#include <string>
#include <vector>

// ==================================================
// MaintenanceRepository
// --------------------------------------------------
// "Done" / "Postpone" records, and the per-hive history
// the MaintenanceSchedule rules work from.
// ==================================================

class MaintenanceRepository
{
public:
    explicit MaintenanceRepository(DatabaseManager& database);

    bool MarkDone(int hiveId, int taskTypeId, const std::string& date);

    // Replaces any earlier postponement of this task.
    bool Postpone(int hiveId, int taskTypeId, const std::string& today, const std::string& until);

    bool LoadHistory(int hiveId, MaintenanceSchedule::HiveHistory& history);

    // LoadHistory + MaintenanceSchedule::Compute.
    bool DueItems(int hiveId, const SettingsData& settings, const DateFormat::Date& today, std::vector<DueItem>& items);

    // Active hives (id, name, apiary id, apiary name).
    struct HiveRef
    {
        int hiveId = 0;
        std::string hiveName;
        int apiaryId = 0;
        std::string apiaryName;
    };

    bool ActiveHives(std::vector<HiveRef>& hives);

    std::string GetLastError() const;

private:
    DatabaseManager& m_database;
    std::string m_lastError;

    bool Fail();
};
