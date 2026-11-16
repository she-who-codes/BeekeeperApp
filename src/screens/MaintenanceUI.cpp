
#include "MaintenanceUI.h"

#include "config/LayoutUI.h"
#include "config/Theme.h"
#include "localization/DateFormat.h"
#include "localization/Localization.h"
#include "settings/MaintenanceSchedule.h"
#include "settings/SettingsDefaults.h"
#include "ui/Display.h"
#include "ui/Widgets.h"

#include "imgui.h"

namespace
{
    struct PostponeOption
    {
        int value;
        IntervalUnit unit;
        const char* labelKey;
    };

    // Postpone counts from today.
    const PostponeOption PostponeOptions[] =
    {
        { 1, IntervalUnit::Days,   "maintenance.postpone_1_day"   },
        { 3, IntervalUnit::Days,   "maintenance.postpone_3_days"  },
        { 1, IntervalUnit::Weeks,  "maintenance.postpone_1_week"  },
        { 2, IntervalUnit::Weeks,  "maintenance.postpone_2_weeks" },
        { 1, IntervalUnit::Months, "maintenance.postpone_1_month" },
    };
}

namespace MaintenanceUI
{
    std::string TaskLabel(const DueItem& item)
    {
        std::string name;

        if (const auto* info = SettingsDefaults::FindTask(item.taskKey))
            name = Tr(info->labelKey);
        else
            name = item.taskName.empty() ? std::string(Tr("settings.maintenance.unnamed")) : item.taskName;

        // Frames past the interval (only while the task is due;
        // after "Done" the hive's clock restarts).
        if (item.taskKey == "comb_replacement" && item.count > 0 && item.status != DueStatus::Upcoming)
            name += " \xC2\xB7 " + Localization::FormatPlural("maintenance.frames_count", item.count);

        return name;
    }

    std::string DueText(const DueItem& item)
    {
        const std::string date = Display::Date(item.dueOn);

        if (item.status == DueStatus::Overdue)
            return Localization::Format("maintenance.overdue_since", { { "date", date } });

        if (item.dueOn == DateFormat::ToIso(DateFormat::Today()))
            return Tr("maintenance.due_today");

        return Localization::Format("maintenance.due_on", { { "date", date } });
    }

    void StatusChip(const DueItem& item)
    {
        using namespace Theme::Colors;

        switch (item.status)
        {
            case DueStatus::Overdue:  UI::StatusChip(Tr("maintenance.status_overdue"), Danger); break;
            case DueStatus::DueSoon:  UI::StatusChip(Tr("maintenance.status_due_soon"), Warning); break;
            case DueStatus::Upcoming: UI::StatusChip(Tr("maintenance.status_upcoming"), SageLight); break;
        }
    }

    bool Actions(const DueItem& item, MaintenanceRepository& repository, std::string& error)
    {
        bool changed = false;
        const std::string today = DateFormat::ToIso(DateFormat::Today());

        const std::string doneLabel = Localization::Label("maintenance.done", "Done");
        const std::string postponeLabel = Localization::Label("maintenance.postpone", "Postpone");

        if (UI::Button(doneLabel.c_str(), UI::ButtonKind::Primary, -1.0f))
        {
            if (repository.MarkDone(item.hiveId, item.taskTypeId, today))
                changed = true;
            else
                error = Localization::Format("errors.save_failed", { { "details", repository.GetLastError() } });
        }

        ImGui::SetItemTooltip("%s", Tr("maintenance.done_help"));

        UI::SameLineOrWrap(UI::ButtonWidth(postponeLabel.c_str()));

        if (UI::Button(postponeLabel.c_str(), UI::ButtonKind::Secondary, -1.0f))
            ImGui::OpenPopup("PostponeOptions");

        if (ImGui::BeginPopup("PostponeOptions"))
        {
            UI::TextStyled(UI::Text::Label, Theme::Colors::TextSecondary, Tr("maintenance.postpone_for"));
            ImGui::Separator();

            const float rowHeight = std::max(UI::Px(UI::Size::ButtonHeight), ImGui::GetFrameHeight());

            for (const PostponeOption& option : PostponeOptions)
            {
                if (ImGui::Selectable(Tr(option.labelKey), false, ImGuiSelectableFlags_None, ImVec2(UI::Em(UI::Maintenance::PostponeMenuEm), rowHeight)))
                {
                    const DateFormat::Date until = MaintenanceSchedule::AddInterval(DateFormat::Today(), option.value, option.unit);

                    if (repository.Postpone(item.hiveId, item.taskTypeId, today, DateFormat::ToIso(until)))
                        changed = true;
                    else
                        error = Localization::Format("errors.save_failed", { { "details", repository.GetLastError() } });
                }
            }

            ImGui::EndPopup();
        }

        return changed;
    }

    float ActionsWidth()
    {
        const std::string doneLabel = Localization::Label("maintenance.done", "Done");
        const std::string postponeLabel = Localization::Label("maintenance.postpone", "Postpone");

        return UI::ButtonWidth(doneLabel.c_str()) + ImGui::GetStyle().ItemSpacing.x + UI::ButtonWidth(postponeLabel.c_str());
    }

    bool DueRow(const DueItem& item, MaintenanceRepository& repository, std::string& error, const char* hiveLabel, bool* openHive)
    {
        using namespace Theme::Colors;

        const float actionsWidth = ActionsWidth();
        const float available = ImGui::GetContentRegionAvail().x;
        const float spacing = ImGui::GetStyle().ItemSpacing.x;

        // Actions to the right of the text when there is room,
        // otherwise underneath (long translations, narrow panes).
        const bool sideBySide = available - actionsWidth - spacing >= UI::Em(UI::Maintenance::TaskTextMinEm);
        const float textWidth = sideBySide ? available - actionsWidth - spacing : available;
        const float startX = ImGui::GetCursorPosX();

        ImGui::BeginGroup();
        ImGui::PushTextWrapPos(startX + textWidth);

        const std::string task = TaskLabel(item);
        UI::TextStyledWrapped(UI::Text::Label, TextPrimary, task.c_str());

        if (hiveLabel)
        {
            UI::ScopedFont font(UI::Text::Caption);

            if (ImGui::TextLink(hiveLabel) && openHive)
                *openHive = true;
        }

        StatusChip(item);
        ImGui::SameLine();

        const std::string due = DueText(item);
        const ImVec4 dueColor = item.status == DueStatus::Overdue ? Danger : TextSecondary;
        UI::TextStyledWrapped(UI::Text::Caption, dueColor, due.c_str());

        if (item.offSeasonShifted || item.postponed)
        {
            const char* key = item.postponed ? "maintenance.postponed_note" : "maintenance.off_season_note";
            UI::TextStyledWrapped(UI::Text::Caption, TextMuted, Tr(key));
        }

        ImGui::PopTextWrapPos();
        ImGui::EndGroup();

        if (sideBySide)
            ImGui::SameLine(startX + available - actionsWidth);
        else
            ImGui::Spacing();

        ImGui::BeginGroup();
        const bool changed = Actions(item, repository, error);
        ImGui::EndGroup();

        return changed;
    }

    void MiteAlertChip(MiteAlert alert)
    {
        if (alert == MiteAlert::Act)
            UI::StatusChip(Tr("mites.alert_act"), Theme::Colors::Danger);
        else if (alert == MiteAlert::Watch)
            UI::StatusChip(Tr("mites.alert_watch"), Theme::Colors::Warning);
    }
}
