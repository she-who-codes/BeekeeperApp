
#include "Dashboard.h"

#include "config/LayoutUI.h"
#include "config/Theme.h"
#include "localization/DateFormat.h"
#include "localization/Localization.h"
#include "screens/MaintenanceUI.h"
#include "screens/MiteDialogs.h"
#include "settings/MaintenanceSchedule.h"
#include "ui/Display.h"
#include "ui/Widgets.h"

#include "imgui.h"

#include <algorithm>

Dashboard::Dashboard(MaintenanceRepository& maintenanceRepository, MiteRepository& miteRepository, const SettingsData& settings)
    : m_maintenanceRepository(maintenanceRepository),
      m_miteRepository(miteRepository),
      m_settings(settings)
{
}

int Dashboard::TakeOpenHiveRequest()
{
    const int hiveId = m_openHiveRequest;
    m_openHiveRequest = 0;
    return hiveId;
}

// --------------------------------------------------
// Data
// --------------------------------------------------

void Dashboard::Refresh()
{
    m_overdue.clear();
    m_dueSoon.clear();
    m_comingUp.clear();
    m_miteAlerts.clear();
    m_hiveCount = 0;

    std::vector<MaintenanceRepository::HiveRef> hives;

    if (!m_maintenanceRepository.ActiveHives(hives))
    {
        m_error = Localization::Format("errors.load_failed", { { "details", m_maintenanceRepository.GetLastError() } });
        return;
    }

    m_hiveCount = static_cast<int>(hives.size());

    const DateFormat::Date today = DateFormat::Today();
    const std::string horizon = DateFormat::ToIso(DateFormat::AddDays(today, ComingUpDays));

    for (const MaintenanceRepository::HiveRef& hive : hives)
    {
        // Stable ID: the visible text changes with the language.
        const std::string label = Localization::WithId(
            Localization::Format("dashboard.hive_label", { { "hive", hive.hiveName }, { "apiary", hive.apiaryName } }),
            "HiveLink");

        std::vector<DueItem> items;

        if (!m_maintenanceRepository.DueItems(hive.hiveId, m_settings, today, items))
        {
            m_error = Localization::Format("errors.load_failed", { { "details", m_maintenanceRepository.GetLastError() } });
            return;
        }

        for (const DueItem& item : items)
        {
            switch (item.status)
            {
                case DueStatus::Overdue:
                    m_overdue.push_back({ item, label });
                    break;

                case DueStatus::DueSoon:
                    m_dueSoon.push_back({ item, label });
                    break;

                case DueStatus::Upcoming:
                    if (item.dueOn <= horizon)
                        m_comingUp.push_back({ item, label });
                    break;
            }
        }

        MiteCheck latest;

        if (m_settings.app.miteTrackingEnabled && m_miteRepository.LatestCheck(hive.hiveId, latest))
        {
            const MiteAlert alert = MiteText::Alert(m_settings, latest);

            if (alert != MiteAlert::None)
                m_miteAlerts.push_back({ latest, alert, label });
        }
    }

    // Soonest first (stable: hives stay in apiary / name order).
    const auto byDate = [](const Entry& a, const Entry& b) { return a.item.dueOn < b.item.dueOn; };

    std::stable_sort(m_overdue.begin(), m_overdue.end(), byDate);
    std::stable_sort(m_dueSoon.begin(), m_dueSoon.end(), byDate);
    std::stable_sort(m_comingUp.begin(), m_comingUp.end(), byDate);

    // Act before Watch.
    std::stable_sort(m_miteAlerts.begin(), m_miteAlerts.end(),
        [](const MiteEntry& a, const MiteEntry& b) { return a.alert == MiteAlert::Act && b.alert != MiteAlert::Act; });
}

// --------------------------------------------------
// Frame
// --------------------------------------------------

void Dashboard::Render()
{
    using namespace Theme::Colors;

    ImGui::PushID("Dashboard");

    if (!m_error.empty() && UI::Banner("DashboardError", m_error, UI::BannerKind::Error, Tr("common.dismiss")))
        m_error.clear();

    UI::BeginScrollPane("DashboardPane", ImVec2(0.0f, 0.0f), UI::Em(UI::Maintenance::ContentMinEm), ImGuiChildFlags_None);

    const bool showMites = m_settings.app.miteTrackingEnabled;
    const float maxWidth = UI::Em(UI::Maintenance::ContentMaxWidthEm + (showMites ? UI::Maintenance::SideColumnEm : 0.0f));
    const float width = std::min(ImGui::GetContentRegionAvail().x, maxWidth);

    ImGui::PushStyleColor(ImGuiCol_ChildBg, UI::WithAlpha(Surface, 0.0f));
    ImGui::BeginChild("DashboardBody", ImVec2(width, 0.0f), ImGuiChildFlags_AutoResizeY);
    ImGui::PopStyleColor();

    // Title + "date · N active hives"
    UI::TextStyledWrapped(UI::Text::Display, TextPrimary, Tr("dashboard.title"));

    const std::string subtitle = Localization::Format(
        "dashboard.subtitle",
        {
            { "date", Display::Date(DateFormat::ToIso(DateFormat::Today())) },
            { "hives", Localization::FormatPlural("dashboard.active_hives", m_hiveCount) }
        });

    UI::TextStyledWrapped(UI::Text::Caption, TextSecondary, subtitle.c_str());

    // Off-season: explain why little or nothing is due.
    if (const auto until = MaintenanceSchedule::PausedUntil(DateFormat::Today(), m_settings.app))
    {
        const std::string paused = Localization::Format(
            "dashboard.off_season", { { "date", Display::Date(DateFormat::ToIso(*until)) } });

        UI::TextStyledWrapped(UI::Text::Body, Warning, paused.c_str());
    }

    ImGui::Spacing();

    if (m_hiveCount == 0)
    {
        ImGui::Spacing();
        UI::TextStyled(UI::Text::Heading, TextPrimary, Tr("dashboard.no_hives_title"));
        ImGui::Spacing();
        UI::TextStyledWrapped(UI::Text::Body, TextSecondary, Tr("dashboard.no_hives"));
    }
    else
    {
        DrawSummary();
        ImGui::Spacing();

        // Mite alerts beside the tasks when there is room.
        const bool twoColumns = showMites && ImGui::GetContentRegionAvail().x >= UI::Em(UI::Maintenance::TwoColumnMinEm);

        if (twoColumns)
        {
            const float gap = UI::Px(UI::Spacing::Gutter);

            ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(gap * 0.5f, 0.0f));

            if (ImGui::BeginTable("Columns", 2, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoPadOuterX))
            {
                ImGui::TableSetupColumn("Tasks", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Mites", ImGuiTableColumnFlags_WidthFixed, UI::Em(UI::Maintenance::SideColumnEm));

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                DrawTasks();

                ImGui::TableNextColumn();
                DrawMiteAlerts();

                ImGui::EndTable();
            }

            ImGui::PopStyleVar();
        }
        else
        {
            DrawTasks();

            if (showMites)
            {
                ImGui::Spacing();
                DrawMiteAlerts();
            }
        }
    }

    ImGui::EndChild();
    UI::EndScrollPane();

    ImGui::PopID();

    // Never reload while rows are being drawn.
    if (m_reloadRequested)
    {
        m_reloadRequested = false;
        Refresh();
    }
}

// "3 overdue" "2 due this week" "1 mite alert" / "All caught up"
void Dashboard::DrawSummary()
{
    using namespace Theme::Colors;

    std::vector<std::pair<std::string, ImVec4>> chips;

    if (!m_overdue.empty())
        chips.emplace_back(Localization::FormatPlural("maintenance.badge_overdue", static_cast<int>(m_overdue.size())), Danger);

    if (!m_dueSoon.empty())
        chips.emplace_back(Localization::FormatPlural("maintenance.badge_due_soon", static_cast<int>(m_dueSoon.size())), Warning);

    if (!m_miteAlerts.empty())
        chips.emplace_back(Localization::FormatPlural("dashboard.mite_alert_count", static_cast<int>(m_miteAlerts.size())), Danger);

    if (m_overdue.empty() && m_dueSoon.empty())
        chips.insert(chips.begin(), { Tr("dashboard.all_caught_up"), Success });

    for (size_t i = 0; i < chips.size(); ++i)
    {
        if (i > 0)
            UI::SameLineOrWrap(UI::StatusChipWidth(chips[i].first.c_str()));

        UI::StatusChip(chips[i].first.c_str(), chips[i].second);
    }
}

void Dashboard::DrawTasks()
{
    DrawSection("Overdue", "dashboard.section_overdue", "dashboard.none_overdue", m_overdue);
    ImGui::Spacing();
    DrawSection("DueSoon", "dashboard.section_due_soon", "dashboard.none_due_soon", m_dueSoon);
    ImGui::Spacing();
    DrawSection("ComingUp", "dashboard.section_coming_up", "dashboard.none_coming_up", m_comingUp,
        Localization::FormatPlural("dashboard.coming_up_help", ComingUpDays));
}

void Dashboard::DrawSection(
    const char* id,
    const char* titleKey,
    const char* emptyKey,
    const std::vector<Entry>& entries,
    const std::string& help)
{
    using namespace Theme::Colors;

    UI::BeginCard(id);

    const std::string title = Localization::Format(titleKey, { { "count", std::to_string(entries.size()) } });
    UI::TextStyledWrapped(UI::Text::Title, TextPrimary, title.c_str());

    if (!help.empty())
        UI::TextStyledWrapped(UI::Text::Caption, TextMuted, help.c_str());

    ImGui::Spacing();

    if (entries.empty())
        UI::TextStyledWrapped(UI::Text::Body, TextSecondary, Tr(emptyKey));

    for (size_t i = 0; i < entries.size(); ++i)
    {
        if (i > 0)
        {
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
        }

        ImGui::PushID(static_cast<int>(i));

        bool openHive = false;

        if (MaintenanceUI::DueRow(entries[i].item, m_maintenanceRepository, m_error, entries[i].hiveLabel.c_str(), &openHive))
            m_reloadRequested = true;

        if (openHive)
            m_openHiveRequest = entries[i].item.hiveId;

        ImGui::PopID();
    }

    UI::EndCard();
}

void Dashboard::DrawMiteAlerts()
{
    using namespace Theme::Colors;

    UI::BeginCard("MiteAlerts");
    UI::TextStyledWrapped(UI::Text::Title, TextPrimary, Tr("dashboard.section_mites"));
    UI::TextStyledWrapped(UI::Text::Caption, TextMuted, Tr("dashboard.mites_help"));
    ImGui::Spacing();

    if (m_miteAlerts.empty())
        UI::TextStyledWrapped(UI::Text::Body, TextSecondary, Tr("dashboard.none_mites"));

    for (size_t i = 0; i < m_miteAlerts.size(); ++i)
    {
        const MiteEntry& entry = m_miteAlerts[i];

        if (i > 0)
        {
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
        }

        ImGui::PushID(static_cast<int>(i));

        if (ImGui::TextLink(entry.hiveLabel.c_str()))
            m_openHiveRequest = entry.check.hiveId;

        MaintenanceUI::MiteAlertChip(entry.alert);
        ImGui::SameLine();

        const std::string result = MiteText::Result(m_settings, entry.check);
        const std::string detail = Localization::Format(
            "dashboard.mite_detail",
            {
                { "result", result },
                { "method", MiteText::MethodName(m_settings, entry.check.methodId) },
                { "date", Display::Date(entry.check.checkedOn) }
            });

        UI::TextStyledWrapped(UI::Text::Caption, TextSecondary, detail.c_str());

        ImGui::PopID();
    }

    UI::EndCard();
}
