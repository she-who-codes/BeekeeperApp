
#pragma once

#include "models/Box.h"
#include "models/HiveEquipment.h"
#include "repositories/BoxRepository.h"

#include <string>
#include <vector>

// ==================================================
// BoxDialog
// --------------------------------------------------
// Modal to add a box on top of a hive's stack or edit an
// existing box (role, frame count, install date, label,
// notes). The box type is chosen when adding; to change
// it, remove the box and add another.
// ==================================================

class BoxDialog
{
public:
    enum class Result
    {
        None,
        Added,
        Updated
    };

    explicit BoxDialog(BoxRepository& boxRepository);

    void OpenForAdd(int hiveId, const std::string& hiveType, const std::vector<Box>& stack);
    void OpenForEdit(const Box& box, const std::string& hiveType);

    // Call every frame at the same ImGui ID-stack level.
    Result Draw();

private:
    BoxRepository& m_boxRepository;

    bool m_requestOpen = false;
    bool m_isEdit = false;

    Box m_original;
    std::string m_hiveType;
    bool m_stackHasExcluder = false;

    std::vector<const HiveEquipment::BoxTypeInfo*> m_allowedTypes;
    int m_typeIndex = 0;

    int m_roleIndex = 0;          // index into HiveEquipment::Roles()
    int m_frameCount = 0;
    int m_frameTypeIndex = 0;     // index into HiveEquipment::FrameTypes()

    std::string m_dateText;       // locale format
    std::string m_label;
    std::string m_notes;

    std::string m_dateError;
    std::string m_saveError;

    const HiveEquipment::BoxTypeInfo* SelectedType() const;
    void ApplyTypeDefaults();
    void DrawFields();
    Result Save();
};
