
#include "App.h"

#include "config/LayoutUI.h"
#include "config/Theme.h"
#include "localization/Localization.h"
#include "settings/RegionalSettings.h"
#include "ui/Widgets.h"

#include "imgui.h"

#include <algorithm>
#include <cstring>

namespace
{
    constexpr const char* LeaveConfirmPopupId = "###LeaveSettingsDialog";

    // End of the visible part of an ImGui label ("Text##id").
    const char* VisibleEnd(const char* label)
    {
        const char* hidden = std::strstr(label, "##");
        return hidden ? hidden : label + std::strlen(label);
    }
}

App::App(DatabaseManager& database)
    : m_database(database),
      m_apiaryRepository(database),
      m_hiveRepository(database),
      m_boxRepository(database),
      m_inspectionRepository(database),
      m_miteRepository(database),
      m_maintenanceRepository(database),
      m_settingsRepository(database),
      m_settingsScreen(m_settingsRepository),
      m_setupWizard(m_settingsRepository)
{
}

bool App::Initialize(std::string& error)
{
    if (!m_settingsRepository.Load(m_settings))
    {
        error = m_settingsRepository.GetLastError();
        return false;
    }

    if (!RegionalSettings::Apply(m_settings.app))
        error = Localization::GetLastError();   // not fatal: keys are shown

    // Created after localization so their messages are
    // translated. They read m_settings by reference
    // (intervals, mite methods...), so they always see the
    // saved settings.
    m_dashboard = std::make_unique<Dashboard>(m_maintenanceRepository, m_miteRepository, m_settings);

    m_apiaryScreen = std::make_unique<ApiaryScreen>(
        m_apiaryRepository, m_hiveRepository, m_boxRepository,
        m_maintenanceRepository, m_miteRepository, m_settings, m_database);

    m_hiveDetailScreen = std::make_unique<HiveDetailScreen>(
        m_database, m_apiaryRepository, m_hiveRepository, m_boxRepository, m_inspectionRepository,
        m_miteRepository, m_maintenanceRepository, m_settings);

    if (m_settings.app.setupStatus == SetupStatus::NotStarted)
    {
        m_setupWizard.Begin(m_settings);
        m_page = Page::Setup;
    }
    else
    {
        m_dashboard->Refresh();
        m_page = Page::Dashboard;
    }

    return true;
}

// --------------------------------------------------
// Navigation
// --------------------------------------------------

void App::RequestPage(Page page)
{
    if (page == m_page)
        return;

    if (m_page == Page::Settings && m_settingsScreen.HasUnsavedChanges())
    {
        m_pendingPage = page;
        m_openLeaveConfirm = true;
        return;
    }

    GoTo(page);
}

void App::GoTo(Page page)
{
    if (page == Page::Settings)
        m_settingsScreen.Load(m_settings);

    // Pick up changes made elsewhere (box counts, moved
    // hives, tasks done, settings...).
    if (page == Page::Dashboard)
        m_dashboard->Refresh();

    if (page == Page::Apiaries)
        m_apiaryScreen->Refresh(m_page == Page::HiveDetail ? m_hiveDetailScreen->ApiaryId() : 0);

    m_page = page;
}

void App::OpenHive(int hiveId)
{
    if (hiveId != 0 && m_hiveDetailScreen->Open(hiveId))
        m_page = Page::HiveDetail;
}

// --------------------------------------------------
// Frame
// --------------------------------------------------

void App::Render()
{
    const ImGuiIO& io = ImGui::GetIO();

    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(io.DisplaySize);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

    ImGui::Begin(
        "###BeeKeeperRoot",
        nullptr,
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse
    );

    ImGui::PopStyleVar();

    // Page side padding scales with window width.
    const float pagePaddingX = std::clamp(
        io.DisplaySize.x * UI::Spacing::PageRatio,
        UI::Px(UI::Spacing::PageMin),
        UI::Px(UI::Spacing::PageMax)
    );

    DrawTopBar(pagePaddingX);

    // ---------------- Page ----------------
    ImGui::SetCursorPos(ImVec2(0.0f, UI::Px(UI::TopBar::Height)));

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(pagePaddingX, UI::Px(UI::Spacing::PageTop)));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, UI::WithAlpha(Theme::Colors::Background, 0.0f));

    ImGui::BeginChild(
        "Page",
        ImVec2(0.0f, 0.0f),
        ImGuiChildFlags_AlwaysUseWindowPadding,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse
    );

    ImGui::PopStyleColor();
    ImGui::PopStyleVar();

    DrawPage();

    ImGui::EndChild();

    DrawLeaveConfirmDialog();

    ImGui::End();
}

void App::DrawPage()
{
    switch (m_page)
    {
        case Page::Setup:
        {
            const SetupWizard::Result result = m_setupWizard.Render();

            if (result != SetupWizard::Result::None)
            {
                m_settings = m_setupWizard.Saved();
                GoTo(Page::Dashboard);
            }

            break;
        }

        case Page::Dashboard:
            m_dashboard->Render();
            OpenHive(m_dashboard->TakeOpenHiveRequest());
            break;

        case Page::Apiaries:
            m_apiaryScreen->Render();
            OpenHive(m_apiaryScreen->TakeOpenHiveRequest());
            break;

        case Page::HiveDetail:
        {
            const HiveDetailScreen::Navigation navigation = m_hiveDetailScreen->Render();

            if (navigation != HiveDetailScreen::Navigation::None)
            {
                // Back to the hive's apiary (it may have been
                // moved while editing the hive).
                m_apiaryScreen->Refresh(m_hiveDetailScreen->ApiaryId());
                m_page = Page::Apiaries;
            }

            break;
        }

        case Page::Settings:
            if (m_settingsScreen.Render())
                m_settings = m_settingsScreen.Saved();
            break;
    }
}

// --------------------------------------------------
// Top bar
// --------------------------------------------------

bool App::NavItem(const char* label, bool active, float height)
{
    using namespace Theme::Colors;

    const float padding = UI::Em(UI::TopBar::NavItemPadEm);
    const ImVec2 textSize = ImGui::CalcTextSize(label, nullptr, true);
    const ImVec2 size(textSize.x + padding * 2.0f, height);
    const ImVec2 position = ImGui::GetCursorScreenPos();

    const bool pressed = ImGui::InvisibleButton(label, size);
    const bool hovered = ImGui::IsItemHovered();

    ImDrawList* drawList = ImGui::GetWindowDrawList();

    if (hovered && !active)
    {
        drawList->AddRectFilled(position, ImVec2(position.x + size.x, position.y + size.y),
            ImGui::GetColorU32(ForestLight));
    }

    drawList->AddText(
        ImVec2(position.x + padding, position.y + (height - textSize.y) * 0.5f),
        ImGui::GetColorU32(active ? TextPrimary : TextSecondary),
        label,
        VisibleEnd(label)
    );

    if (active)
    {
        const float indicator = UI::Px(UI::TopBar::IndicatorPx);
        drawList->AddRectFilled(
            ImVec2(position.x + padding * 0.5f, position.y + height - indicator),
            ImVec2(position.x + size.x - padding * 0.5f, position.y + height),
            ImGui::GetColorU32(Honey)
        );
    }

    return pressed;
}

void App::DrawTopBar(float pagePaddingX)
{
    using namespace Theme::Colors;

    const float height = UI::Px(UI::TopBar::Height);

    ImGui::PushStyleColor(ImGuiCol_ChildBg, Forest);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(pagePaddingX, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.0f);

    ImGui::BeginChild(
        "TopBar",
        ImVec2(0.0f, height),
        ImGuiChildFlags_AlwaysUseWindowPadding,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse
    );

    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor();

    // Brand
    {
        UI::ScopedFont font(UI::Text::Title);
        ImGui::SetCursorPosY((height - ImGui::GetTextLineHeight()) * 0.5f);
        ImGui::TextColored(Honey, "%s", Tr("app.name"));
    }

    if (m_page != Page::Setup)
    {
        ImGui::SameLine(0.0f, UI::Em(2.0f));
        ImGui::SetCursorPosY(0.0f);

        if (NavItem(Localization::Label("navigation.dashboard", "NavDashboard").c_str(), m_page == Page::Dashboard, height))
            RequestPage(Page::Dashboard);

        ImGui::SameLine(0.0f, 0.0f);
        ImGui::SetCursorPosY(0.0f);

        const bool onApiaries = m_page == Page::Apiaries || m_page == Page::HiveDetail;

        if (NavItem(Localization::Label("navigation.apiaries", "NavApiaries").c_str(), onApiaries, height))
            RequestPage(Page::Apiaries);

        ImGui::SameLine(0.0f, 0.0f);
        ImGui::SetCursorPosY(0.0f);

        if (NavItem(Localization::Label("navigation.settings", "NavSettings").c_str(), m_page == Page::Settings, height))
            RequestPage(Page::Settings);

        // Reminder while setup was deferred.
        if (m_settings.app.setupStatus == SetupStatus::Deferred && m_page != Page::Settings)
        {
            const std::string finishLabel = Localization::Label("setup.finish_reminder", "FinishSetup");
            const float width = UI::ButtonWidth(finishLabel.c_str());

            ImGui::SameLine();

            const float free = ImGui::GetContentRegionAvail().x - width;

            if (free > 0.0f)
            {
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + free);
                ImGui::SetCursorPosY((height - ImGui::GetFrameHeight()) * 0.5f);

                ImGui::PushStyleColor(ImGuiCol_Button, UI::Mix(Forest, Warning, 0.35f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, UI::Mix(Forest, Warning, 0.55f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, Warning);

                if (ImGui::Button(finishLabel.c_str(), ImVec2(width, ImGui::GetFrameHeight())))
                    RequestPage(Page::Settings);

                ImGui::PopStyleColor(3);
            }
        }
    }

    ImGui::EndChild();
}

// --------------------------------------------------
// Unsaved changes prompt
// --------------------------------------------------

void App::DrawLeaveConfirmDialog()
{
    if (m_openLeaveConfirm)
    {
        ImGui::OpenPopup(LeaveConfirmPopupId);
        m_openLeaveConfirm = false;
    }

    UI::SetNextDialogLayout(UI::Dialog::ConfirmWidthEm);

    const std::string title = Localization::Label("settings.leave_dialog.title", "LeaveSettingsDialog");

    if (!ImGui::BeginPopupModal(title.c_str(), nullptr, ImGuiWindowFlags_NoSavedSettings))
        return;

    UI::TextStyledWrapped(UI::Text::Body, Theme::Colors::TextPrimary, Tr("settings.leave_dialog.body"));

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    const std::string saveLabel = Localization::Label("settings.leave_dialog.save", "Save");
    const std::string discardLabel = Localization::Label("settings.leave_dialog.discard", "Discard");
    const std::string stayLabel = Localization::Label("settings.leave_dialog.stay", "Stay");

    if (UI::Button(saveLabel.c_str(), UI::ButtonKind::Primary))
    {
        // Stay on Settings if the draft cannot be saved
        // (the screen shows what needs fixing).
        if (m_settingsScreen.Save())
        {
            m_settings = m_settingsScreen.Saved();
            GoTo(m_pendingPage);
        }

        ImGui::CloseCurrentPopup();
    }

    UI::SameLineOrWrap(UI::ButtonWidth(discardLabel.c_str()));

    if (UI::Button(discardLabel.c_str(), UI::ButtonKind::Danger))
    {
        m_settingsScreen.Discard();
        GoTo(m_pendingPage);
        ImGui::CloseCurrentPopup();
    }

    UI::SameLineOrWrap(UI::ButtonWidth(stayLabel.c_str()));

    if (UI::Button(stayLabel.c_str()) || ImGui::IsKeyPressed(ImGuiKey_Escape, false))
        ImGui::CloseCurrentPopup();

    ImGui::EndPopup();
}
