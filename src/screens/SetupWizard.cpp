
#include "SetupWizard.h"

#include "config/LayoutUI.h"
#include "config/Theme.h"
#include "localization/Localization.h"
#include "screens/SettingsEditors.h"
#include "settings/RegionalSettings.h"
#include "ui/Widgets.h"

#include "imgui.h"

#include <algorithm>

using SettingsEditors::Section;

SetupWizard::SetupWizard(SettingsRepository& settingsRepository)
    : m_settingsRepository(settingsRepository)
{
}

void SetupWizard::Begin(const SettingsData& initial)
{
    m_data = initial;
    m_step = 0;
    m_message.clear();
}

bool SetupWizard::ValidateStep(int step)
{
    if (step >= static_cast<int>(Section::Count))
        return true;

    const auto validation = SettingsEditors::Validate(m_data);

    if (validation.IsSectionValid(static_cast<Section>(step)))
        return true;

    m_message = Tr("settings.fix_errors");
    return false;
}

SetupWizard::Result SetupWizard::Save(SetupStatus status)
{
    // Every section must be valid before saving. Deferring
    // ("finish later") may leave setup choices incomplete,
    // e.g. no mite method selected yet.
    const bool allowIncomplete = status == SetupStatus::Deferred;
    const auto validation = SettingsEditors::Validate(m_data);

    for (int s = 0; s < static_cast<int>(Section::Count); ++s)
    {
        if (!validation.IsSectionValid(static_cast<Section>(s), allowIncomplete))
        {
            m_step = s;
            m_message = Tr("settings.fix_errors");
            m_scrollToTop = true;
            return Result::None;
        }
    }

    SettingsData toSave = m_data;
    toSave.app.setupStatus = status;

    if (!m_settingsRepository.Save(toSave))
    {
        m_message = Localization::Format(
            "errors.save_failed",
            { { "details", m_settingsRepository.GetLastError() } }
        );
        return Result::None;
    }

    m_data = toSave;
    m_message.clear();

    return status == SetupStatus::Completed ? Result::Completed : Result::Deferred;
}

SetupWizard::Result SetupWizard::Render()
{
    using namespace Theme::Colors;

    Result result = Result::None;

    const ImVec2 available = ImGui::GetContentRegionAvail();
    const float width = std::min(UI::Em(UI::Wizard::WidthEm), available.x);

    // Centre the card horizontally.
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (available.x - width) * 0.5f);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, UI::Px(UI::Spacing::Section, UI::Spacing::Section));
    ImGui::BeginChild(
        "SetupCard",
        ImVec2(width, available.y),
        ImGuiChildFlags_Borders,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse
    );
    ImGui::PopStyleVar();

    if (m_footerHeight <= 0.0f)
    {
        // First frame estimate; measured afterwards.
        m_footerHeight = UI::Px(UI::Size::ButtonHeight) + ImGui::GetStyle().ItemSpacing.y * 3.0f;
    }

    // Short windows: header and steps scroll with the
    // content so the form keeps as much room as possible.
    const bool compact = available.y < UI::Em(UI::Wizard::CompactHeightEm);

    if (!compact)
    {
        DrawHeader();
        ImGui::Separator();
    }

    // ---------------- Body (scrolls both ways) ----------------
    const float bodyHeight = -std::max(m_footerHeight, UI::Px(UI::Size::ButtonHeight));

    UI::BeginScrollPane(
        "SetupBody",
        ImVec2(0.0f, bodyHeight),
        UI::Em(UI::Wizard::MinContentEm),
        ImGuiChildFlags_None
    );

    if (compact)
    {
        DrawHeader();
        ImGui::Separator();
    }

    // After Next / Back show the top of the new step. In
    // compact mode that is just below the (scrolling)
    // header, so the keeper doesn't have to scroll past it
    // on every step.
    if (m_scrollToTop)
    {
        ImGui::SetScrollY(compact ? ImGui::GetCursorPosY() : 0.0f);
        m_scrollToTop = false;
    }

    ImGui::Spacing();

    if (IsReviewStep())
    {
        UI::TextStyled(UI::Text::Heading, TextPrimary, Tr("setup.review"));
        UI::FieldHelp(Tr("setup.review_help"));
        ImGui::Spacing();
        SettingsEditors::DrawReview(m_data);
    }
    else
    {
        const Section section = static_cast<Section>(m_step);

        UI::TextStyled(UI::Text::Heading, TextPrimary, SettingsEditors::SectionTitle(section));
        ImGui::Spacing();

        const auto validation = SettingsEditors::Validate(m_data);

        if (SettingsEditors::DrawSection(section, m_data, validation))
        {
            m_message.clear();

            // Language / date format take effect at once
            // so the rest of setup is shown in that language.
            if (section == Section::Region)
                RegionalSettings::Apply(m_data.app);
        }
    }

    ImGui::Spacing();
    UI::EndScrollPane();

    // ---------------- Footer (pinned) ----------------
    const float footerStart = ImGui::GetCursorPosY();

    ImGui::Separator();
    ImGui::Spacing();

    UI::FieldError(m_message);

    const std::string deferLabel = Localization::Label("setup.use_defaults", "Defer");
    const std::string backLabel = Localization::Label("common.back", "Back");
    const std::string nextLabel = IsReviewStep()
        ? Localization::Label("setup.finish", "Next")
        : Localization::Label("common.next", "Next");

    if (UI::Button(deferLabel.c_str(), UI::ButtonKind::Secondary))
        result = Save(SetupStatus::Deferred);

    if (ImGui::BeginItemTooltip())
    {
        ImGui::TextUnformatted(Tr("setup.use_defaults_help"));
        ImGui::EndTooltip();
    }

    // Back / Next right-aligned (wrap if they don't fit).
    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    const float rightWidth = UI::ButtonWidth(backLabel.c_str()) + spacing + UI::ButtonWidth(nextLabel.c_str());

    UI::SameLineOrWrap(rightWidth);

    const float free = ImGui::GetContentRegionAvail().x - rightWidth;

    if (free > 0.0f)
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + free);

    ImGui::BeginDisabled(m_step == 0);

    if (UI::Button(backLabel.c_str()))
    {
        --m_step;
        m_message.clear();
        m_scrollToTop = true;
    }

    ImGui::EndDisabled();

    ImGui::SameLine();

    if (UI::Button(nextLabel.c_str(), UI::ButtonKind::Primary))
    {
        if (IsReviewStep())
        {
            result = Save(SetupStatus::Completed);
        }
        else if (ValidateStep(m_step))
        {
            ++m_step;
            m_message.clear();
            m_scrollToTop = true;
        }
    }

    m_footerHeight = ImGui::GetCursorPosY() - footerStart;

    ImGui::EndChild();

    return result;
}

void SetupWizard::DrawHeader()
{
    using namespace Theme::Colors;

    UI::TextStyledWrapped(UI::Text::Display, TextPrimary, Tr("setup.title"));
    UI::TextStyledWrapped(UI::Text::Body, TextSecondary, Tr("setup.subtitle"));
    ImGui::Spacing();

    const char* stepLabels[StepCount] =
    {
        SettingsEditors::SectionTitle(Section::Region),
        SettingsEditors::SectionTitle(Section::Seasons),
        SettingsEditors::SectionTitle(Section::MiteMonitoring),
        SettingsEditors::SectionTitle(Section::Maintenance),
        Tr("setup.review"),
    };

    UI::StepIndicator(stepLabels, StepCount, m_step);

    ImGui::Spacing();
}
