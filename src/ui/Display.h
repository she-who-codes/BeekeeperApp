
#pragma once

#include "models/Box.h"
#include "models/Hive.h"

#include <string>

// ==================================================
// Display helpers: turn stored values (keys, ISO
// dates) into localized text. Shared by all screens.
// ==================================================

namespace Display
{
    // Translated hive type, the custom "other" text,
    // or a "not specified" message.
    std::string HiveType(const Hive& hive);

    // ISO "YYYY-MM-DD" -> active locale pattern.
    // Returns "" for empty / invalid values.
    std::string Date(const std::string& iso);

    // Equipment names (box type, role, frame type).
    std::string BoxType(const std::string& key);
    std::string BoxRole(const std::string& key);
    std::string FrameType(const std::string& key);
    std::string FrameTypeShort(const std::string& key);

    // "Box 3 · Medium · Honey super" (+ " · label").
    std::string BoxTitle(const Box& box);

    // Comb age, e.g. "2 years, 3 months" / "5 months" /
    // "Less than a month". Empty for invalid dates.
    std::string CombAge(const std::string& installedIso);

    // Short tile form: "2 yr" / "5 mo"; empty under a month.
    std::string CombAgeShort(const std::string& installedIso);

    // Active date pattern (Settings override or locale
    // default) and its localized placeholder text.
    std::string DatePattern();
    std::string DateHint();

    enum class FutureDates
    {
        Allowed,
        Rejected
    };

    // Validates a date typed in the active locale format.
    //  required : empty text is an error when true;
    //             otherwise empty is accepted (iso = "").
    //  future   : Rejected -> dates after today (local
    //             calendar) are an error.
    // On success returns true and sets 'iso'. On failure
    // returns false and sets a localized 'error'.
    bool ParseDateInput(
        const std::string& text,
        bool required,
        FutureDates future,
        std::string& iso,
        std::string& error
    );
}
