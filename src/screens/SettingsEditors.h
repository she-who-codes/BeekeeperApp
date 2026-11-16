
#pragma once

#include "models/Settings.h"

#include <string>
#include <vector>

// ==================================================
// SettingsEditors
// --------------------------------------------------
// The editor for each settings section. Shared by the
// first-run SetupWizard and the SettingsScreen so both
// always look and validate the same way.
// ==================================================

namespace SettingsEditors
{
    enum class Section
    {
        Region,
        Seasons,
        MiteMonitoring,
        Maintenance,
        Count
    };

    // Localized section title.
    const char* SectionTitle(Section section);

    struct Validation
    {
        std::string seasons;
        std::string offSeason;

        // Mite tracking is on but no method is selected.
        // Required to finish setup / save Settings, but not
        // to "Use defaults and finish later".
        std::string methodSelection;

        std::vector<std::string> methods;  // per miteMethods index ("" = ok)
        std::vector<std::string> tasks;    // per taskTypes index

        // allowIncomplete = true skips methodSelection.
        bool IsSectionValid(Section section, bool allowIncomplete = false) const;
        bool IsValid(bool allowIncomplete = false) const;
    };

    Validation Validate(const SettingsData& data);

    // Editors. Return true when something changed.
    bool DrawRegion(SettingsData& data);
    bool DrawSeasons(SettingsData& data, const Validation& validation);
    bool DrawMiteMonitoring(SettingsData& data, const Validation& validation);
    bool DrawMaintenance(SettingsData& data, const Validation& validation);

    bool DrawSection(Section section, SettingsData& data, const Validation& validation);

    // Read-only summary (wizard review step).
    void DrawReview(const SettingsData& data);

    // Display names
    std::string MethodName(const MiteCheckMethod& method);
    std::string TaskName(const MaintenanceTaskType& task);
    const char* SeasonName(Season season);
    std::string MonthDayText(const MonthDay& value);
}
