
#pragma once

#include "models/Settings.h"
#include "localization/DateFormat.h"

#include <string>

// ==================================================
// Seasons
// --------------------------------------------------
// Season lookup and off-season checks for a calendar
// date, using the keeper's season start dates. All
// ranges may wrap over New Year (e.g. Dec 1 -> Mar 1).
// ==================================================

namespace Seasons
{
    // Stable keys used in the database ("spring"...).
    const char* Key(Season season);
    bool FromKey(const std::string& key, Season& out);

    // "MM-DD" <-> MonthDay. Parse rejects invalid days.
    std::string ToText(const MonthDay& value);
    bool FromText(const std::string& text, MonthDay& out);

    // Day must exist in at least a leap year (Feb 29 ok).
    bool IsValid(const MonthDay& value);
    int MaxDay(int month);

    // Days from Jan 1 in a leap year (0..365); used to
    // compare month/days independent of the year.
    int DayOfYear(const MonthDay& value);

    // True when the season starts are all valid, all
    // different and in calendar order spring -> summer ->
    // fall -> winter (wrapping over New Year is allowed).
    bool AreStartsValid(const std::array<MonthDay, SeasonCount>& starts);

    // Season containing 'date'. Requires valid starts.
    Season SeasonFor(
        const DateFormat::Date& date,
        const std::array<MonthDay, SeasonCount>& starts
    );

    // True if 'date' is in [start, end) - wraps over
    // New Year when end is earlier than start.
    bool IsInRange(const DateFormat::Date& date, const MonthDay& start, const MonthDay& end);

    bool IsInOffSeason(const DateFormat::Date& date, const AppSettings& settings);
}
