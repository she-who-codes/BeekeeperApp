
#include "MiteDialogs.h"

#include "config/LayoutUI.h"
#include "config/Theme.h"
#include "localization/DateFormat.h"
#include "localization/Localization.h"
#include "screens/MaintenanceUI.h"
#include "screens/SettingsEditors.h"
#include "settings/MaintenanceSchedule.h"
#include "ui/Display.h"
#include "ui/Widgets.h"

#include "imgui.h"
#include "imgui_stdlib.h"

#include <cstdio>

namespace
{
    constexpr const char* MitePopupId = "###MiteCheckDialog";
    constexpr const char* TreatmentPopupId = "###TreatmentDialog";

    const MiteCheckMethod* FindMethod(const SettingsData& settings, int id)
    {
        for (const MiteCheckMethod& method : settings.miteMethods)
        {
            if (method.id == id)
                return &method;
        }

        return nullptr;
    }

    const char* SampleLabelKey(MiteUnit unit)
    {
        switch (unit)
        {
            case MiteUnit::PercentBees:  return "settings.mites.sample_bees";
            case MiteUnit::MitesPerDay:  return "settings.mites.sample_days";
            case MiteUnit::PercentCells: return "settings.mites.sample_cells";
        }

        return "settings.mites.sample_bees";
    }

    const char* CountLabelKey(MiteUnit unit)
    {
        return unit == MiteUnit::PercentCells ? "mites.count_cells" : "mites.count_mites";
    }

    std::string TodayText()
    {
        return DateFormat::Format(DateFormat::Today(), Display::DatePattern());
    }

    // Footer: Save (primary), Cancel, Delete (edit only).
    // Delete asks first, inline: the footer switches to
    // "Delete <thing>?" with Delete / Keep.
    enum class Footer { None, Save, Cancel, Delete };

    Footer DrawFooter(bool canDelete, bool& confirmingDelete, const char* confirmKey)
    {
        ImGui::Separator();
        ImGui::Spacing();

        Footer pressed = Footer::None;

        if (confirmingDelete)
        {
            UI::TextStyledWrapped(UI::Text::Body, Theme::Colors::Danger, Tr(confirmKey));
            ImGui::Spacing();

            const std::string deleteLabel = Localization::Label("common.delete", "ConfirmDelete");
            const std::string keepLabel = Localization::Label("common.keep", "Keep");

            if (UI::Button(deleteLabel.c_str(), UI::ButtonKind::Danger))
                pressed = Footer::Delete;

            UI::SameLineOrWrap(UI::ButtonWidth(keepLabel.c_str()));

            if (UI::Button(keepLabel.c_str()) || (ImGui::IsKeyPressed(ImGuiKey_Escape, false) && !ImGui::IsAnyItemActive()))
                confirmingDelete = false;

            return pressed;
        }

        const std::string saveLabel = Localization::Label("common.save", "Save");
        const std::string cancelLabel = Localization::Label("common.cancel", "Cancel");
        const std::string deleteLabel = Localization::Label("common.delete", "Delete");

        if (UI::Button(saveLabel.c_str(), UI::ButtonKind::Primary))
            pressed = Footer::Save;

        UI::SameLineOrWrap(UI::ButtonWidth(cancelLabel.c_str()));

        if (UI::Button(cancelLabel.c_str()) || (ImGui::IsKeyPressed(ImGuiKey_Escape, false) && !ImGui::IsAnyItemActive()))
            pressed = Footer::Cancel;

        if (canDelete)
        {
            UI::SameLineOrWrap(UI::ButtonWidth(deleteLabel.c_str()));

            if (UI::Button(deleteLabel.c_str(), UI::ButtonKind::Danger))
                confirmingDelete = true;
        }

        return pressed;
    }
}

// ==================================================
// Text helpers
// ==================================================

namespace MiteText
{
    std::string MethodName(const SettingsData& settings, int methodId)
    {
        const MiteCheckMethod* method = FindMethod(settings, methodId);
        return method ? SettingsEditors::MethodName(*method) : std::string();
    }

    std::string Result(const SettingsData& settings, const MiteCheck& check)
    {
        const MiteCheckMethod* method = FindMethod(settings, check.methodId);

        if (!method)
            return "";

        const auto value = MaintenanceSchedule::MiteResult(check, method->unit);

        if (!value)
            return "";

        char number[32];
        std::snprintf(number, sizeof(number), "%.1f", *value);

        return Localization::Format(
            method->unit == MiteUnit::MitesPerDay ? "mites.result_per_day" : "mites.result_percent",
            { { "value", number } });
    }

    MiteAlert Alert(const SettingsData& settings, const MiteCheck& check)
    {
        const MiteCheckMethod* method = FindMethod(settings, check.methodId);

        if (!method)
            return MiteAlert::None;

        const auto value = MaintenanceSchedule::MiteResult(check, method->unit);
        return value ? MaintenanceSchedule::AlertFor(*value, *method, check.checkedOn, settings.app) : MiteAlert::None;
    }
}

// ==================================================
// Mite check dialog
// ==================================================

MiteCheckDialog::MiteCheckDialog(MiteRepository& repository, const SettingsData& settings)
    : m_repository(repository),
      m_settings(settings)
{
}

const MiteCheckMethod* MiteCheckDialog::Method() const
{
    return FindMethod(m_settings, m_check.methodId);
}

void MiteCheckDialog::UseMethod(const MiteCheckMethod& method)
{
    m_check.methodId = method.id;
    m_check.sampleSize = method.sampleSize > 0 ? method.sampleSize : 1;
}

void MiteCheckDialog::OpenForAdd(int hiveId, int inspectionId, const std::string& dateIso)
{
    m_check = MiteCheck();
    m_check.hiveId = hiveId;
    m_check.inspectionId = inspectionId;

    // First enabled method.
    for (const MiteCheckMethod& method : m_settings.miteMethods)
    {
        if (method.enabled && !method.archived)
        {
            UseMethod(method);
            break;
        }
    }

    m_dateText = dateIso.empty() ? TodayText() : Display::Date(dateIso);
    m_dateError.clear();
    m_error.clear();
    m_confirmingDelete = false;
    m_requestOpen = true;
}

void MiteCheckDialog::OpenForEdit(const MiteCheck& check)
{
    m_check = check;
    m_dateText = Display::Date(check.checkedOn);
    m_dateError.clear();
    m_error.clear();
    m_confirmingDelete = false;
    m_requestOpen = true;
}

MiteCheckDialog::Result MiteCheckDialog::Draw()
{
    using namespace Theme::Colors;

    if (m_requestOpen)
    {
        ImGui::OpenPopup(MitePopupId);
        m_requestOpen = false;
    }

    UI::SetNextDialogLayout(UI::Dialog::WidthEm);

    const std::string title = Localization::Label(m_check.id == 0 ? "mites.dialog.title_add" : "mites.dialog.title_edit", "MiteCheckDialog");

    if (!ImGui::BeginPopupModal(title.c_str(), nullptr, ImGuiWindowFlags_NoSavedSettings))
        return Result::None;

    Result result = Result::None;

    UI::BeginDialogBody("MiteCheckBody");

    // Date
    UI::FormLabel(Tr("mites.date"), true);
    const std::string todayLabel = Localization::Label("common.today", "Today");

    if (UI::DateInput("##MiteDate", m_dateText, Display::DatePattern(), Display::DateHint().c_str(), todayLabel.c_str()))
        m_dateError.clear();

    UI::FieldError(m_dateError);
    ImGui::Spacing();

    // Method (enabled ones, plus the current one when editing)
    UI::FormLabel(Tr("mites.method"), true);
    const MiteCheckMethod* current = Method();

    ImGui::SetNextItemWidth(-FLT_MIN);

    if (ImGui::BeginCombo("##MiteMethod", current ? SettingsEditors::MethodName(*current).c_str() : Tr("mites.no_method")))
    {
        for (const MiteCheckMethod& method : m_settings.miteMethods)
        {
            if ((!method.enabled || method.archived) && method.id != m_check.methodId)
                continue;

            ImGui::PushID(method.id);

            if (ImGui::Selectable(SettingsEditors::MethodName(method).c_str(), method.id == m_check.methodId))
                UseMethod(method);

            ImGui::PopID();
        }

        ImGui::EndCombo();
    }

    ImGui::Spacing();

    if (const MiteCheckMethod* method = Method())
    {
        // Sample size and count, labelled for the method's unit.
        UI::FormLabel(Tr(SampleLabelKey(method->unit)), true);
        ImGui::SetNextItemWidth(UI::Em(UI::Controls::NumberFieldEm));
        UI::IntInput("##Sample", m_check.sampleSize, 1, 100000);
        ImGui::Spacing();

        UI::FormLabel(Tr(CountLabelKey(method->unit)), true);
        ImGui::SetNextItemWidth(UI::Em(UI::Controls::NumberFieldEm));
        UI::IntInput("##Count", m_check.miteCount, 0, 100000);
        ImGui::Spacing();

        // Live result with the season's Watch / Act level.
        MiteCheck preview = m_check;
        std::string iso;
        std::string ignored;

        if (Display::ParseDateInput(m_dateText, true, Display::FutureDates::Rejected, iso, ignored))
            preview.checkedOn = iso;

        const std::string value = MiteText::Result(m_settings, preview);

        if (!value.empty())
        {
            const std::string text = Localization::Format("mites.result_label", { { "value", value } });
            UI::TextStyled(UI::Text::Title, TextPrimary, text.c_str());
            ImGui::SameLine();
            MaintenanceUI::MiteAlertChip(MiteText::Alert(m_settings, preview));
        }
    }
    else
    {
        UI::TextStyledWrapped(UI::Text::Body, Warning, Tr("mites.no_method_help"));
    }

    ImGui::Spacing();
    UI::FormLabel(Tr("mites.notes"));
    ImGui::InputTextMultiline("##MiteNotes", &m_check.notes, ImVec2(-FLT_MIN, UI::Em(UI::Maintenance::NotesHeightEm)), ImGuiInputTextFlags_WordWrap);

    UI::EndDialogBody();

    UI::FieldError(m_error);

    switch (DrawFooter(m_check.id != 0, m_confirmingDelete, "mites.delete_confirm"))
    {
        case Footer::Save:
        {
            std::string iso;

            if (!Method())
            {
                m_error = Tr("mites.no_method_help");
            }
            else if (Display::ParseDateInput(m_dateText, true, Display::FutureDates::Rejected, iso, m_dateError))
            {
                m_check.checkedOn = iso;

                if (m_repository.SaveCheck(m_check))
                {
                    result = Result::Saved;
                    ImGui::CloseCurrentPopup();
                }
                else
                {
                    m_error = Localization::Format("errors.save_failed", { { "details", m_repository.GetLastError() } });
                }
            }

            break;
        }

        case Footer::Cancel:
            ImGui::CloseCurrentPopup();
            break;

        case Footer::Delete:
            if (m_repository.DeleteCheck(m_check.id))
            {
                result = Result::Deleted;
                ImGui::CloseCurrentPopup();
            }
            else
            {
                m_error = Localization::Format("errors.save_failed", { { "details", m_repository.GetLastError() } });
            }
            break;

        case Footer::None:
            break;
    }

    UI::EndDialogFooter();
    ImGui::EndPopup();
    return result;
}

// ==================================================
// Treatment dialog
// ==================================================

TreatmentDialog::TreatmentDialog(MiteRepository& repository)
    : m_repository(repository)
{
}

void TreatmentDialog::OpenForAdd(int hiveId)
{
    m_treatment = Treatment();
    m_treatment.hiveId = hiveId;
    m_dateText = TodayText();
    m_dateError.clear();
    m_productError.clear();
    m_error.clear();
    m_confirmingDelete = false;
    m_requestOpen = true;
}

void TreatmentDialog::OpenForEdit(const Treatment& treatment)
{
    m_treatment = treatment;
    m_dateText = Display::Date(treatment.startedOn);
    m_dateError.clear();
    m_productError.clear();
    m_error.clear();
    m_confirmingDelete = false;
    m_requestOpen = true;
}

TreatmentDialog::Result TreatmentDialog::Draw()
{
    if (m_requestOpen)
    {
        ImGui::OpenPopup(TreatmentPopupId);
        m_requestOpen = false;
    }

    UI::SetNextDialogLayout(UI::Dialog::WidthEm);

    const std::string title = Localization::Label(m_treatment.id == 0 ? "treatments.dialog.title_add" : "treatments.dialog.title_edit", "TreatmentDialog");

    if (!ImGui::BeginPopupModal(title.c_str(), nullptr, ImGuiWindowFlags_NoSavedSettings))
        return Result::None;

    Result result = Result::None;

    UI::BeginDialogBody("TreatmentBody");

    UI::FormLabel(Tr("treatments.started"), true);
    const std::string todayLabel = Localization::Label("common.today", "Today");

    if (UI::DateInput("##TreatmentDate", m_dateText, Display::DatePattern(), Display::DateHint().c_str(), todayLabel.c_str()))
        m_dateError.clear();

    UI::FieldError(m_dateError);
    ImGui::Spacing();

    UI::FormLabel(Tr("treatments.product"), true);
    ImGui::SetNextItemWidth(-FLT_MIN);

    if (ImGui::InputTextWithHint("##Product", Tr("treatments.product_hint"), &m_treatment.product))
        m_productError.clear();

    UI::FieldError(m_productError);
    UI::FieldHelp(Tr("treatments.follow_up_help"));
    ImGui::Spacing();

    UI::FormLabel(Tr("treatments.notes"));
    ImGui::InputTextMultiline("##TreatmentNotes", &m_treatment.notes, ImVec2(-FLT_MIN, UI::Em(UI::Maintenance::NotesHeightEm)), ImGuiInputTextFlags_WordWrap);

    UI::EndDialogBody();

    UI::FieldError(m_error);

    switch (DrawFooter(m_treatment.id != 0, m_confirmingDelete, "treatments.delete_confirm"))
    {
        case Footer::Save:
        {
            std::string iso;
            const bool dateOk = Display::ParseDateInput(m_dateText, true, Display::FutureDates::Rejected, iso, m_dateError);

            const size_t first = m_treatment.product.find_first_not_of(" \t");

            if (first == std::string::npos)
                m_productError = Tr("treatments.product_required");

            if (dateOk && m_productError.empty())
            {
                m_treatment.startedOn = iso;

                if (m_repository.SaveTreatment(m_treatment))
                {
                    result = Result::Saved;
                    ImGui::CloseCurrentPopup();
                }
                else
                {
                    m_error = Localization::Format("errors.save_failed", { { "details", m_repository.GetLastError() } });
                }
            }

            break;
        }

        case Footer::Cancel:
            ImGui::CloseCurrentPopup();
            break;

        case Footer::Delete:
            if (m_repository.DeleteTreatment(m_treatment.id))
            {
                result = Result::Deleted;
                ImGui::CloseCurrentPopup();
            }
            else
            {
                m_error = Localization::Format("errors.save_failed", { { "details", m_repository.GetLastError() } });
            }
            break;

        case Footer::None:
            break;
    }

    UI::EndDialogFooter();
    ImGui::EndPopup();
    return result;
}
