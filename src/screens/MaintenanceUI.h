
#pragma once

#include "models/Maintenance.h"
#include "models/Mites.h"
#include "models/Settings.h"
#include "repositories/MaintenanceRepository.h"

#include <string>

// ==================================================
// MaintenanceUI
// --------------------------------------------------
// Shared pieces for due tasks (hive Overview, hive cards,
// Dashboard): status chip, task name and the Done /
// Postpone actions.
// ==================================================

namespace MaintenanceUI
{
    // "Hive inspection", custom task name, "... (3 frames)".
    std::string TaskLabel(const DueItem& item);

    // "Overdue · 06/01/2026" / "Due today" / "Due 06/22/2026".
    std::string DueText(const DueItem& item);

    void StatusChip(const DueItem& item);

    // Done + Postpone (with interval list). Returns true when
    // the schedule changed; 'error' is set on failure.
    // Must be called inside a unique ImGui ID scope.
    bool Actions(const DueItem& item, MaintenanceRepository& repository, std::string& error);

    // Width of Done + Postpone on one line.
    float ActionsWidth();

    // One due task: task name, optional hive link, status
    // chip and due date on the left; Done / Postpone on the
    // right (underneath when the pane is narrow).
    //  hiveLabel : Dashboard only ("Hive 1 · Home yard");
    //              'openHive' is set when it is clicked.
    // Returns true when the schedule changed.
    // Must be called inside a unique ImGui ID scope.
    bool DueRow(
        const DueItem& item,
        MaintenanceRepository& repository,
        std::string& error,
        const char* hiveLabel = nullptr,
        bool* openHive = nullptr
    );

    // Mite alert chip ("Mites: Act"). No-op for None.
    void MiteAlertChip(MiteAlert alert);
}
