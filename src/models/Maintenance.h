
#pragma once

#include <string>

// ==================================================
// Maintenance schedule output
// ==================================================

enum class DueStatus
{
    Overdue,
    DueSoon,        // within MaintenanceSchedule::DueSoonDays (incl. today)
    Upcoming
};

struct DueItem
{
    int hiveId = 0;
    int taskTypeId = 0;
    std::string taskKey;        // built-in key or "" for custom
    std::string taskName;       // display name (custom tasks)

    std::string dueOn;          // ISO date
    DueStatus status = DueStatus::Upcoming;

    bool neverDone = false;     // no history yet ("due now")
    bool postponed = false;     // a postponement moved it
    bool offSeasonShifted = false;

    int count = 0;              // e.g. frames due for comb replacement
};
