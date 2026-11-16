
#pragma once

#include <string>
#include <vector>

// ==================================================
// InspectionOptions
// --------------------------------------------------
// SINGLE SOURCE for the choices recorded in an inspection.
// The database stores the keys; the UI shows Tr(labelKey).
// ==================================================

namespace InspectionOptions
{
    struct Option
    {
        const char* key;
        const char* labelKey;
    };

    const std::vector<Option>& Weather();
    const std::vector<Option>& Temperament();
    const std::vector<Option>& QueenStatus();
    const std::vector<Option>& Strength();
    const std::vector<Option>& Stores();
    const std::vector<Option>& Findings();      // pest / disease signs
    const std::vector<Option>& CombDrawn();

    // Translated label for a key in a list ("" if unknown).
    std::string Label(const std::vector<Option>& options, const std::string& key);

    // Index of a key in a list, -1 if empty / unknown.
    int IndexOf(const std::vector<Option>& options, const std::string& key);

    // Coverage choices (percent).
    constexpr int CoverageSteps[] = { 0, 25, 50, 75, 100 };
    constexpr int CoverageStepCount = 5;
}
