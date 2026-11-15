
#pragma once

#include <string>

// ==================================================
// DateFormat
// --------------------------------------------------
// Dates are STORED as ISO-8601 "YYYY-MM-DD" (sortable,
// locale independent) and SHOWN/ENTERED using the
// active locale's pattern, e.g.
//
//   en-US  "MM/DD/YYYY"     en-GB  "DD/MM/YYYY"
//   de     "DD.MM.YYYY"     en     "YYYY-MM-DD"
//
// Pattern tokens: YYYY (4-digit year), MM (month),
// DD (day). Any other character is a literal
// separator. When parsing, MM and DD accept 1 or 2
// digits ("3/7/2026" is fine).
// ==================================================

namespace DateFormat
{
    struct Date
    {
        int year = 0;
        int month = 0;
        int day = 0;
    };

    enum class ParseResult
    {
        Ok,
        Empty,
        BadFormat,      // does not match the pattern
        InvalidDate     // e.g. 02/30/2026
    };

    constexpr int MinimumYear = 1900;
    constexpr int MaximumYear = 9999;

    bool IsValid(const Date& date);

    // -1, 0, 1
    int Compare(const Date& a, const Date& b);

    // ISO storage format.
    bool ParseIso(const std::string& text, Date& out);
    std::string ToIso(const Date& date);

    // Locale display / entry format.
    ParseResult Parse(const std::string& text, const std::string& pattern, Date& out);
    std::string Format(const Date& date, const std::string& pattern);

    // ISO -> locale text. Returns "" if 'iso' is not a
    // valid ISO date (e.g. legacy empty values).
    std::string IsoToDisplay(const std::string& iso, const std::string& pattern);

    // Local calendar date of the machine.
    Date Today();

    // True for characters that may appear in text typed
    // for 'pattern' (digits + the pattern's separators).
    bool IsAllowedInput(unsigned int character, const std::string& pattern);
}
