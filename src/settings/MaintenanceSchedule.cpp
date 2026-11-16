
#include "MaintenanceSchedule.h"

#include "settings/Seasons.h"

#include <algorithm>

namespace
{
    using DateFormat::Date;

    bool Parse(const std::string& iso, Date& out)
    {
        return DateFormat::ParseIso(iso, out);
    }

    // The off-season end date (month/day) in 'year', with
    // the day clamped to the month (Feb 29 -> Feb 28).
    Date EndInYear(int year, const MonthDay& end)
    {
        Date date { year, end.month, end.day };

        while (!DateFormat::IsValid(date) && date.day > 28)
            --date.day;

        return date;
    }

    // First off-season end date after 'date'.
    Date OffSeasonEndAfter(const Date& date, const AppSettings& settings)
    {
        Date end = EndInYear(date.year, settings.offSeasonEnd);

        if (DateFormat::Compare(end, date) <= 0)
            end = EndInYear(date.year + 1, settings.offSeasonEnd);

        return end;
    }
}

namespace MaintenanceSchedule
{
    Date AddInterval(const Date& date, int value, IntervalUnit unit)
    {
        switch (unit)
        {
            case IntervalUnit::Days:   return DateFormat::AddDays(date, value);
            case IntervalUnit::Weeks:  return DateFormat::AddDays(date, value * 7L);
            case IntervalUnit::Months: return DateFormat::AddMonths(date, value);
            case IntervalUnit::Years:  return DateFormat::AddMonths(date, value * 12);
        }

        return date;
    }

    std::optional<Date> PausedUntil(const Date& today, const AppSettings& settings)
    {
        if (!settings.offSeasonPauseEnabled || !Seasons::IsInOffSeason(today, settings))
            return std::nullopt;

        return OffSeasonEndAfter(today, settings);
    }

    std::vector<DueItem> Compute(const HiveHistory& history, const SettingsData& settings, const Date& today)
    {
        std::vector<DueItem> items;
        const AppSettings& app = settings.app;

        for (const MaintenanceTaskType& task : settings.taskTypes)
        {
            if (!task.enabled || task.archived)
                continue;

            const bool mite = task.key == "mite_check" || task.key == "treatment_follow_up";

            if (mite && !app.miteTrackingEnabled)
                continue;

            DueItem item;
            item.hiveId = history.hiveId;
            item.taskTypeId = task.id;
            item.taskKey = task.key;
            item.taskName = task.customName;

            // ---------------- Natural due date ----------------
            // 'basis' = date the interval counts from ("" = never).
            std::string basis;
            bool listed = true;
            std::optional<Date> due;

            const auto doneIt = history.lastDone.find(task.id);
            const std::string lastDone = doneIt != history.lastDone.end() ? doneIt->second : "";

            if (task.key == "inspection")
            {
                basis = std::max(history.lastInspection, lastDone);
            }
            else if (task.key == "mite_check")
            {
                basis = std::max(history.lastMiteCheck, lastDone);
            }
            else if (task.key == "treatment_follow_up")
            {
                // Only while no mite check was made after the treatment
                // started (a check on the same day is usually the one
                // that led to the treatment).
                if (history.latestTreatment.empty()
                    || history.lastMiteCheck > history.latestTreatment
                    || lastDone >= history.latestTreatment)
                    listed = false;
                else
                    basis = history.latestTreatment;
            }
            else if (task.key == "comb_replacement")
            {
                // Oldest comb frame drives the date.
                std::string oldest;
                int count = 0;

                for (const std::string& installed : history.combFrameInstallDates)
                {
                    Date d;

                    if (!Parse(installed, d))
                        continue;

                    if (oldest.empty() || installed < oldest)
                        oldest = installed;

                    if (DateFormat::Compare(AddInterval(d, task.intervalValue, task.intervalUnit), today) <= 0)
                        ++count;
                }

                if (oldest.empty())
                    listed = false;

                basis = std::max(oldest, lastDone);
                item.count = count;
            }
            else if (task.key == "requeening")
            {
                basis = std::max(history.colonyEstablished, lastDone);

                if (basis.empty())
                    listed = false;     // nothing to count from
            }
            else
            {
                basis = lastDone;       // feeding check, custom tasks
            }

            if (!listed)
                continue;

            Date from;

            if (!basis.empty() && Parse(basis, from))
                due = AddInterval(from, task.intervalValue, task.intervalUnit);
            else
            {
                due = today;            // never done: due now
                item.neverDone = true;
            }

            // ---------------- Postponement ----------------
            const auto postponed = history.postponements.find(task.id);

            if (postponed != history.postponements.end())
            {
                Date until;
                const bool doneSince = !basis.empty() && basis >= postponed->second.postponedOn;

                if (!doneSince && Parse(postponed->second.until, until) && DateFormat::Compare(until, *due) > 0)
                {
                    due = until;
                    item.postponed = true;
                }
            }

            // ---------------- Off-season ----------------
            if (app.offSeasonPauseEnabled)
            {
                if (Seasons::IsInOffSeason(*due, app))
                {
                    due = OffSeasonEndAfter(*due, app);
                    item.offSeasonShifted = true;
                }
                else if (Seasons::IsInOffSeason(today, app) && DateFormat::Compare(*due, today) < 0)
                {
                    // Overdue when the off-season started: paused too.
                    due = OffSeasonEndAfter(today, app);
                    item.offSeasonShifted = true;
                }
            }

            // ---------------- Status ----------------
            item.dueOn = DateFormat::ToIso(*due);

            const long days = DateFormat::DaysBetween(today, *due);

            item.status = days < 0 ? DueStatus::Overdue
                        : days <= DueSoonDays ? DueStatus::DueSoon
                        : DueStatus::Upcoming;

            items.push_back(item);
        }

        std::sort(items.begin(), items.end(),
            [](const DueItem& a, const DueItem& b) { return a.dueOn < b.dueOn; });

        return items;
    }

    std::optional<double> MiteResult(const MiteCheck& check, MiteUnit unit)
    {
        if (check.sampleSize <= 0)
            return std::nullopt;

        switch (unit)
        {
            case MiteUnit::PercentBees:
            case MiteUnit::PercentCells:
                return 100.0 * check.miteCount / check.sampleSize;

            case MiteUnit::MitesPerDay:
                return static_cast<double>(check.miteCount) / check.sampleSize;
        }

        return std::nullopt;
    }

    MiteAlert AlertFor(double result, const MiteCheckMethod& method, const std::string& checkedOnIso, const AppSettings& settings)
    {
        Date date;

        if (!Parse(checkedOnIso, date) || !Seasons::AreStartsValid(settings.seasonStarts))
            return MiteAlert::None;

        const Season season = Seasons::SeasonFor(date, settings.seasonStarts);
        const MiteThreshold& level = method.thresholds[static_cast<int>(season)];

        if (level.act && result >= *level.act)
            return MiteAlert::Act;

        if (level.watch && result >= *level.watch)
            return MiteAlert::Watch;

        return MiteAlert::None;
    }
}
