
#pragma once

#include "database/DatabaseManager.h"
#include "models/Mites.h"
#include "models/Settings.h"
#include "repositories/ApiaryRepository.h"
#include "repositories/BoxRepository.h"
#include "repositories/HiveRepository.h"
#include "repositories/MaintenanceRepository.h"
#include "repositories/MiteRepository.h"
#include "screens/HiveFormDialog.h"

#include <map>
#include <string>
#include <unordered_map>
#include <vector>

class ApiaryScreen
{
public:
    ApiaryScreen(
        ApiaryRepository& apiaryRepository,
        HiveRepository& hiveRepository,
        BoxRepository& boxRepository,
        MaintenanceRepository& maintenanceRepository,
        MiteRepository& miteRepository,
        const SettingsData& settings,
        DatabaseManager& database
    );

    // Draws into the current window (the App page area).
    void Render();

    // Hive the keeper asked to open this frame (0 = none).
    // Cleared when read.
    int TakeOpenHiveRequest();

    // Reload data, optionally selecting an apiary
    // (e.g. when returning from a hive's detail view).
    void Refresh(int selectApiaryId = 0);

private:
    // Repositories
    ApiaryRepository& m_apiaryRepository;
    HiveRepository& m_hiveRepository;
    BoxRepository& m_boxRepository;
    MaintenanceRepository& m_maintenanceRepository;
    MiteRepository& m_miteRepository;
    const SettingsData& m_settings;

    // Card badges (active hives): tasks overdue / due this
    // week and the latest mite check's alert.
    struct HiveStatus
    {
        int overdue = 0;
        int dueSoon = 0;
        MiteAlert mites = MiteAlert::None;
    };

    // Data
    std::vector<Apiary> m_apiaries;
    std::vector<Hive> m_hives;          // active first, then archived
    std::map<int, HiveEquipmentCounts> m_equipment;   // by hive id
    std::map<int, HiveStatus> m_status;               // by hive id

    int m_openHiveRequest = 0;

    int m_selectedApiaryId = 0;
    bool m_showArchived = false;

    // Page-level feedback (separate from form errors)
    std::string m_pageError;
    std::string m_pageNotice;

    // Apiary form
    bool m_openApiaryForm = false;
    std::string m_apiaryName;
    std::string m_apiaryLocation;
    std::string m_apiaryNotes;
    std::string m_apiaryFormError;

    // Hive form (add / edit)
    HiveFormDialog m_hiveForm;

    // Natural (content) height of each hive card from the
    // previous frame, used to equalise card heights per row.
    std::unordered_map<int, float> m_cardHeights;

    // Archive confirmation
    bool m_openArchiveConfirm = false;
    Hive m_archiveCandidate;

    // Data operations
    void LoadApiaries();
    void LoadHives();
    void LoadStatus();
    const Apiary* FindApiary(int id) const;
    void SetArchived(const Hive& hive, bool archived);

    // UI sections
    void DrawHeader();
    void DrawBanners();
    void DrawApiaryList();
    void DrawHivePanel();
    void DrawHiveGrid();
    void DrawHiveCard(const Hive& hive, float rowHeight);
    void DrawStatusBadges(int hiveId);

    // Dialogs (drawn at root-window level so OpenPopup
    // and BeginPopupModal share the same ID stack)
    void DrawApiaryFormDialog();
    void DrawHiveFormDialog();
    void DrawArchiveConfirmDialog();
};
