
#pragma once

#include "models/Settings.h"

// ==================================================
// Applies language, date format and temperature unit
// from AppSettings.
// ==================================================

namespace RegionalSettings
{
    // "auto" locale = Windows region. Returns false if
    // the locale files could not be loaded.
    bool Apply(const AppSettings& settings);
}
