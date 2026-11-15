
#pragma once

#include "repositories/ApiaryRepository.h"
#include "repositories/HiveRepository.h"
#include "screens/HiveFormDialog.h"

#include <string>
#include <unordered_map>
#include <vector>

class ApiaryScreen
{
public:
    ApiaryScreen(
        ApiaryRepository& apiaryRepository,
        HiveRepository& hiveRepository
    );

    void Render();

private:
    // Repositories
    ApiaryRepository& m_apiaryRepository;
    HiveRepository& m_hiveRepository;

    // Data
    std::vector<Apiary> m_apiaries;
    std::vector<Hive> m_hives;          // active first, then archived

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
    const Apiary* FindApiary(int id) const;
    void SetArchived(const Hive& hive, bool archived);

    // UI sections
    void DrawHeader();
    void DrawBanners();
    void DrawApiaryList();
    void DrawHivePanel();
    void DrawHiveGrid();
    void DrawHiveCard(const Hive& hive, float rowHeight);

    // Dialogs (drawn at root-window level so OpenPopup
    // and BeginPopupModal share the same ID stack)
    void DrawApiaryFormDialog();
    void DrawHiveFormDialog();
    void DrawArchiveConfirmDialog();
};
