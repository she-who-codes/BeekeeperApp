
#pragma once

#include <string>

// ==================================================
// TimeFormat
// --------------------------------------------------
// Times are STORED as "HH:MM" (24 h) and SHOWN in the
// locale style from "format.time_pattern":
//   "HH:MM"    -> 14:30
//   "h:MM a"   -> 2:30 PM   (AM/PM text from the locale)
// Parsing accepts both styles: "14:30", "2:30 pm", "2:30PM".
// ==================================================

namespace TimeFormat
{
    struct Time
    {
        int hour = 0;     // 0..23
        int minute = 0;   // 0..59
    };

    enum class ParseResult
    {
        Ok,
        Empty,
        Invalid
    };

    // 'am' / 'pm' are the locale's markers (English "AM"/"PM"
    // are always accepted too, case-insensitive).
    ParseResult Parse(const std::string& text, const std::string& am, const std::string& pm, Time& out);

    // pattern containing 'a' = 12-hour clock.
    std::string Format(const Time& time, const std::string& pattern, const std::string& am, const std::string& pm);

    std::string ToStorage(const Time& time);                        // "HH:MM"
    bool FromStorage(const std::string& text, Time& out);

    Time Now();
}
