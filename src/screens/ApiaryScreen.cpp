
#include "ApiaryScreen.h"

#include "config/LayoutUI.h"
#include "config/Theme.h"
#include "localization/Localization.h"
#include "ui/Display.h"
#include "ui/Widgets.h"

#include "imgui.h"
#include "imgui_stdlib.h"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr const char* ApiaryFormPopupId = "###ApiaryFormDialog";
    constexpr const char* ArchivePopupId = "###ArchiveHiveDialog";

    std::string Trim(const std::string& text)
    {
        const size_t first = text.find_first_not_of(" \t\r\n");

        if (first == std::string::npos)
            return "";

        const size_t last = text.find_last_not_of(" \t\r\n");
        return text.substr(first, last - first + 1);
    }

    // Single-line preview of multi-line notes.
    std::string NotesPreview(const std::string& notes)
    {
        std::string flat = notes;
        std::replace(flat.begin(), flat.end(), '\n', ' ');
        std::replace(flat.begin(), flat.end(), '\r', ' ');

        return UI::Ellipsize(Trim(flat), UI::Card::NotesPreviewChars);
    }
}

// --------------------------------------------------
// Constructor
// --------------------------------------------------

ApiaryScreen::ApiaryScreen(
    ApiaryRepository& apiaryRepository,
    HiveRepository& hiveRepository,
    BoxRepository& boxRepository,
    DatabaseManager& database
)
    : m_apiaryRepository(apiaryRepository),
      m_hiveRepository(hiveRepository),
      m_boxRepository(boxRepository),
      m_hiveForm(hiveRepository, boxRepository, database)
{
    LoadApiaries();
}

int ApiaryScreen::TakeOpenHiveRequest()
{
    const int hiveId = m_openHiveRequest;
    m_openHiveRequest = 0;
    return hiveId;
}

void ApiaryScreen::Refresh(int selectApiaryId)
{
    if (selectApiaryId != 0)
        m_selectedApiaryId = selectApiaryId;

    LoadApiaries();
}

// --------------------------------------------------
// Data Loading
// --------------------------------------------------

void ApiaryScreen::LoadApiaries()
{
    if (!m_apiaryRepository.GetAll(m_apiaries))
    {
        m_pageError = Localization::Format(
            "errors.load_failed",
            { { "details", m_apiaryRepository.GetLastError() } }
        );
        return;
    }

    // Preserve the current selection if possible.
    if (!FindApiary(m_selectedApiaryId))
    {
        m_selectedApiaryId =
            m_apiaries.empty() ? 0 : m_apiaries.front().id;
    }

    LoadHives();
}

void ApiaryScreen::LoadHives()
{
    m_hives.clear();
    m_equipment.clear();

    if (m_selectedApiaryId == 0)
        return;

    if (!m_hiveRepository.GetByApiary(
        m_selectedApiaryId,
        m_hives,
        HiveRepository::ArchiveFilter::All))
    {
        m_pageError = Localization::Format(
            "errors.load_failed",
            { { "details", m_hiveRepository.GetLastError() } }
        );
    }

    if (!m_boxRepository.GetCountsByApiary(m_selectedApiaryId, m_equipment))
    {
        m_pageError = Localization::Format(
            "errors.load_failed",
            { { "details", m_boxRepository.GetLastError() } }
        );
    }
}

const Apiary* ApiaryScreen::FindApiary(int id) const
{
    for (const Apiary& apiary : m_apiaries)
    {
        if (apiary.id == id)
            return &apiary;
    }

    return nullptr;
}

void ApiaryScreen::SetArchived(const Hive& hive, bool archived)
{
    m_pageError.clear();
    m_pageNotice.clear();

    if (!m_hiveRepository.SetArchived(hive.id, archived))
    {
        m_pageError = Localization::Format(
            "errors.save_failed",
            { { "details", m_hiveRepository.GetLastError() } }
        );
        return;
    }

    m_pageNotice = Localization::Format(
        archived ? "hive.archived_notice" : "hive.restored_notice",
        { { "hive", hive.name } }
    );

    LoadHives();
}

// --------------------------------------------------
// Main Screen
// --------------------------------------------------

void ApiaryScreen::Render()
{
    // Drawn inside the page area provided by App (which
    // owns the window, top bar and page padding).
    ImGui::PushID("ApiaryScreen");

    DrawHeader();
    DrawBanners();

    ImGui::Dummy(ImVec2(0.0f, UI::Px(UI::Spacing::Section) * 0.25f));

    // ----------------------------------------------
    // Left pane: apiaries (user-resizable)
    // ----------------------------------------------

    const float available = ImGui::GetContentRegionAvail().x;
    const bool compact = available < UI::Px(UI::Breakpoint::Compact);

    const float sidebarMin = UI::Em(UI::Pane::SidebarMinEm);
    const float sidebarMax = std::max(
        sidebarMin,
        std::min(
            UI::Em(UI::Pane::SidebarMaxEm),
            available * (compact ? UI::Pane::SidebarMaxRatioCompact : UI::Pane::SidebarMaxRatio)
        )
    );

    ImGui::SetNextWindowSizeConstraints(
        ImVec2(sidebarMin, -1.0f),
        ImVec2(sidebarMax, -1.0f)
    );

    UI::BeginScrollPane(
        "ApiarySidebar",
        ImVec2(std::min(UI::Em(UI::Pane::SidebarDefaultEm), sidebarMax), 0.0f),
        UI::Em(UI::Pane::SidebarContentMinEm),
        ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX
    );

    DrawApiaryList();

    UI::EndScrollPane();

    ImGui::SameLine(0.0f, UI::Px(UI::Spacing::Gutter));

    // ----------------------------------------------
    // Right pane: selected apiary and its hives
    // ----------------------------------------------

    UI::BeginScrollPane(
        "HivePanel",
        ImVec2(0.0f, 0.0f),
        UI::Em(UI::Pane::ContentMinEm)
    );

    DrawHivePanel();

    UI::EndScrollPane();

    // ----------------------------------------------
    // Dialogs
    // ----------------------------------------------

    DrawApiaryFormDialog();
    DrawHiveFormDialog();
    DrawArchiveConfirmDialog();

    ImGui::PopID();
}

// --------------------------------------------------
// Header
// --------------------------------------------------

void ApiaryScreen::DrawHeader()
{
    ImGui::BeginGroup();

    UI::TextStyled(UI::Text::Display, Theme::Colors::TextPrimary, Tr("apiary.screen_title"));
    UI::TextStyled(UI::Text::Caption, Theme::Colors::TextSecondary, Tr("apiary.screen_subtitle"));

    ImGui::EndGroup();

    // Primary action, right-aligned when it fits.
    const std::string addLabel = Localization::Label("apiary.add_button", "AddApiary");
    const float addWidth = UI::ButtonWidth(addLabel.c_str());

    UI::SameLineOrWrap(addWidth);

    const float free = ImGui::GetContentRegionAvail().x - addWidth;

    if (free > 0.0f)
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + free);

    if (UI::Button(addLabel.c_str(), UI::ButtonKind::Primary))
    {
        m_apiaryName.clear();
        m_apiaryLocation.clear();
        m_apiaryNotes.clear();
        m_apiaryFormError.clear();

        m_openApiaryForm = true;
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
}

void ApiaryScreen::DrawBanners()
{
    if (!m_pageError.empty()
        && UI::Banner("ErrorBanner", m_pageError, UI::BannerKind::Error, Tr("common.dismiss")))
    {
        m_pageError.clear();
    }

    if (!m_pageNotice.empty()
        && UI::Banner("NoticeBanner", m_pageNotice, UI::BannerKind::Success, Tr("common.dismiss")))
    {
        m_pageNotice.clear();
    }
}

// --------------------------------------------------
// Apiary List
// --------------------------------------------------

void ApiaryScreen::DrawApiaryList()
{
    UI::TextStyled(UI::Text::Label, Theme::Colors::TextSecondary, Tr("apiary.list_heading"));

    ImGui::Separator();
    ImGui::Spacing();

    if (m_apiaries.empty())
    {
        UI::TextStyledWrapped(UI::Text::Caption, Theme::Colors::TextSecondary, Tr("apiary.list_empty"));
        return;
    }

    const float padY = UI::Em(UI::List::RowPaddingYEm);
    const float padX = ImGui::GetStyle().FramePadding.x;
    const float nameHeight = UI::LineHeight(UI::Text::Body);
    const float locationHeight = UI::LineHeight(UI::Text::Caption);

    ImDrawList* drawList = ImGui::GetWindowDrawList();

    for (const Apiary& apiary : m_apiaries)
    {
        ImGui::PushID(apiary.id);

        const bool selected = apiary.id == m_selectedApiaryId;
        const bool hasLocation = !apiary.location.empty();

        const float rowHeight = padY * 2.0f + nameHeight
            + (hasLocation ? locationHeight + UI::Px(2.0f) : 0.0f);

        const ImVec2 rowMin = ImGui::GetCursorScreenPos();

if (ImGui::Selectable("##Apiary", selected, ImGuiSelectableFlags_None, ImVec2(0.0f, rowHeight)))
        {
            if (m_selectedApiaryId != apiary.id)
            {
                m_selectedApiaryId = apiary.id;
                m_pageError.clear();
                m_pageNotice.clear();

                LoadHives();
            }
        }

        // Resting fill + outline over the selectable's real
        // area (same width as the selected highlight) so rows
        // read as tappable. Not drawn while hovered or
        // selected: the selectable paints those states.
        if (!selected && !ImGui::IsItemHovered())
        {
            drawList->AddRectFilled(
                ImGui::GetItemRectMin(), ImGui::GetItemRectMax(),
                ImGui::GetColorU32(Theme::Colors::SurfaceRaised),
                UI::Px(UI::Radius::Frame));

            drawList->AddRect(
                ImGui::GetItemRectMin(), ImGui::GetItemRectMax(),
                ImGui::GetColorU32(Theme::Colors::Border),
                UI::Px(UI::Radius::Frame),
                0,
                UI::Px(UI::Border::Frame));
        }

        const ImVec2 rowMax = ImGui::GetItemRectMax();
        const ImVec4 clip(rowMin.x, rowMin.y, rowMax.x - padX * 0.5f, rowMax.y);

        drawList->AddText(
            Fonts::Get(UI::Text::Body.face),
            nameHeight,
            ImVec2(rowMin.x + padX, rowMin.y + padY),
            ImGui::GetColorU32(Theme::Colors::TextPrimary),
            apiary.name.c_str(),
            nullptr,
            0.0f,
            &clip
        );

        if (hasLocation)
        {
            drawList->AddText(
                Fonts::Get(UI::Text::Caption.face),
                locationHeight,
                ImVec2(rowMin.x + padX, rowMin.y + padY + nameHeight + UI::Px(2.0f)),
                ImGui::GetColorU32(Theme::Colors::TextSecondary),
                apiary.location.c_str(),
                nullptr,
                0.0f,
                &clip
            );
        }

        // Full text on hover in case it was clipped.
        if (ImGui::BeginItemTooltip())
        {
            ImGui::TextUnformatted(apiary.name.c_str());

            if (hasLocation)
                UI::TextStyled(UI::Text::Caption, Theme::Colors::TextSecondary, apiary.location.c_str());

            ImGui::EndTooltip();
        }

        ImGui::PopID();
    }
}

// --------------------------------------------------
// Selected Apiary / Hive Panel
// --------------------------------------------------

void ApiaryScreen::DrawHivePanel()
{
    const Apiary* apiary = FindApiary(m_selectedApiaryId);

    if (!apiary)
    {
        UI::TextStyled(UI::Text::Heading, Theme::Colors::TextPrimary, Tr("apiary.welcome_title"));
        ImGui::Spacing();
        UI::TextStyledWrapped(UI::Text::Body, Theme::Colors::TextSecondary, Tr("apiary.welcome_body"));
        return;
    }

    // ----------------------------------------------
    // Apiary heading
    // ----------------------------------------------

    UI::TextStyledWrapped(UI::Text::Heading, Theme::Colors::TextPrimary, apiary->name.c_str());

    if (!apiary->location.empty())
        UI::TextStyledWrapped(UI::Text::Caption, Theme::Colors::TextSecondary, apiary->location.c_str());

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ----------------------------------------------
    // Hive toolbar (wraps on narrow panes / long text)
    // ----------------------------------------------

    const int activeCount = static_cast<int>(std::count_if(
        m_hives.begin(), m_hives.end(),
        [](const Hive& hive) { return hive.isActive; }
    ));

    const int archivedCount = static_cast<int>(m_hives.size()) - activeCount;

    ImGui::BeginGroup();
    UI::TextStyled(UI::Text::Title, Theme::Colors::TextPrimary, Tr("hive.section_title"));

    const std::string countText = Localization::Format(
        "hive.count_active",
        { { "count", std::to_string(activeCount) } }
    );

    UI::TextStyled(UI::Text::Caption, Theme::Colors::TextSecondary, countText.c_str());
    ImGui::EndGroup();

    const std::string addLabel = Localization::Label("hive.add_button", "AddHive");

    UI::SameLineOrWrap(UI::ButtonWidth(addLabel.c_str()));
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + UI::Px(UI::Spacing::Gutter));

    if (UI::Button(addLabel.c_str(), UI::ButtonKind::Primary))
    {
        m_pageNotice.clear();
        m_hiveForm.OpenForCreate(m_selectedApiaryId);
    }

    const std::string archivedLabel = Localization::WithId(
        Localization::Format("hive.show_archived", { { "count", std::to_string(archivedCount) } }),
        "ShowArchived"
    );

    UI::SameLineOrWrap(
        ImGui::CalcTextSize(archivedLabel.c_str(), nullptr, true).x + ImGui::GetFrameHeight() * 2.0f
    );

    ImGui::SetCursorPosY(
        ImGui::GetCursorPosY()
        + (std::max(UI::Px(UI::Size::ButtonHeight), ImGui::GetFrameHeight()) - ImGui::GetFrameHeight()) * 0.5f
    );

    ImGui::BeginDisabled(archivedCount == 0);
    ImGui::Checkbox(archivedLabel.c_str(), &m_showArchived);
    ImGui::EndDisabled();

    ImGui::Spacing();
    ImGui::Spacing();

    // ----------------------------------------------
    // Empty state
    // ----------------------------------------------

    if (activeCount == 0 && !(m_showArchived && archivedCount > 0))
    {
        ImGui::Spacing();
        UI::TextStyled(UI::Text::Heading, Theme::Colors::TextPrimary, Tr("hive.empty_title"));
        ImGui::Spacing();
        UI::TextStyledWrapped(UI::Text::Body, Theme::Colors::TextSecondary, Tr("hive.empty_body"));
        return;
    }

    DrawHiveGrid();
}

void ApiaryScreen::DrawHiveGrid()
{
    // Responsive columns: as many cards as fit at their
    // minimum width, up to the configured maximum.
    const float gap = UI::Px(UI::Spacing::Gutter);
    const float cardMinWidth = UI::Em(UI::Card::HiveMinWidthEm);
    const float available = ImGui::GetContentRegionAvail().x;

    const int columns = std::clamp(
        static_cast<int>(std::floor((available + gap) / (cardMinWidth + gap))),
        1,
        UI::Card::HiveMaxColumns
    );

    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(gap * 0.5f, gap * 0.5f));

    std::vector<const Hive*> visible;

    for (const Hive& hive : m_hives)
    {
        if (hive.isActive || m_showArchived)
            visible.push_back(&hive);
    }

    if (ImGui::BeginTable("HiveGrid", columns, ImGuiTableFlags_SizingStretchSame))
    {
        for (size_t rowStart = 0; rowStart < visible.size(); rowStart += columns)
        {
            const size_t rowEnd = std::min(visible.size(), rowStart + columns);

            // Tallest card in this row (previous frame), so
            // every card in the row ends at the same line.
            float rowHeight = 0.0f;

            for (size_t i = rowStart; i < rowEnd; ++i)
            {
                const auto it = m_cardHeights.find(visible[i]->id);

                if (it != m_cardHeights.end())
                    rowHeight = std::max(rowHeight, it->second);
            }

            for (size_t i = rowStart; i < rowEnd; ++i)
            {
                ImGui::TableNextColumn();
                DrawHiveCard(*visible[i], rowHeight);
            }
        }

        ImGui::EndTable();
    }

    ImGui::PopStyleVar();
}

void ApiaryScreen::DrawHiveCard(const Hive& hive, float rowHeight)
{
    using namespace Theme::Colors;

    ImGui::PushID(hive.id);

    const bool archived = !hive.isActive;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, UI::Px(UI::Spacing::Panel, UI::Spacing::Panel));

    if (archived)
        ImGui::PushStyleColor(ImGuiCol_ChildBg, Background);

    ImGui::BeginChild(
        "HiveCard",
        ImVec2(0.0f, 0.0f),
        ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY
    );

    // Name (+ archived chip)
    UI::TextStyledWrapped(UI::Text::Title, archived ? TextSecondary : TextPrimary, hive.name.c_str());

    if (archived)
        UI::StatusChip(Tr("hive.archived_chip"), Warning);

    // Type
    const std::string type = Display::HiveType(hive);
    UI::TextStyledWrapped(UI::Text::Caption, TextSecondary, type.c_str());

    // Hive set-up date and colony established date
    const std::string setupDate = Display::Date(hive.setupDate);
    const std::string colonyDate = Display::Date(hive.colonyEstablishedDate);

    const std::string setupText = setupDate.empty()
        ? std::string(Tr("hive.setup_not_set"))
        : Localization::Format("hive.setup_on", { { "date", setupDate } });

    const std::string colonyText = colonyDate.empty()
        ? std::string(Tr("hive.colony_not_set"))
        : Localization::Format("hive.colony_on", { { "date", colonyDate } });

    UI::TextStyledWrapped(UI::Text::Caption, TextMuted, setupText.c_str());
    UI::TextStyledWrapped(UI::Text::Caption, TextMuted, colonyText.c_str());

    // Equipment summary
    {
        const auto it = m_equipment.find(hive.id);
        const HiveEquipmentCounts counts = it != m_equipment.end() ? it->second : HiveEquipmentCounts();

        const std::string equipment = counts.boxes == 0
            ? std::string(Tr("hive.no_boxes"))
            : Localization::Format(
                "hive.equipment_summary",
                {
                    { "boxes", Localization::FormatPlural("hive.box_count", counts.boxes) },
                    { "frames", Localization::FormatPlural("hive.frame_count", counts.frames) }
                });

        UI::TextStyledWrapped(UI::Text::Caption, TextSecondary, equipment.c_str());
    }

    // Notes preview
    if (!hive.notes.empty())
    {
        ImGui::Spacing();
        const std::string preview = NotesPreview(hive.notes);
        UI::TextStyledWrapped(UI::Text::Caption, TextSecondary, preview.c_str());
    }

    ImGui::Spacing();

    // Push the actions down so they line up with the
    // tallest card in the row.
    const auto measured = m_cardHeights.find(hive.id);

    const float spacer = measured != m_cardHeights.end()
        ? std::max(0.0f, rowHeight - measured->second)
        : 0.0f;

    if (spacer > 0.0f)
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + spacer);

    // Actions
    const std::string openLabel = Localization::Label("hive.open", "Open");

    if (UI::Button(openLabel.c_str(), UI::ButtonKind::Primary, -1.0f))
        m_openHiveRequest = hive.id;

    if (archived)
    {
        const std::string restoreLabel = Localization::Label("hive.restore", "Restore");

        UI::SameLineOrWrap(UI::ButtonWidth(restoreLabel.c_str()));

        if (UI::Button(restoreLabel.c_str(), UI::ButtonKind::Secondary, -1.0f))
            SetArchived(hive, false);
    }
    else
    {
        const std::string editLabel = Localization::Label("hive.edit", "Edit");
        const std::string archiveLabel = Localization::Label("hive.archive", "Archive");

        UI::SameLineOrWrap(UI::ButtonWidth(editLabel.c_str()));

        if (UI::Button(editLabel.c_str(), UI::ButtonKind::Secondary, -1.0f))
        {
            m_pageNotice.clear();
            m_hiveForm.OpenForEdit(hive);
        }

        UI::SameLineOrWrap(UI::ButtonWidth(archiveLabel.c_str()));

        if (UI::Button(archiveLabel.c_str(), UI::ButtonKind::Secondary, -1.0f))
        {
            m_archiveCandidate = hive;
            m_openArchiveConfirm = true;
        }
    }

    // Natural height = content without the spacer.
    m_cardHeights[hive.id] = ImGui::GetCursorPosY() - spacer;

    ImGui::EndChild();

    if (archived)
        ImGui::PopStyleColor();

    ImGui::PopStyleVar();
    ImGui::PopID();
}

// --------------------------------------------------
// Add Apiary Dialog
// --------------------------------------------------

void ApiaryScreen::DrawApiaryFormDialog()
{
    if (m_openApiaryForm)
    {
        ImGui::OpenPopup(ApiaryFormPopupId);
        m_openApiaryForm = false;
    }

    UI::SetNextDialogLayout(UI::Dialog::WidthEm);

    const std::string title = Localization::Label("apiary.form.title_add", "ApiaryFormDialog");

    if (!ImGui::BeginPopupModal(
        title.c_str(),
        nullptr,
        ImGuiWindowFlags_NoSavedSettings))
    {
        return;
    }

    UI::BeginDialogBody("ApiaryFormBody");

    // Name
    UI::FormLabel(Tr("apiary.form.name"), true);

    if (ImGui::IsWindowAppearing())
        ImGui::SetKeyboardFocusHere();

    ImGui::SetNextItemWidth(-FLT_MIN);

    if (ImGui::InputText("##ApiaryName", &m_apiaryName))
        m_apiaryFormError.clear();

    UI::FieldError(m_apiaryFormError);
    ImGui::Spacing();

    // Location
    UI::FormLabel(Tr("apiary.form.location"));
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputText("##ApiaryLocation", &m_apiaryLocation);
    ImGui::Spacing();

    // Notes
    UI::FormLabel(Tr("apiary.form.notes"));
    ImGui::InputTextMultiline(
        "##ApiaryNotes",
        &m_apiaryNotes,
        ImVec2(-FLT_MIN, UI::Em(UI::Dialog::NotesHeightEm)),
        ImGuiInputTextFlags_WordWrap
    );

    UI::EndDialogBody();

    ImGui::Separator();
    ImGui::Spacing();

    const std::string saveLabel = Localization::Label("apiary.form.save", "Save");
    const std::string cancelLabel = Localization::Label("common.cancel", "Cancel");

    if (UI::Button(saveLabel.c_str(), UI::ButtonKind::Primary))
    {
        Apiary apiary;
        apiary.name = Trim(m_apiaryName);
        apiary.location = Trim(m_apiaryLocation);
        apiary.notes = m_apiaryNotes;

        if (apiary.name.empty())
        {
            m_apiaryFormError = Tr("apiary.form.name_required");
        }
        else if (m_apiaryRepository.Create(apiary))
        {
            m_selectedApiaryId = apiary.id;
            m_apiaryFormError.clear();

            LoadApiaries();

            ImGui::CloseCurrentPopup();
        }
        else
        {
            m_apiaryFormError = Localization::Format(
                "errors.save_failed",
                { { "details", m_apiaryRepository.GetLastError() } }
            );
        }
    }

    UI::SameLineOrWrap(UI::ButtonWidth(cancelLabel.c_str()));

    const bool escape =
        ImGui::IsKeyPressed(ImGuiKey_Escape, false) && !ImGui::IsAnyItemActive();

    if (UI::Button(cancelLabel.c_str()) || escape)
    {
        m_apiaryFormError.clear();
        ImGui::CloseCurrentPopup();
    }

    UI::EndDialogFooter();

    ImGui::EndPopup();
}

// --------------------------------------------------
// Add / Edit Hive Dialog
// --------------------------------------------------

void ApiaryScreen::DrawHiveFormDialog()
{
    const HiveFormDialog::Result result = m_hiveForm.Draw(m_apiaries);

    if (result == HiveFormDialog::Result::None)
        return;

    m_pageError.clear();
    m_pageNotice.clear();

    const Hive& saved = m_hiveForm.SavedHive();

    // Edited hive moved to another apiary: tell the
    // user where it went (it leaves this list).
    if (result == HiveFormDialog::Result::Updated
        && saved.apiaryId != m_hiveForm.OriginalHive().apiaryId)
    {
        const Apiary* destination = FindApiary(saved.apiaryId);

        m_pageNotice = Localization::Format(
            "hive.moved_notice",
            {
                { "hive", saved.name },
                { "apiary", destination ? destination->name : std::string() }
            }
        );
    }

    LoadHives();
}

// --------------------------------------------------
// Archive Confirmation
// --------------------------------------------------

void ApiaryScreen::DrawArchiveConfirmDialog()
{
    if (m_openArchiveConfirm)
    {
        ImGui::OpenPopup(ArchivePopupId);
        m_openArchiveConfirm = false;
    }

    UI::SetNextDialogLayout(UI::Dialog::ConfirmWidthEm);

    const std::string title = Localization::Label("hive.archive_dialog.title", "ArchiveHiveDialog");

    if (!ImGui::BeginPopupModal(title.c_str(), nullptr, ImGuiWindowFlags_NoSavedSettings))
        return;

    const std::string body = Localization::Format(
        "hive.archive_dialog.body",
        { { "hive", m_archiveCandidate.name } }
    );

    UI::TextStyledWrapped(UI::Text::Body, Theme::Colors::TextPrimary, body.c_str());

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    const std::string confirmLabel = Localization::Label("hive.archive_dialog.confirm", "Confirm");
    const std::string cancelLabel = Localization::Label("common.cancel", "Cancel");

    if (UI::Button(confirmLabel.c_str(), UI::ButtonKind::Danger))
    {
        SetArchived(m_archiveCandidate, true);
        ImGui::CloseCurrentPopup();
    }

    UI::SameLineOrWrap(UI::ButtonWidth(cancelLabel.c_str()));

    if (UI::Button(cancelLabel.c_str()) || ImGui::IsKeyPressed(ImGuiKey_Escape, false))
        ImGui::CloseCurrentPopup();

    ImGui::EndPopup();
}
