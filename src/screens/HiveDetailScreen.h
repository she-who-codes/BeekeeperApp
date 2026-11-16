
#pragma once

#include "database/DatabaseManager.h"
#include "models/Apiary.h"
#include "models/Box.h"
#include "models/Hive.h"
#include "models/Settings.h"
#include "repositories/ApiaryRepository.h"
#include "repositories/BoxRepository.h"
#include "repositories/HiveRepository.h"
#include "screens/BoxDialog.h"
#include "screens/HiveFormDialog.h"

#include <string>
#include <vector>

// ==================================================
// HiveDetailScreen
// --------------------------------------------------
// Apiaries > Apiary > Hive. Tabs:
//   Overview        hive info and equipment summary
//   Boxes & Frames  the box stack (bottom box at the
//                   bottom) with frame tiles, and a
//                   details panel for the selected frame
// Archived hives are shown read-only.
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
    const SettingsData& m_settings;

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
