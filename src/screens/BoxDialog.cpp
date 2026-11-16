
#include "BoxDialog.h"

#include "config/LayoutUI.h"
#include "config/Theme.h"
#include "localization/DateFormat.h"
#include "localization/Localization.h"
#include "ui/Display.h"
#include "ui/Widgets.h"

#include "imgui.h"
#include "imgui_stdlib.h"

#include <algorithm>

namespace
{
    constexpr const char* PopupId = "###BoxDialog";

    int FrameTypeIndex(const std::string& key)
    {
        const auto& types = HiveEquipment::FrameTypes();

        for (size_t i = 0; i < types.size(); ++i)
        {
            if (key == types[i].key)
                return static_cast<int>(i);
        }

        return 0;
    }

    int RoleIndex(const std::string& key)
    {
        const auto& roles = HiveEquipment::Roles();

        for (size_t i = 0; i < roles.size(); ++i)
        {
            if (key == roles[i].key)
                return static_cast<int>(i);
        }

        return 0;
    }

    std::string TodayText()
    {
        return DateFormat::Format(DateFormat::Today(), Display::DatePattern());
    }

    bool FrameTypeCombo(const char* id, int& index)
    {
        const auto& types = HiveEquipment::FrameTypes();
        bool changed = false;

        ImGui::SetNextItemWidth(-FLT_MIN);

        if (ImGui::BeginCombo(id, Tr(types[index].labelKey)))
        {
            for (size_t i = 0; i < types.size(); ++i)
            {
                if (ImGui::Selectable(Tr(types[i].labelKey), static_cast<int>(i) == index))
                {
                    index = static_cast<int>(i);
                    changed = true;
                }
            }

            ImGui::EndCombo();
        }

        return changed;
    }
}

BoxDialog::BoxDialog(BoxRepository& boxRepository)
    : m_boxRepository(boxRepository)
{
}

const HiveEquipment::BoxTypeInfo* BoxDialog::SelectedType() const
{
    if (m_isEdit)
        return HiveEquipment::FindBoxType(m_original.boxType);

    return (m_typeIndex >= 0 && m_typeIndex < static_cast<int>(m_allowedTypes.size()))
        ? m_allowedTypes[m_typeIndex]
        : nullptr;
}

void BoxDialog::ApplyTypeDefaults()
{
    const auto* type = SelectedType();

    if (!type)
        return;

    m_frameCount = HiveEquipment::DefaultFrameCount(type->key, m_hiveType);
    m_frameTypeIndex = FrameTypeIndex(HiveEquipment::DefaultFrameType(type->key, m_hiveType));

    const std::string role = HiveEquipment::DefaultRole(type->key, m_stackHasExcluder);
    m_roleIndex = RoleIndex(role.empty() ? "brood" : role);
}

void BoxDialog::OpenForAdd(int hiveId, const std::string& hiveType, const std::vector<Box>& stack)
{
    m_isEdit = false;
    m_original = Box();
    m_original.hiveId = hiveId;
    m_hiveType = hiveType;

    m_stackHasExcluder = std::any_of(stack.begin(), stack.end(),
        [](const Box& box) { return box.boxType == "queen_excluder"; });

    m_allowedTypes = HiveEquipment::AllowedBoxTypes(hiveType);
    m_typeIndex = 0;

    // Stack already has frame boxes: suggest a second box of
    // the same kind as the top one (e.g. another medium).
    for (auto it = stack.rbegin(); it != stack.rend(); ++it)
    {
        const auto* info = HiveEquipment::FindBoxType(it->boxType);

        if (info && info->holdsFrames)
        {
            for (size_t i = 0; i < m_allowedTypes.size(); ++i)
            {
                if (m_allowedTypes[i]->key == it->boxType)
                    m_typeIndex = static_cast<int>(i);
            }

            break;
        }
    }

    ApplyTypeDefaults();

    m_dateText = TodayText();
    m_label.clear();
    m_notes.clear();
    m_dateError.clear();
    m_saveError.clear();
    m_requestOpen = true;
}

void BoxDialog::OpenForEdit(const Box& box, const std::string& hiveType)
{
    m_isEdit = true;
    m_original = box;
    m_hiveType = hiveType;

    m_roleIndex = RoleIndex(box.role.empty() ? "brood" : box.role);
    m_frameCount = box.frameCapacity;
    m_frameTypeIndex = FrameTypeIndex(
        box.frames.empty()
            ? HiveEquipment::DefaultFrameType(box.boxType, hiveType)
            : box.frames.back().frameType);

    m_dateText = Display::Date(box.installedAt);
    m_label = box.label;
    m_notes = box.notes;
    m_dateError.clear();
    m_saveError.clear();
    m_requestOpen = true;
}

BoxDialog::Result BoxDialog::Save()
{
    const auto* type = SelectedType();

    if (!type)
        return Result::None;

    std::string installedIso;

    if (!Display::ParseDateInput(m_dateText, true, Display::FutureDates::Rejected, installedIso, m_dateError))
        return Result::None;

    Box box = m_original;
    box.boxType = type->key;
    box.installedAt = installedIso;
    box.label = m_label;
    box.notes = m_notes;
    box.frameCapacity = type->holdsFrames ? std::clamp(m_frameCount, 1, HiveEquipment::MaxFramesPerBox) : 0;

    if (type->hasRole)
        box.role = HiveEquipment::Roles()[m_roleIndex].key;
    else
        box.role = HiveEquipment::DefaultRole(type->key, false);   // flow super -> honey super

    const std::string frameType = HiveEquipment::FrameTypes()[m_frameTypeIndex].key;

    const bool ok = m_isEdit
        ? m_boxRepository.UpdateBox(box, frameType, DateFormat::ToIso(DateFormat::Today()))
        : m_boxRepository.AddBox(box, frameType);

    if (!ok)
    {
        m_saveError = Localization::Format(
            "errors.save_failed",
            { { "details", m_boxRepository.GetLastError() } }
        );
        return Result::None;
    }

    return m_isEdit ? Result::Updated : Result::Added;
}

BoxDialog::Result BoxDialog::Draw()
{
    if (m_requestOpen)
    {
        ImGui::OpenPopup(PopupId);
        m_requestOpen = false;
    }

    UI::SetNextDialogLayout(UI::Dialog::WidthEm);

    const std::string title = Localization::WithId(
        m_isEdit ? Display::BoxTitle(m_original) : std::string(Tr("boxes.dialog.title_add")),
        "BoxDialog"
    );

    if (!ImGui::BeginPopupModal(title.c_str(), nullptr, ImGuiWindowFlags_NoSavedSettings))
        return Result::None;

    Result result = Result::None;

    UI::BeginDialogBody("BoxDialogBody");
    DrawFields();
    UI::EndDialogBody();

    ImGui::Separator();
    ImGui::Spacing();

    UI::FieldError(m_saveError);

    const std::string saveLabel = Localization::Label(m_isEdit ? "common.save" : "boxes.dialog.add", "Save");
    const std::string cancelLabel = Localization::Label("common.cancel", "Cancel");

    if (UI::Button(saveLabel.c_str(), UI::ButtonKind::Primary))
    {
        result = Save();

        if (result != Result::None)
            ImGui::CloseCurrentPopup();
    }

    UI::SameLineOrWrap(UI::ButtonWidth(cancelLabel.c_str()));

    const bool escape = ImGui::IsKeyPressed(ImGuiKey_Escape, false) && !ImGui::IsAnyItemActive();

    if (UI::Button(cancelLabel.c_str()) || escape)
        ImGui::CloseCurrentPopup();

    UI::EndDialogFooter();
    ImGui::EndPopup();

    return result;
}

void BoxDialog::DrawFields()
{
    using namespace Theme::Colors;

    const auto* type = SelectedType();

    // ---------------- Box type ----------------
    UI::FormLabel(Tr("boxes.dialog.type"), !m_isEdit);

    if (m_isEdit)
    {
        ImGui::TextUnformatted(Display::BoxType(m_original.boxType).c_str());
        UI::FieldHelp(Tr("boxes.dialog.type_fixed"));
    }
    else
    {
        ImGui::SetNextItemWidth(-FLT_MIN);

        if (ImGui::BeginCombo("##BoxType", type ? Tr(type->labelKey) : ""))
        {
            for (size_t i = 0; i < m_allowedTypes.size(); ++i)
            {
                if (ImGui::Selectable(Tr(m_allowedTypes[i]->labelKey), static_cast<int>(i) == m_typeIndex))
                {
                    m_typeIndex = static_cast<int>(i);
                    ApplyTypeDefaults();
                }
            }

            ImGui::EndCombo();
        }
    }

    ImGui::Spacing();

    if (!type)
        return;

    // ---------------- Role ----------------
    if (type->hasRole)
    {
        UI::FormLabel(Tr("boxes.dialog.role"));

        const auto& roles = HiveEquipment::Roles();
        const char* labels[] = { Tr(roles[0].labelKey), Tr(roles[1].labelKey) };

        UI::SegmentedControl("Role", labels, 2, m_roleIndex, ImGui::GetFrameHeight());
        ImGui::Spacing();
    }

    // ---------------- Frames ----------------
    if (type->holdsFrames)
    {
        UI::FormLabel(Tr("boxes.dialog.frame_count"), true);
        UI::FieldHelp(Tr("boxes.dialog.frame_count_help"));

        ImGui::SetNextItemWidth(UI::Em(UI::Controls::NumberFieldEm));
        UI::IntInput("##FrameCount", m_frameCount, 1, HiveEquipment::MaxFramesPerBox);

        if (m_isEdit && m_frameCount < m_original.frameCapacity)
        {
            const std::string warning = Localization::Format(
                "boxes.dialog.frames_retired",
                {
                    { "from", std::to_string(m_frameCount + 1) },
                    { "to", std::to_string(m_original.frameCapacity) }
                }
            );

            UI::TextStyledWrapped(UI::Text::Caption, Warning, warning.c_str());
        }

        ImGui::Spacing();

        // Type of the frames created now (all frames when
        // adding, only the extra ones when editing).
        if (!m_isEdit || m_frameCount > m_original.frameCapacity)
        {
            UI::FormLabel(Tr(m_isEdit ? "boxes.dialog.new_frame_type" : "boxes.dialog.frame_type"));
            FrameTypeCombo("##FrameType", m_frameTypeIndex);
            ImGui::Spacing();
        }
    }
    else
    {
        UI::FieldHelp(Tr("boxes.dialog.no_frames"));
        ImGui::Spacing();
    }

    // ---------------- Installed ----------------
    UI::FormLabel(Tr("boxes.dialog.installed"), true);

    const std::string todayLabel = Localization::Label("common.today", "Today");

    if (UI::DateInput("##Installed", m_dateText, Display::DatePattern(), Display::DateHint().c_str(), todayLabel.c_str()))
        m_dateError.clear();

    UI::FieldError(m_dateError);
    ImGui::Spacing();

    // ---------------- Label & notes ----------------
    UI::FormLabel(Tr("boxes.dialog.label"));
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputTextWithHint("##Label", Tr("boxes.dialog.label_hint"), &m_label);
    ImGui::Spacing();

    UI::FormLabel(Tr("boxes.dialog.notes"));
    ImGui::InputTextMultiline(
        "##Notes",
        &m_notes,
        ImVec2(-FLT_MIN, UI::Em(UI::Dialog::NotesHeightEm * 0.7f)),
        ImGuiInputTextFlags_WordWrap
    );
}
