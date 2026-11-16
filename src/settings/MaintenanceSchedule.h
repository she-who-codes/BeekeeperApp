
#pragma once

#include "localization/DateFormat.h"
#include "models/Maintenance.h"
#include "models/Mites.h"
#include "models/Settings.h"

#include <map>
#include <optional>
#include <string>
#include <vector>

// ==================================================
// MaintenanceSchedule
// --------------------------------------------------
// Pure scheduling rules (no database, no UI) so every
// rule is testable on its own.
//
//  Inspection        last completed inspection + interval
//                    (never inspected: due now)
//  Mite check        last mite check + interval (never:
//                    due now). Only while tracking is on.
//  Treatment follow  latest treatment + interval, until a
//  -up               mite check after the treatment start
//                    (a same-day check doesn't count).
//  Comb replacement  oldest comb frame + interval.
//  Other / custom    last Done + interval (never: due now;
//                    requeening counts from the colony
//                    established date, none = not listed).
//
//  Done      a later Done restarts the interval.
//  Postpone  not due before 'until' - ignored once the
//            task has been done after it was postponed.
//  Off-season  all tasks: a due date inside the off-season
//            (or overdue during it) moves to its end.
// ==================================================

namespace MaintenanceSchedule
{
    constexpr int DueSoonDays = 7;

    struct Postponement
    {
        std::string postponedOn;    // ISO
        std::string until;          // ISO
    };

    // Everything known about one hive.
    struct HiveHistory
    {
        int hiveId = 0;

        std::string lastInspection;     // latest completed, ISO or ""
        std::string lastMiteCheck;      // ISO or ""
        std::string latestTreatment;    // ISO or ""
        std::string colonyEstablished;  // ISO or ""

        // Install dates of active comb frames (ISO).
        std::vector<std::string> combFrameInstallDates;

        // By task type id.
        std::map<int, std::string> lastDone;
        std::map<int, Postponement> postponements;
    };

    DateFormat::Date AddInterval(const DateFormat::Date& date, int value, IntervalUnit unit);

    // End of the current off-season when the pause is on and
    // 'today' is in it (tasks resume then), otherwise nullopt.
    std::optional<DateFormat::Date> PausedUntil(const DateFormat::Date& today, const AppSettings& settings);

    // Due items for one hive (enabled tasks only), soonest first.
    std::vector<DueItem> Compute(
        const HiveHistory& history,
        const SettingsData& settings,
        const DateFormat::Date& today
    );

    // Mite result in the method's unit: % of bees, mites
    // per day or % of cells. nullopt if it can't be computed.
    std::optional<double> MiteResult(const MiteCheck& check, MiteUnit unit);

    // Watch / Act for a result on a date (season by date).
    MiteAlert AlertFor(
        double result,
        const MiteCheckMethod& method,
        const std::string& checkedOnIso,
        const AppSettings& settings
    );
}
