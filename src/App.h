
#pragma once

#include "database/DatabaseManager.h"
#include "models/Settings.h"
#include "repositories/ApiaryRepository.h"
#include "repositories/BoxRepository.h"
#include "repositories/HiveRepository.h"
#include "repositories/InspectionRepository.h"
#include "repositories/MaintenanceRepository.h"
#include "repositories/MiteRepository.h"
#include "repositories/SettingsRepository.h"
#include "screens/ApiaryScreen.h"
#include "screens/Dashboard.h"
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
//   Setup (first run) | Dashboard (start page) |
//   Apiaries (> Hive detail) | Settings
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
        Dashboard,
        Apiaries,
        HiveDetail,     // shown under the Apiaries tab
        Settings
    };

    DatabaseManager& m_database;

    ApiaryRepository m_apiaryRepository;
    HiveRepository m_hiveRepository;
    BoxRepository m_boxRepository;
    InspectionRepository m_inspectionRepository;
    MiteRepository m_miteRepository;
    MaintenanceRepository m_maintenanceRepository;
    SettingsRepository m_settingsRepository;

    SettingsData m_settings;

    std::unique_ptr<Dashboard> m_dashboard;
    std::unique_ptr<ApiaryScreen> m_apiaryScreen;
    std::unique_ptr<HiveDetailScreen> m_hiveDetailScreen;
    SettingsScreen m_settingsScreen;
    SetupWizard m_setupWizard;

    Page m_page = Page::Dashboard;

    // Navigation away from Settings with unsaved changes.
    bool m_openLeaveConfirm = false;
    Page m_pendingPage = Page::Dashboard;

    void RequestPage(Page page);
    void GoTo(Page page);
    void OpenHive(int hiveId);

    void DrawTopBar(float pagePaddingX);
    bool NavItem(const char* label, bool active, float height);
    void DrawPage();
    void DrawLeaveConfirmDialog();
};
