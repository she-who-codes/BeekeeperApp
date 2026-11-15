
#include "HiveFormDialog.h"

#include "config/LayoutUI.h"
#include "config/Theme.h"
#include "localization/Localization.h"
#include "models/HiveType.h"
#include "ui/Display.h"
#include "ui/Widgets.h"

#include "imgui.h"
#include "imgui_stdlib.h"

namespace
{
    constexpr const char* PopupId = "###HiveFormDialog";

    std::string Trim(const std::string& text)
    {
        const size_t first = text.find_first_not_of(" \t\r\n");

        if (first == std::string::npos)
            return "";

        const size_t last = text.find_last_not_of(" \t\r\n");
        return text.substr(first, last - first + 1);
    }
}

HiveFormDialog::HiveFormDialog(HiveRepository& hiveRepository)
    : m_hiveRepository(hiveRepository)
{
}

// --------------------------------------------------
// Opening
// --------------------------------------------------

void HiveFormDialog::OpenForCreate(int apiaryId)
{
    m_isEdit = false;
    m_original = Hive();
    m_original.apiaryId = apiaryId;

    m_apiaryId = apiaryId;
    m_name.clear();
    m_typeIndex = -1;
    m_typeOther.clear();
    m_setupDateText.clear();
    m_colonyDateText.clear();
    m_notes.clear();

    ClearErrors();
    m_requestOpen = true;
}

void HiveFormDialog::OpenForEdit(const Hive& hive)
{
    m_isEdit = true;
    m_original = hive;

    m_apiaryId = hive.apiaryId;
    m_name = hive.name;
    m_typeIndex = HiveTypes::IndexOf(hive.hiveType);
    m_typeOther = hive.hiveTypeOther;
    m_setupDateText = Display::Date(hive.setupDate);
    m_colonyDateText = Display::Date(hive.colonyEstablishedDate);
    m_notes = hive.notes;

    ClearErrors();
    m_requestOpen = true;
}

void HiveFormDialog::ClearErrors()
{
    m_nameError.clear();
    m_apiaryError.clear();
    m_typeError.clear();
    m_setupDateError.clear();
    m_colonyDateError.clear();
    m_saveError.clear();
}

// --------------------------------------------------
// Validation & saving
// --------------------------------------------------

bool HiveFormDialog::Validate(Hive& out)
{
    ClearErrors();

    out = m_original;

    // Name
    out.name = Trim(m_name);

    if (out.name.empty())
        m_nameError = Tr("hive.form.name_required");

    // Apiary
    out.apiaryId = m_apiaryId;

    if (out.apiaryId == 0)
        m_apiaryError = Tr("hive.form.apiary_required");

    // Type
    if (m_typeIndex < 0 || m_typeIndex >= HiveTypes::Count)
    {
        m_typeError = Tr("hive.form.type_required");
    }
    else
    {
        out.hiveType = HiveTypes::All[m_typeIndex].key;
        out.hiveTypeOther.clear();

        if (HiveTypes::IsOther(out.hiveType))
        {
            out.hiveTypeOther = Trim(m_typeOther);

            if (out.hiveTypeOther.empty())
                m_typeError = Tr("hive.form.type_other_required");
        }
    }

    // Dates are entered in the locale format and stored
    // as ISO. Neither may be in the future.
    //  - Hive set-up date: required.
    //  - Colony established date: optional (the hive may
    //    not have a colony yet). No ordering rule against
    //    the set-up date: a colony can be established
    //    elsewhere first (e.g. a nuc moved into this hive).
    Display::ParseDateInput(
        m_setupDateText,
        true,
        Display::FutureDates::Rejected,
        out.setupDate,
        m_setupDateError
    );

    Display::ParseDateInput(
        m_colonyDateText,
        false,
        Display::FutureDates::Rejected,
        out.colonyEstablishedDate,
        m_colonyDateError
    );

    // Notes are optional.
    out.notes = m_notes;

    return m_nameError.empty()
        && m_apiaryError.empty()
        && m_typeError.empty()
        && m_setupDateError.empty()
        && m_colonyDateError.empty();
}

HiveFormDialog::Result HiveFormDialog::Save()
{
    Hive hive;

    if (!Validate(hive))
        return Result::None;

    const bool ok = m_isEdit
        ? m_hiveRepository.Update(hive)
        : m_hiveRepository.Create(hive);

    if (!ok)
    {
        m_saveError = Localization::Format(
            "errors.save_failed",
            { { "details", m_hiveRepository.GetLastError() } }
        );
        return Result::None;
    }

    m_saved = hive;
    return m_isEdit ? Result::Updated : Result::Created;
}

// --------------------------------------------------
// Drawing
// --------------------------------------------------

HiveFormDialog::Result HiveFormDialog::Draw(const std::vector<Apiary>& apiaries)
{
    if (m_requestOpen)
    {
        ImGui::OpenPopup(PopupId);
        m_requestOpen = false;
    }

    UI::SetNextDialogLayout(UI::Dialog::WidthEm);

    const std::string title = Localization::WithId(
        Tr(m_isEdit ? "hive.form.title_edit" : "hive.form.title_add"),
        "HiveFormDialog"
    );

    if (!ImGui::BeginPopupModal(
        title.c_str(),
        nullptr,
        ImGuiWindowFlags_NoSavedSettings))
    {
        return Result::None;
    }

    Result result = Result::None;

    // Scrollable fields, pinned footer.
    UI::BeginDialogBody("HiveFormBody");
    DrawFields(apiaries);
    UI::EndDialogBody();

    ImGui::Separator();
    ImGui::Spacing();

    // Save error (database failure), kept next to the
    // buttons so it is always visible.
    UI::FieldError(m_saveError);

    // Footer
    const std::string saveLabel = Localization::Label(
        m_isEdit ? "hive.form.save_edit" : "hive.form.save_add",
        "Save"
    );
    const std::string cancelLabel = Localization::Label("common.cancel", "Cancel");

    if (UI::Button(saveLabel.c_str(), UI::ButtonKind::Primary))
    {
        result = Save();

        if (result != Result::None)
            ImGui::CloseCurrentPopup();
        else
            m_scrollToError = true;
    }

    UI::SameLineOrWrap(UI::ButtonWidth(cancelLabel.c_str()));

    const bool escape =
        ImGui::IsKeyPressed(ImGuiKey_Escape, false) && !ImGui::IsAnyItemActive();

    if (UI::Button(cancelLabel.c_str()) || escape)
    {
        ClearErrors();
        ImGui::CloseCurrentPopup();
    }

    // Required-field legend.
    {
        const std::string legend = std::string("* ") + Tr("common.required_legend");

        UI::SameLineOrWrap(ImGui::CalcTextSize(legend.c_str()).x);
        ImGui::AlignTextToFramePadding();
        UI::TextStyled(UI::Text::Caption, Theme::Colors::TextMuted, legend.c_str());
    }

    UI::EndDialogFooter();

    ImGui::EndPopup();
    return result;
}

void HiveFormDialog::ShowFieldError(const std::string& message)
{
    UI::FieldError(message);

    if (m_scrollToError && !message.empty())
    {
        ImGui::SetScrollHereY(0.5f);
        m_scrollToError = false;
    }
}

void HiveFormDialog::DrawFields(const std::vector<Apiary>& apiaries)
{
    // ---------------- Name ----------------
    UI::FormLabel(Tr("hive.form.name"), true);

    if (ImGui::IsWindowAppearing())
        ImGui::SetKeyboardFocusHere();

    ImGui::SetNextItemWidth(-FLT_MIN);

    if (ImGui::InputText("##HiveName", &m_name))
        m_nameError.clear();

    ShowFieldError(m_nameError);
    ImGui::Spacing();

    // ---------------- Apiary ----------------
    UI::FormLabel(Tr("hive.form.apiary"), true);

    const Apiary* selected = nullptr;

    for (const Apiary& apiary : apiaries)
    {
        if (apiary.id == m_apiaryId)
            selected = &apiary;
    }

    ImGui::SetNextItemWidth(-FLT_MIN);

    if (ImGui::BeginCombo(
        "##HiveApiary",
        selected ? selected->name.c_str() : Tr("hive.form.apiary_placeholder")))
    {
        for (const Apiary& apiary : apiaries)
        {
            // Archived apiaries are not offered, except
            // the one the hive already belongs to.
            if (!apiary.isActive && apiary.id != m_original.apiaryId)
                continue;

            ImGui::PushID(apiary.id);

            const bool isSelected = apiary.id == m_apiaryId;

            if (ImGui::Selectable(apiary.name.c_str(), isSelected))
            {
                m_apiaryId = apiary.id;
                m_apiaryError.clear();
            }

            if (isSelected)
                ImGui::SetItemDefaultFocus();

            ImGui::PopID();
        }

        ImGui::EndCombo();
    }

    ShowFieldError(m_apiaryError);
    ImGui::Spacing();

    // ---------------- Hive type ----------------
    UI::FormLabel(Tr("hive.form.type"), true);

    ImGui::SetNextItemWidth(-FLT_MIN);

    const char* typePreview = m_typeIndex >= 0
        ? Tr(HiveTypes::All[m_typeIndex].labelKey)
        : Tr("hive.form.type_placeholder");

    if (ImGui::BeginCombo("##HiveType", typePreview))
    {
        for (int i = 0; i < HiveTypes::Count; ++i)
        {
            ImGui::PushID(i);

            const bool isSelected = i == m_typeIndex;

            if (ImGui::Selectable(Tr(HiveTypes::All[i].labelKey), isSelected))
            {
                m_typeIndex = i;
                m_typeError.clear();
            }

            if (isSelected)
                ImGui::SetItemDefaultFocus();

            ImGui::PopID();
        }

        ImGui::EndCombo();
    }

    // "Other" requires a description.
    if (m_typeIndex >= 0 && HiveTypes::IsOther(HiveTypes::All[m_typeIndex].key))
    {
        ImGui::Spacing();
        UI::FormLabel(Tr("hive.form.type_other"), true);

        ImGui::SetNextItemWidth(-FLT_MIN);

        if (ImGui::InputTextWithHint(
            "##HiveTypeOther",
            Tr("hive.form.type_other_hint"),
            &m_typeOther))
        {
            m_typeError.clear();
        }
    }

    ShowFieldError(m_typeError);
    ImGui::Spacing();

    // ---------------- Dates ----------------
    const std::string todayLabel = Localization::Label("common.today", "Today");

    // Hive set-up date (required)
    UI::FormLabel(Tr("hive.form.setup_date"), true);
    UI::FieldHelp(Tr("hive.form.setup_date_help"));

    if (UI::DateInput(
        "##SetupDate",
        m_setupDateText,
        Display::DatePattern(),
        Display::DateHint(),
        todayLabel.c_str()))
    {
        m_setupDateError.clear();
    }

    ShowFieldError(m_setupDateError);
    ImGui::Spacing();

    // Colony established date (optional)
    UI::FormLabel(Tr("hive.form.colony_date"));
    UI::FieldHelp(Tr("hive.form.colony_date_help"));

    if (UI::DateInput(
        "##ColonyDate",
        m_colonyDateText,
        Display::DatePattern(),
        Display::DateHint(),
        todayLabel.c_str()))
    {
        m_colonyDateError.clear();
    }

    ShowFieldError(m_colonyDateError);
    ImGui::Spacing();

    // ---------------- Notes ----------------
    UI::FormLabel(Tr("hive.form.notes"));

    ImGui::InputTextMultiline(
        "##HiveNotes",
        &m_notes,
        ImVec2(-FLT_MIN, UI::Em(UI::Dialog::NotesHeightEm)),
        ImGuiInputTextFlags_WordWrap
    );

    // No error in the body (e.g. database failure only):
    // nothing to scroll to.
    m_scrollToError = false;
}
