
#pragma once

#include "models/Settings.h"
#include "repositories/SettingsRepository.h"

#include <string>

// ==================================================
// SetupWizard
// --------------------------------------------------
// First-run setup: Region -> Seasons -> Mite monitoring
// -> Maintenance -> Review. Every step is prefilled with
// the recommended defaults. "Use defaults and finish
// later" saves the current values with status Deferred;
// the keeper can finish in Settings at any time.
// ==================================================

class SetupWizard
{
public:
    enum class Result
    {
        None,
        Completed,
        Deferred
    };

    explicit SetupWizard(SettingsRepository& settingsRepository);

    void Begin(const SettingsData& initial);

    // Draws into the current window (full page).
    Result Render();

    const SettingsData& Saved() const { return m_data; }

private:
    static constexpr int StepCount = 5;   // 4 sections + review

    SettingsRepository& m_settingsRepository;

    SettingsData m_data;
    int m_step = 0;

    std::string m_message;      // validation / save error
    float m_footerHeight = 0.0f;
    bool m_scrollToTop = false;

    void DrawHeader();

    bool IsReviewStep() const { return m_step == StepCount - 1; }
    bool ValidateStep(int step);
    Result Save(SetupStatus status);
};
