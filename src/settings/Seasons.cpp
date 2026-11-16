
#include "Seasons.h"

#include <cstdio>

namespace Seasons
{
    const char* Key(Season season)
    {
        switch (season)
        {
            case Season::Spring: return "spring";
            case Season::Summer: return "summer";
            case Season::Fall:   return "fall";
            case Season::Winter: return "winter";
        }

        return "spring";
    }

    bool FromKey(const std::string& key, Season& out)
    {
        for (int i = 0; i < SeasonCount; ++i)
        {
            if (key == Key(static_cast<Season>(i)))
            {
                out = static_cast<Season>(i);
                return true;
            }
        }

        return false;
    }

    int MaxDay(int month)
    {
        static const int days[] = { 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
        return (month >= 1 && month <= 12) ? days[month - 1] : 0;
    }

    bool IsValid(const MonthDay& value)
    {
        return value.month >= 1 && value.month <= 12
            && value.day >= 1 && value.day <= MaxDay(value.month);
    }

    std::string ToText(const MonthDay& value)
    {
        char buffer[8];
        std::snprintf(buffer, sizeof(buffer), "%02d-%02d", value.month, value.day);
        return buffer;
    }

    bool FromText(const std::string& text, MonthDay& out)
    {
        int month = 0;
        int day = 0;

        if (text.size() != 5 || text[2] != '-'
            || std::sscanf(text.c_str(), "%2d-%2d", &month, &day) != 2)
            return false;

        const MonthDay value { month, day };

        if (!IsValid(value))
            return false;

        out = value;
        return true;
    }

    int DayOfYear(const MonthDay& value)
    {
        int total = 0;

        for (int m = 1; m < value.month; ++m)
            total += MaxDay(m);

        return total + value.day - 1;
    }

    bool AreStartsValid(const std::array<MonthDay, SeasonCount>& starts)
    {
        for (const MonthDay& start : starts)
        {
            if (!IsValid(start))
                return false;
        }

        // Walking spring -> summer -> fall -> winter ->
        // spring must go round the year exactly once.
        int travelled = 0;

        for (int i = 0; i < SeasonCount; ++i)
        {
            const int from = DayOfYear(starts[i]);
            const int to = DayOfYear(starts[(i + 1) % SeasonCount]);

            const int gap = (to - from + 366) % 366;

            if (gap == 0)
                return false;   // two seasons start on the same day

            travelled += gap;
        }

        return travelled == 366;
    }

    namespace
    {
        MonthDay ToMonthDay(const DateFormat::Date& date)
        {
            return MonthDay { date.month, date.day };
        }
    }

    bool IsInRange(const DateFormat::Date& date, const MonthDay& start, const MonthDay& end)
    {
        const int d = DayOfYear(ToMonthDay(date));
        const int s = DayOfYear(start);
        const int e = DayOfYear(end);

        if (s == e)
            return false;

        return s < e
            ? (d >= s && d < e)
            : (d >= s || d < e);   // wraps over New Year
    }

    Season SeasonFor(
        const DateFormat::Date& date,
        const std::array<MonthDay, SeasonCount>& starts)
    {
        for (int i = 0; i < SeasonCount; ++i)
        {
            if (IsInRange(date, starts[i], starts[(i + 1) % SeasonCount]))
                return static_cast<Season>(i);
        }

        return Season::Spring;
    }

    bool IsInOffSeason(const DateFormat::Date& date, const AppSettings& settings)
    {
        return settings.offSeasonPauseEnabled
            && IsInRange(date, settings.offSeasonStart, settings.offSeasonEnd);
    }
}
