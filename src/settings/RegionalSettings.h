
#pragma once

#include "models/Settings.h"

// ==================================================
// Applies language and date format from AppSettings
// to the Localization module.
// ==================================================

namespace RegionalSettings
{
    // "auto" locale = Windows region. Returns false if
    // the locale files could not be loaded.
    bool Apply(const AppSettings& settings);
}
