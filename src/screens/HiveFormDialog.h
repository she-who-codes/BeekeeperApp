
#pragma once

#include "database/DatabaseManager.h"
#include "models/Apiary.h"
#include "models/Hive.h"
#include "models/HiveEquipment.h"
#include "repositories/BoxRepository.h"
#include "repositories/HiveRepository.h"

#include <string>
#include <vector>

// ==================================================
// HiveFormDialog
// --------------------------------------------------
// Modal used to add a new hive or edit an existing one.
// Owns its own form state and validation messages.
//
// Usage (from the screen, every frame, at the same
// ImGui ID-stack level - e.g. the root window):
//
//   m_hiveForm.OpenForCreate(apiaryId);   // on click
//   ...
//   if (m_hiveForm.Draw(m_apiaries) != Result::None)
//       reload...
// ==================================================

class HiveFormDialog
{
public:
    enum class Result
    {
        None,
        Created,
        Updated
    };

    HiveFormDialog(
        HiveRepository& hiveRepository,
        BoxRepository& boxRepository,
        DatabaseManager& database
    );

    void OpenForCreate(int apiaryId);
    void OpenForEdit(const Hive& hive);

    // 'apiaries' populates the apiary selector (active
    // apiaries plus the hive's current one).
    Result Draw(const std::vector<Apiary>& apiaries);

    // Valid after Draw() returned Created / Updated.
    const Hive& SavedHive() const { return m_saved; }

    // The hive as it was before editing (Updated only).
    const Hive& OriginalHive() const { return m_original; }

private:
    HiveRepository& m_hiveRepository;
    BoxRepository& m_boxRepository;
    DatabaseManager& m_database;

    // Starting setup (Add Hive only): an optional first box
    // created together with the hive.
    bool m_addStartingBox = true;
    std::string m_startForHiveType;     // hive type the defaults were made for
    std::vector<const HiveEquipment::BoxTypeInfo*> m_startBoxTypes;
    int m_startBoxIndex = 0;
    int m_startFrameCount = 0;
    int m_startFrameTypeIndex = 0;

    void ResetStartingSetup(const std::string& hiveType);
    void ApplyStartingBoxDefaults();
    void DrawStartingSetup();

    bool m_requestOpen = false;
    bool m_isEdit = false;

    Hive m_original;
    Hive m_saved;

    // Form fields
    int m_apiaryId = 0;
    std::string m_name;
    int m_typeIndex = -1;
    std::string m_typeOther;
    std::string m_setupDateText;     // locale format
    std::string m_colonyDateText;    // locale format
    std::string m_notes;

    // Validation messages
    std::string m_nameError;
    std::string m_apiaryError;
    std::string m_typeError;
    std::string m_setupDateError;
    std::string m_colonyDateError;
    std::string m_saveError;

    void ClearErrors();
    bool Validate(Hive& out);
    Result Save();

    // Set when a save fails validation: the body scrolls
    // to the first field with an error.
    bool m_scrollToError = false;

    void DrawFields(const std::vector<Apiary>& apiaries);

    // FieldError() + scroll-to-first-error support.
    void ShowFieldError(const std::string& message);
};
