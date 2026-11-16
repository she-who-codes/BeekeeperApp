
#pragma once

#include "database/DatabaseManager.h"
#include "models/Apiary.h"
#include "models/Box.h"
#include "models/Hive.h"
#include "models/Maintenance.h"
#include "models/Mites.h"
#include "models/Settings.h"
#include "repositories/ApiaryRepository.h"
#include "repositories/BoxRepository.h"
#include "repositories/HiveRepository.h"
#include "repositories/InspectionRepository.h"
#include "repositories/MaintenanceRepository.h"
#include "repositories/MiteRepository.h"
#include "screens/BoxDialog.h"
#include "screens/HiveFormDialog.h"
#include "screens/InspectionScreen.h"
#include "screens/MiteDialogs.h"

#include <string>
#include <vector>

// ==================================================
// HiveDetailScreen
// --------------------------------------------------
// Apiaries > Apiary > Hive. Tabs:
//   Overview        hive info, maintenance due (Done /
//                   Postpone), latest inspection and
//                   equipment summary
//   Boxes & Frames  the box stack (bottom box at the
//                   bottom) with frame tiles, and a
//                   details panel for the selected frame
//   Inspections     history; new / resume draft; opens
//                   the InspectionScreen
//   Mites           mite checks and treatments (only
//                   while mite tracking is on)
// Archived hives are shown read-only and have no
// maintenance schedule.
// ==================================================

class HiveDetailScreen
{
public:
    enum class Navigation
    {
        None,
        Apiaries,       // breadcrumb "Apiaries"
        Apiary          // breadcrumb apiary name (ApiaryId())
    };

    HiveDetailScreen(
        DatabaseManager& database,
        ApiaryRepository& apiaryRepository,
        HiveRepository& hiveRepository,
        BoxRepository& boxRepository,
        InspectionRepository& inspectionRepository,
        MiteRepository& miteRepository,
        MaintenanceRepository& maintenanceRepository,
        const SettingsData& settings
    );

    bool Open(int hiveId);

    // Draws into the current window.
    Navigation Render();

    int ApiaryId() const { return m_hive.apiaryId; }
    int HiveId() const { return m_hive.id; }

private:
    ApiaryRepository& m_apiaryRepository;
    HiveRepository& m_hiveRepository;
    BoxRepository& m_boxRepository;
    InspectionRepository& m_inspectionRepository;
    MiteRepository& m_miteRepository;
    MaintenanceRepository& m_maintenanceRepository;
    const SettingsData& m_settings;

    // Maintenance and mites
    std::vector<DueItem> m_dueItems;            // soonest first
    std::vector<MiteCheck> m_miteChecks;        // newest first
    std::vector<Treatment> m_treatments;        // newest first
    float m_mitesTablesWidth = 0.0f;            // natural width of the mite tables

    // Inspections
    std::vector<InspectionSummary> m_inspections;     // newest first
    InspectionSummary m_latestInspection;
    bool m_hasLatestInspection = false;
    int m_draftId = 0;

    InspectionScreen m_inspectionScreen;
    bool m_inInspection = false;
    bool m_selectInspectionsTab = false;
    bool m_selectOverviewTab = false;       // opening a hive starts on Overview

    Hive m_hive;
    Apiary m_apiary;
    std::vector<Apiary> m_apiaries;
    std::vector<Box> m_stack;       // bottom first

    std::string m_error;
    std::string m_notice;

    // Selected frame and its editable draft.
    int m_selectedFrameId = 0;
    int m_draftTypeIndex = 0;
    std::string m_draftLabel;
    std::string m_draftDateText;
    std::string m_draftNotes;
    std::string m_draftDateError;

    // Switching frames with unsaved changes.
    int m_pendingFrameId = 0;
    bool m_openDiscardConfirm = false;

    // Dialogs
    BoxDialog m_boxDialog;
    HiveFormDialog m_hiveForm;
    MiteCheckDialog m_miteDialog;
    TreatmentDialog m_treatmentDialog;

    bool m_openRemoveBox = false;
    Box m_removeCandidate;
    std::string m_removeDateText;
    std::string m_removeDateError;

    bool m_openReplaceFrame = false;
    int m_replaceTypeIndex = 0;
    std::string m_replaceDateText;
    std::string m_replaceDateError;

    // Actions never reload in the middle of drawing (that
    // would invalidate Box / Frame references held by the
    // draw code); they request a reload that runs at the
    // end of Render().
    bool m_reloadRequested = false;
    void RequestReload() { m_reloadRequested = true; }

    // Data
    void Reload();
    const Frame* FindFrame(int frameId, const Box** box = nullptr) const;
    void SelectFrame(int frameId);
    bool IsFrameDirty() const;
    bool IsReplacementDue(const Frame& frame) const;
    std::string ReplacementIntervalText() const;
    bool IsReadOnly() const { return !m_hive.isActive; }

    // UI
    Navigation DrawHeader();
    void DrawBanners();
    void DrawOverview();
    void DrawMaintenanceCard();
    void DrawMitesTab();
    void DrawMiteChecksTable();
    void DrawTreatmentsTable();
    void DrawInspectionsTab();
    void OpenInspection(int inspectionId);
    void StartInspection();
    void DrawBoxesTab();
    void DrawStack();
    void DrawBox(const Box& box, bool isTop, bool isBottom, float rowWidth);
    void DrawFrameTile(const Frame& frame, float width, float height);
    void DrawFramePanel();

    // Dialogs (root level of this screen)
    void DrawRemoveBoxDialog();
    void DrawReplaceFrameDialog();
    void DrawDiscardFrameDialog();
};
