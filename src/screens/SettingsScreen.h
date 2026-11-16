
#pragma once

#include "models/Settings.h"
#include "repositories/SettingsRepository.h"
#include "screens/SettingsEditors.h"

#include <string>

// ==================================================
// SettingsScreen
// --------------------------------------------------
// Section list on the left, the selected section's
// editor on the right, pinned Save / Discard footer.
// Changes are kept in a draft until saved.
// ==================================================

class SettingsScreen
{
public:
    explicit SettingsScreen(SettingsRepository& settingsRepository);

    // Call when the screen is entered.
    void Load(const SettingsData& current);

    // Draws into the current window. Returns true on the
    // frame the settings were saved.
    bool Render();

    bool HasUnsavedChanges() const;

    // Used by the app's "unsaved changes" prompt.
    bool Save();
    void Discard();

    const SettingsData& Saved() const { return m_saved; }

private:
    SettingsRepository& m_settingsRepository;

    SettingsData m_saved;
    SettingsData m_draft;

    SettingsEditors::Section m_section = SettingsEditors::Section::Region;

    std::string m_error;
    std::string m_notice;
    float m_footerHeight = 0.0f;

    bool CanSave() const;
    void DrawNavigation(const SettingsEditors::Validation& validation);
    bool DrawFooter();   // returns true when saved
};
