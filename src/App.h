
#pragma once

#include "database/DatabaseManager.h"
#include "models/Settings.h"
#include "repositories/ApiaryRepository.h"
#include "repositories/BoxRepository.h"
#include "repositories/HiveRepository.h"
#include "repositories/SettingsRepository.h"
#include "screens/ApiaryScreen.h"
#include "screens/HiveDetailScreen.h"
#include "screens/SettingsScreen.h"
#include "screens/SetupWizard.h"

#include <memory>
#include <string>

// ==================================================
// App
// --------------------------------------------------
// Application shell: owns the repositories and screens,
// draws the top navigation bar and routes between:
//   Setup (first run) | Apiaries (> Hive detail) | Settings
// ==================================================

class App
{
public:
    explicit App(DatabaseManager& database);

    // Loads settings and applies language / date format.
    bool Initialize(std::string& error);

    // Draws one frame filling the whole window.
    void Render();

private:
    enum class Page
    {
        Setup,
        Apiaries,
        HiveDetail,     // shown under the Apiaries tab
        Settings
    };

    DatabaseManager& m_database;

    ApiaryRepository m_apiaryRepository;
    HiveRepository m_hiveRepository;
    BoxRepository m_boxRepository;
    SettingsRepository m_settingsRepository;

    SettingsData m_settings;

    std::unique_ptr<ApiaryScreen> m_apiaryScreen;
    std::unique_ptr<HiveDetailScreen> m_hiveDetailScreen;
    SettingsScreen m_settingsScreen;
    SetupWizard m_setupWizard;

    Page m_page = Page::Apiaries;

    // Navigation away from Settings with unsaved changes.
    bool m_openLeaveConfirm = false;
    Page m_pendingPage = Page::Apiaries;

    void RequestPage(Page page);
    void GoTo(Page page);

    void DrawTopBar(float pagePaddingX);
    bool NavItem(const char* label, bool active, float height);
    void DrawPage();
    void DrawLeaveConfirmDialog();
};
