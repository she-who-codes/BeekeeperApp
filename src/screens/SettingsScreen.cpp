
#include "SettingsScreen.h"

#include "config/LayoutUI.h"
#include "config/Theme.h"
#include "localization/Localization.h"
#include "settings/RegionalSettings.h"
#include "ui/Widgets.h"

#include "imgui.h"

#include <algorithm>

using SettingsEditors::Section;

SettingsScreen::SettingsScreen(SettingsRepository& settingsRepository)
    : m_settingsRepository(settingsRepository)
{
}

void SettingsScreen::Load(const SettingsData& current)
{
    m_saved = current;
    m_draft = current;
    m_error.clear();
    m_notice.clear();
}

bool SettingsScreen::HasUnsavedChanges() const
{
    return m_draft != m_saved;
}

bool SettingsScreen::CanSave() const
{
    // A deferred setup can be completed by saving even
    // without changes.
    return HasUnsavedChanges() || m_saved.app.setupStatus != SetupStatus::Completed;
}

bool SettingsScreen::Save()
{
    m_notice.clear();

    const auto validation = SettingsEditors::Validate(m_draft);

    if (!validation.IsValid())
    {
        // Show the first section with a problem.
        for (int s = 0; s < static_cast<int>(Section::Count); ++s)
        {
            if (!validation.IsSectionValid(static_cast<Section>(s)))
            {
                m_section = static_cast<Section>(s);
                break;
            }
        }

        m_error = Tr("settings.fix_errors");
        return false;
    }

    SettingsData toSave = m_draft;
    toSave.app.setupStatus = SetupStatus::Completed;

    if (!m_settingsRepository.Save(toSave))
    {
        m_error = Localization::Format(
            "errors.save_failed",
            { { "details", m_settingsRepository.GetLastError() } }
        );
        return false;
    }

    m_saved = toSave;
    m_draft = toSave;
    m_error.clear();
    m_notice = Tr("settings.saved");

    RegionalSettings::Apply(m_saved.app);
    return true;
}

void SettingsScreen::Discard()
{
    m_draft = m_saved;
    m_error.clear();
    m_notice.clear();
}

bool SettingsScreen::Render()
{
    using namespace Theme::Colors;

    bool saved = false;

    // ---------------- Header ----------------
    UI::TextStyled(UI::Text::Display, TextPrimary, Tr("settings.title"));

    // Short windows: drop the subtitle to keep room for the editor.
    if (ImGui::GetContentRegionAvail().y >= UI::Em(UI::SettingsView::CompactHeightEm))
        UI::TextStyled(UI::Text::Caption, TextSecondary, Tr("settings.subtitle"));

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (m_saved.app.setupStatus == SetupStatus::Deferred)
    {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, UI::Mix(Surface, Warning, 0.14f));
        UI::BeginCard("DeferredNotice");
        UI::TextStyledWrapped(UI::Text::Body, Warning, Tr("settings.deferred_notice"));
        UI::EndCard();
        ImGui::PopStyleColor();
        ImGui::Spacing();
    }

    if (m_footerHeight <= 0.0f)
        m_footerHeight = UI::Px(UI::Size::ButtonHeight) + ImGui::GetStyle().ItemSpacing.y * 3.0f;

    const auto validation = SettingsEditors::Validate(m_draft);

    // ---------------- Section list ----------------
    UI::BeginScrollPane(
        "SettingsNav",
        ImVec2(UI::Em(UI::SettingsView::NavWidthEm), -m_footerHeight),
        0.0f
    );

    DrawNavigation(validation);

    UI::EndScrollPane();

    ImGui::SameLine(0.0f, UI::Px(UI::Spacing::Gutter));

    // ---------------- Section editor ----------------
    UI::BeginScrollPane(
        "SettingsContent",
        ImVec2(0.0f, -m_footerHeight),
        UI::Em(UI::SettingsView::ContentMinEm)
    );

    // Keep a readable line length on wide windows.
    const float contentWidth = std::min(
        ImGui::GetContentRegionAvail().x,
        UI::Em(UI::SettingsView::ContentMaxWidthEm)
    );

    ImGui::PushStyleColor(ImGuiCol_ChildBg, UI::WithAlpha(Surface, 0.0f));
    ImGui::BeginChild("SectionBody", ImVec2(contentWidth, 0.0f), ImGuiChildFlags_AutoResizeY);
    ImGui::PopStyleColor();

    UI::TextStyled(UI::Text::Heading, TextPrimary, SettingsEditors::SectionTitle(m_section));
    ImGui::Spacing();

    if (SettingsEditors::DrawSection(m_section, m_draft, validation))
        m_notice.clear();

    ImGui::EndChild();

    UI::EndScrollPane();

    // ---------------- Footer ----------------
    const float footerStart = ImGui::GetCursorPosY();

    saved = DrawFooter();

    m_footerHeight = ImGui::GetCursorPosY() - footerStart;

    return saved;
}

void SettingsScreen::DrawNavigation(const SettingsEditors::Validation& validation)
{
    using namespace Theme::Colors;

    const float rowHeight = std::max(UI::Px(UI::Size::ButtonHeight), ImGui::GetFrameHeight());
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    for (int s = 0; s < static_cast<int>(Section::Count); ++s)
    {
        const Section section = static_cast<Section>(s);
        const bool selected = section == m_section;
        const bool valid = validation.IsSectionValid(section);

        ImGui::PushID(s);

        const ImVec2 rowMin = ImGui::GetCursorScreenPos();

if (ImGui::Selectable("##Section", selected, ImGuiSelectableFlags_None, ImVec2(0.0f, rowHeight)))
            m_section = section;

        // Resting fill + outline over the selectable's real
        // area (same width as the selected highlight) so rows
        // read as tappable. Not drawn while hovered or
        // selected: the selectable paints those states.
        if (!selected && !ImGui::IsItemHovered())
        {
            drawList->AddRectFilled(
                ImGui::GetItemRectMin(), ImGui::GetItemRectMax(),
                ImGui::GetColorU32(SurfaceRaised),
                UI::Px(UI::Radius::Frame));

            drawList->AddRect(
                ImGui::GetItemRectMin(), ImGui::GetItemRectMax(),
                ImGui::GetColorU32(Theme::Colors::Border),
                UI::Px(UI::Radius::Frame),
                0,
                UI::Px(UI::Border::Frame));
        }

        const ImVec2 rowMax = ImGui::GetItemRectMax();
        const float textY = rowMin.y + (rowHeight - UI::LineHeight(UI::Text::Body)) * 0.5f;

        drawList->AddText(
            ImVec2(rowMin.x + ImGui::GetStyle().FramePadding.x, textY),
            ImGui::GetColorU32(selected ? TextPrimary : TextSecondary),
            SettingsEditors::SectionTitle(section)
        );

        // Problem indicator.
        if (!valid)
        {
            const float radius = UI::Px(4.0f);
            drawList->AddCircleFilled(
                ImVec2(rowMax.x - radius * 3.0f, (rowMin.y + rowMax.y) * 0.5f),
                radius,
                ImGui::GetColorU32(Danger)
            );
        }

        ImGui::PopID();
    }
}

bool SettingsScreen::DrawFooter()
{
    using namespace Theme::Colors;

    bool saved = false;

    ImGui::Separator();
    ImGui::Spacing();

    // While setup is unfinished, saving also finishes it.
    const std::string saveLabel = Localization::Label(
        m_saved.app.setupStatus == SetupStatus::Completed ? "common.save" : "settings.save_finish",
        "Save"
    );
    const std::string discardLabel = Localization::Label("common.discard", "Discard");

    ImGui::BeginDisabled(!CanSave());

    if (UI::Button(saveLabel.c_str(), UI::ButtonKind::Primary))
        saved = Save();

    ImGui::EndDisabled();

    ImGui::SameLine();

    ImGui::BeginDisabled(!HasUnsavedChanges());

    if (UI::Button(discardLabel.c_str()))
        Discard();

    ImGui::EndDisabled();

    // Status text
    const char* status = nullptr;
    ImVec4 color = TextMuted;

    if (!m_error.empty())
    {
        status = m_error.c_str();
        color = Danger;
    }
    else if (HasUnsavedChanges())
    {
        status = Tr("settings.unsaved");
        color = Warning;
    }
    else if (!m_notice.empty())
    {
        status = m_notice.c_str();
        color = Success;
    }

    if (status)
    {
        // SameLine() already aligns text to the buttons' baseline.
        UI::SameLineOrWrap(ImGui::CalcTextSize(status).x);
        ImGui::TextColored(color, "%s", status);
    }

    return saved;
}
