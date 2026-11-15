
#pragma once

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

    // Active locale's date pattern and its hint text.
    std::string DatePattern();
    const char* DateHint();

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
