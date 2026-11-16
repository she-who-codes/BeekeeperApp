
#include "SettingsDefaults.h"

#include <string>

namespace
{
    using namespace SettingsDefaults;

    const MiteMethodInfo g_miteMethods[] =
    {
        { "alcohol_wash",    "mite_method.alcohol_wash",    MiteUnit::PercentBees  },
        { "sugar_roll",      "mite_method.sugar_roll",      MiteUnit::PercentBees  },
        { "co2_wash",        "mite_method.co2_wash",        MiteUnit::PercentBees  },
        { "sticky_board",    "mite_method.sticky_board",    MiteUnit::MitesPerDay  },
        { "drone_uncapping", "mite_method.drone_uncapping", MiteUnit::PercentCells },
    };

    const TaskInfo g_tasks[] =
    {
        { "inspection",          "task.inspection"          },
        { "mite_check",          "task.mite_check"          },
        { "treatment_follow_up", "task.treatment_follow_up" },
        { "feeding_check",       "task.feeding_check"       },
        { "comb_replacement",    "task.comb_replacement"    },
        { "requeening",          "task.requeening"          },
    };

    // Thresholds per season, in order Spring, Summer,
    // Fall, Winter. { watch, act }; a negative value
    // means "no alert" for that season.
    struct SeasonLevels
    {
        double watch;
        double act;
    };

    // Mites per 100 bees (alcohol wash, sugar roll, CO2).
    // Based on widely published guidance (e.g. Honey Bee
    // Health Coalition): ~1-2% early season and during
    // dormancy, ~2-3% at peak / decreasing population.
    const SeasonLevels PercentBeesLevels[SeasonCount] =
    {
        { 1.0, 2.0 },   // spring  (population increase)
        { 2.0, 3.0 },   // summer  (peak population)
        { 2.0, 3.0 },   // fall    (population decrease)
        { 1.0, 2.0 },   // winter  (dormant)
    };

    // Natural mite fall per day (sticky board).
    // APPROXIMATE - published figures vary widely by
    // region and season. Please verify.
    const SeasonLevels MitesPerDayLevels[SeasonCount] =
    {
        { 3.0, 6.0 },
        { 8.0, 16.0 },
        { 16.0, 33.0 },
        { 1.0, 2.0 },
    };

    // % of uncapped drone cells with mites.
    // APPROXIMATE - please verify. No winter value
    // (colonies normally have no drone brood).
    const SeasonLevels PercentCellsLevels[SeasonCount] =
    {
        { 5.0, 10.0 },
        { 5.0, 10.0 },
        { 5.0, 10.0 },
        { -1.0, -1.0 },
    };

    int DefaultSampleSize(MiteUnit unit)
    {
        switch (unit)
        {
            case MiteUnit::PercentBees:  return 300;  // bees (~1/2 cup)
            case MiteUnit::MitesPerDay:  return 3;    // days on board
            case MiteUnit::PercentCells: return 100;  // drone cells
        }

        return 0;
    }

    const SeasonLevels* LevelsFor(MiteUnit unit)
    {
        switch (unit)
        {
            case MiteUnit::PercentBees:  return PercentBeesLevels;
            case MiteUnit::MitesPerDay:  return MitesPerDayLevels;
            case MiteUnit::PercentCells: return PercentCellsLevels;
        }

        return PercentBeesLevels;
    }
}

namespace SettingsDefaults
{
    const MiteMethodInfo* MiteMethods(int& count)
    {
        count = static_cast<int>(sizeof(g_miteMethods) / sizeof(g_miteMethods[0]));
        return g_miteMethods;
    }

    const TaskInfo* Tasks(int& count)
    {
        count = static_cast<int>(sizeof(g_tasks) / sizeof(g_tasks[0]));
        return g_tasks;
    }

    const MiteMethodInfo* FindMiteMethod(const std::string& key)
    {
        for (const MiteMethodInfo& info : g_miteMethods)
        {
            if (key == info.key)
                return &info;
        }

        return nullptr;
    }

    const TaskInfo* FindTask(const std::string& key)
    {
        for (const TaskInfo& info : g_tasks)
        {
            if (key == info.key)
                return &info;
        }

        return nullptr;
    }

    std::array<MonthDay, SeasonCount> SeasonStarts(Hemisphere hemisphere)
    {
        if (hemisphere == Hemisphere::Southern)
            return { MonthDay { 9, 1 }, MonthDay { 12, 1 }, MonthDay { 3, 1 }, MonthDay { 6, 1 } };

        return { MonthDay { 3, 1 }, MonthDay { 6, 1 }, MonthDay { 9, 1 }, MonthDay { 12, 1 } };
    }

    void OffSeason(Hemisphere hemisphere, MonthDay& start, MonthDay& end)
    {
        const auto starts = SeasonStarts(hemisphere);

        start = starts[static_cast<int>(Season::Winter)];
        end = starts[static_cast<int>(Season::Spring)];
    }

    MiteCheckMethod CreateMiteMethod(const MiteMethodInfo& info)
    {
        MiteCheckMethod method;
        method.key = info.key;
        method.unit = info.unit;
        method.sampleSize = DefaultSampleSize(info.unit);

        // Unticked here; Create() ticks the first method for
        // new settings. Built-ins added to an existing
        // database by a later version stay unticked.
        method.enabled = false;

        const SeasonLevels* levels = LevelsFor(info.unit);

        for (int s = 0; s < SeasonCount; ++s)
        {
            if (levels[s].watch >= 0.0)
                method.thresholds[s].watch = levels[s].watch;

            if (levels[s].act >= 0.0)
                method.thresholds[s].act = levels[s].act;
        }

        return method;
    }

    MaintenanceTaskType CreateTask(const TaskInfo& info)
    {
        MaintenanceTaskType task;
        task.key = info.key;

        const std::string key = info.key;

        if (key == "inspection")
        {
            task.intervalValue = 1;
            task.intervalUnit = IntervalUnit::Months;
        }
        else if (key == "mite_check")
        {
            task.intervalValue = 1;
            task.intervalUnit = IntervalUnit::Months;
        }
        else if (key == "treatment_follow_up")
        {
            task.intervalValue = 2;
            task.intervalUnit = IntervalUnit::Weeks;
            task.trigger = TaskTrigger::AfterTreatment;
        }
        else if (key == "feeding_check")
        {
            task.enabled = false;
            task.intervalValue = 1;
            task.intervalUnit = IntervalUnit::Weeks;
        }
        else if (key == "comb_replacement")
        {
            task.intervalValue = 3;
            task.intervalUnit = IntervalUnit::Years;
        }
        else if (key == "requeening")
        {
            task.enabled = false;
            task.intervalValue = 2;
            task.intervalUnit = IntervalUnit::Years;
        }

        return task;
    }

    SettingsData Create(Hemisphere hemisphere)
    {
        SettingsData data;

        data.app.hemisphere = hemisphere;
        data.app.seasonStarts = SeasonStarts(hemisphere);
        OffSeason(hemisphere, data.app.offSeasonStart, data.app.offSeasonEnd);

        for (const MiteMethodInfo& info : g_miteMethods)
            data.miteMethods.push_back(CreateMiteMethod(info));

        // Mite tracking is on by default, so the first method
        // (alcohol wash) is selected to make that usable.
        if (!data.miteMethods.empty())
            data.miteMethods.front().enabled = true;

        for (const TaskInfo& info : g_tasks)
            data.taskTypes.push_back(CreateTask(info));

        return data;
    }
}
