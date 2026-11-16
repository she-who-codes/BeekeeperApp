
#pragma once

#include "models/Maintenance.h"
#include "models/Mites.h"
#include "models/Settings.h"
#include "repositories/MaintenanceRepository.h"
#include "repositories/MiteRepository.h"

#include <string>
#include <vector>

// ==================================================
// Dashboard
// --------------------------------------------------
// Start page: maintenance due across all apiaries
// (active hives only), soonest first:
//   Overdue | Due this week | Coming up (next 30 days)
// each with Done / Postpone and a link to the hive, plus
// hives whose latest mite check is at Watch or Act.
// Side by side on wide windows, stacked when narrow.
// ==================================================

class Dashboard
{
public:
    // Days ahead shown under "Coming up".
    static constexpr int ComingUpDays = 30;

    Dashboard(MaintenanceRepository& maintenanceRepository, MiteRepository& miteRepository, const SettingsData& settings);

    // Reload (call when the page is shown).
    void Refresh();

    // Draws into the current window.
    void Render();

    // Hive the keeper asked to open (0 = none). Cleared when read.
    int TakeOpenHiveRequest();

private:
    struct Entry
    {
        DueItem item;
        std::string hiveLabel;      // "Hive 1 · Home yard###HiveLink"
    };

    struct MiteEntry
    {
        MiteCheck check;
        MiteAlert alert = MiteAlert::None;
        std::string hiveLabel;
    };

    MaintenanceRepository& m_maintenanceRepository;
    MiteRepository& m_miteRepository;
    const SettingsData& m_settings;

    std::vector<Entry> m_overdue;
    std::vector<Entry> m_dueSoon;
    std::vector<Entry> m_comingUp;
    std::vector<MiteEntry> m_miteAlerts;       // Act first

    int m_hiveCount = 0;
    int m_openHiveRequest = 0;

    std::string m_error;
    bool m_reloadRequested = false;

    void DrawSummary();
    void DrawTasks();
    void DrawSection(
        const char* id,
        const char* titleKey,          // "{count}" placeholder
        const char* emptyKey,
        const std::vector<Entry>& entries,
        const std::string& help = ""
    );
    void DrawMiteAlerts();
};
