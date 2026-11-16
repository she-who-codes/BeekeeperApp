
#include "HiveDetailScreen.h"

#include "config/LayoutUI.h"
#include "config/Theme.h"
#include "localization/DateFormat.h"
#include "localization/Localization.h"
#include "models/HiveEquipment.h"
#include "screens/SettingsEditors.h"
#include "ui/Display.h"
#include "ui/Widgets.h"

#include "imgui.h"
#include "imgui_stdlib.h"

#include <algorithm>

namespace
{
    constexpr const char* RemoveBoxPopupId = "###RemoveBoxDialog";
    constexpr const char* ReplaceFramePopupId = "###ReplaceFrameDialog";
    constexpr const char* DiscardFramePopupId = "###DiscardFrameDialog";

    constexpr const char* CombReplacementTaskKey = "comb_replacement";

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

    std::string TodayText()
    {
        return DateFormat::Format(DateFormat::Today(), Display::DatePattern());
    }

    float TileWidth()
    {
        return std::max(UI::Px(UI::Size::TouchTarget), UI::Em(3.2f));
    }

    float TileHeight()
    {
        return UI::Em(UI::HiveStack::FrameTileHeightEm);
    }

    // Info row in a two-column table.
    void InfoRow(const char* label, const std::string& value)
    {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        UI::TextStyled(UI::Text::Label, Theme::Colors::TextSecondary, label);
        ImGui::TableNextColumn();
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextUnformatted(value.c_str());
        ImGui::PopTextWrapPos();
    }
}

HiveDetailScreen::HiveDetailScreen(
    DatabaseManager& database,
    ApiaryRepository& apiaryRepository,
    HiveRepository& hiveRepository,
    BoxRepository& boxRepository,
    const SettingsData& settings)
    : m_apiaryRepository(apiaryRepository),
      m_hiveRepository(hiveRepository),
      m_boxRepository(boxRepository),
      m_settings(settings),
      m_boxDialog(boxRepository),
      m_hiveForm(hiveRepository, boxRepository, database)
{
}

// --------------------------------------------------
// Data
// --------------------------------------------------

bool HiveDetailScreen::Open(int hiveId)
{
    m_error.clear();
    m_notice.clear();
    m_selectedFrameId = 0;
    m_pendingFrameId = 0;

    if (!m_hiveRepository.GetById(hiveId, m_hive))
    {
        m_error = Localization::Format("errors.load_failed", { { "details", m_hiveRepository.GetLastError() } });
        return false;
    }

    Reload();
    return true;
}

void HiveDetailScreen::Reload()
{
    m_hiveRepository.GetById(m_hive.id, m_hive);

    if (!m_apiaryRepository.GetAll(m_apiaries))
        m_error = Localization::Format("errors.load_failed", { { "details", m_apiaryRepository.GetLastError() } });

    m_apiary = Apiary();

    for (const Apiary& apiary : m_apiaries)
    {
        if (apiary.id == m_hive.apiaryId)
            m_apiary = apiary;
    }

    if (!m_boxRepository.GetStack(m_hive.id, m_stack))
        m_error = Localization::Format("errors.load_failed", { { "details", m_boxRepository.GetLastError() } });

    // Keep the selection if the frame still exists.
    if (m_selectedFrameId != 0 && !FindFrame(m_selectedFrameId))
        m_selectedFrameId = 0;

    SelectFrame(m_selectedFrameId);
}

const Frame* HiveDetailScreen::FindFrame(int frameId, const Box** owner) const
{
    for (const Box& box : m_stack)
    {
        for (const Frame& frame : box.frames)
        {
            if (frame.id == frameId)
            {
                if (owner)
                    *owner = &box;
                return &frame;
            }
        }
    }

    return nullptr;
}

void HiveDetailScreen::SelectFrame(int frameId)
{
    m_selectedFrameId = frameId;
    m_draftDateError.clear();

    const Frame* frame = FindFrame(frameId);

    if (!frame)
    {
        m_selectedFrameId = 0;
        return;
    }

    m_draftTypeIndex = FrameTypeIndex(frame->frameType);
    m_draftLabel = frame->label;
    m_draftDateText = Display::Date(frame->installedAt);
    m_draftNotes = frame->notes;
}

bool HiveDetailScreen::IsFrameDirty() const
{
    const Frame* frame = FindFrame(m_selectedFrameId);

    if (!frame)
        return false;

    return HiveEquipment::FrameTypes()[m_draftTypeIndex].key != frame->frameType
        || m_draftLabel != frame->label
        || m_draftDateText != Display::Date(frame->installedAt)
        || m_draftNotes != frame->notes;
}

bool HiveDetailScreen::IsReplacementDue(const Frame& frame) const
{
    if (!HiveEquipment::HasComb(frame.frameType))
        return false;

    DateFormat::Date installed;

    if (!DateFormat::ParseIso(frame.installedAt, installed))
        return false;

    for (const MaintenanceTaskType& task : m_settings.taskTypes)
    {
        if (task.key != CombReplacementTaskKey || !task.enabled || task.archived)
            continue;

        DateFormat::Date due = installed;

        switch (task.intervalUnit)
        {
            case IntervalUnit::Days:   due = DateFormat::AddDays(installed, task.intervalValue); break;
            case IntervalUnit::Weeks:  due = DateFormat::AddDays(installed, task.intervalValue * 7L); break;
            case IntervalUnit::Months: due = DateFormat::AddMonths(installed, task.intervalValue); break;
            case IntervalUnit::Years:  due = DateFormat::AddMonths(installed, task.intervalValue * 12); break;
        }

        return DateFormat::Compare(DateFormat::Today(), due) >= 0;
    }

    return false;
}

std::string HiveDetailScreen::ReplacementIntervalText() const
{
    for (const MaintenanceTaskType& task : m_settings.taskTypes)
    {
        if (task.key == CombReplacementTaskKey && task.enabled && !task.archived)
        {
            const char* key = "interval.every_years";

            switch (task.intervalUnit)
            {
                case IntervalUnit::Days:   key = "interval.every_days"; break;
                case IntervalUnit::Weeks:  key = "interval.every_weeks"; break;
                case IntervalUnit::Months: key = "interval.every_months"; break;
                case IntervalUnit::Years:  key = "interval.every_years"; break;
            }

            return Localization::Format(
                "frames.replace_interval",
                { { "interval", Localization::FormatPlural(key, task.intervalValue) } }
            );
        }
    }

    return "";
}

// --------------------------------------------------
// Frame
// --------------------------------------------------

HiveDetailScreen::Navigation HiveDetailScreen::Render()
{
    ImGui::PushID("HiveDetail");

    Navigation navigation = DrawHeader();

    DrawBanners();

    const std::string overviewLabel = Localization::Label("hive_detail.tab_overview", "TabOverview");
    const std::string boxesLabel = Localization::Label("hive_detail.tab_boxes", "TabBoxes");

    if (ImGui::BeginTabBar("HiveTabs"))
    {
        if (ImGui::BeginTabItem(overviewLabel.c_str()))
        {
            ImGui::Spacing();
            DrawOverview();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem(boxesLabel.c_str()))
        {
            ImGui::Spacing();
            DrawBoxesTab();
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    // ---------------- Dialogs ----------------
    const BoxDialog::Result boxResult = m_boxDialog.Draw();

    if (boxResult != BoxDialog::Result::None)
    {
        m_notice = Tr(boxResult == BoxDialog::Result::Added ? "boxes.added_notice" : "boxes.updated_notice");
        RequestReload();
    }

    if (m_hiveForm.Draw(m_apiaries) != HiveFormDialog::Result::None)
        RequestReload();

    DrawRemoveBoxDialog();
    DrawReplaceFrameDialog();
    DrawDiscardFrameDialog();

    ImGui::PopID();

    if (m_reloadRequested)
    {
        m_reloadRequested = false;
        Reload();
    }

    return navigation;
}

HiveDetailScreen::Navigation HiveDetailScreen::DrawHeader()
{
    using namespace Theme::Colors;

    Navigation navigation = Navigation::None;

    // Breadcrumb: Apiaries > Apiary > Hive
    {
        UI::ScopedFont font(UI::Text::Caption);

        if (ImGui::TextLink(Localization::Label("navigation.apiaries", "CrumbApiaries").c_str()))
            navigation = Navigation::Apiaries;

        ImGui::SameLine(0.0f, UI::Em(0.4f));
        ImGui::TextColored(TextMuted, "\xE2\x80\xBA");   // U+203A
        ImGui::SameLine(0.0f, UI::Em(0.4f));

        if (ImGui::TextLink(Localization::WithId(m_apiary.name, "CrumbApiary").c_str()))
            navigation = Navigation::Apiary;

        ImGui::SameLine(0.0f, UI::Em(0.4f));
        ImGui::TextColored(TextMuted, "\xE2\x80\xBA");
        ImGui::SameLine(0.0f, UI::Em(0.4f));
        ImGui::TextColored(TextSecondary, "%s", m_hive.name.c_str());
    }

    ImGui::Spacing();

    // Title + archived chip + Edit hive
    ImGui::BeginGroup();
    UI::TextStyledWrapped(UI::Text::Display, TextPrimary, m_hive.name.c_str());

    const std::string subtitle = Display::HiveType(m_hive) + "  \xC2\xB7  " + m_apiary.name;
    UI::TextStyled(UI::Text::Caption, TextSecondary, subtitle.c_str());

    if (IsReadOnly())
        UI::StatusChip(Tr("hive.archived_chip"), Warning);

    ImGui::EndGroup();

    const std::string editLabel = Localization::Label("hive_detail.edit_hive", "EditHive");
    const float editWidth = UI::ButtonWidth(editLabel.c_str());

    UI::SameLineOrWrap(editWidth);

    const float free = ImGui::GetContentRegionAvail().x - editWidth;

    if (free > 0.0f)
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + free);

    ImGui::BeginDisabled(IsReadOnly());

    if (UI::Button(editLabel.c_str()))
        m_hiveForm.OpenForEdit(m_hive);

    ImGui::EndDisabled();

    ImGui::Spacing();
    return navigation;
}

void HiveDetailScreen::DrawBanners()
{
    if (!m_error.empty() && UI::Banner("HiveError", m_error, UI::BannerKind::Error, Tr("common.dismiss")))
        m_error.clear();

    if (!m_notice.empty() && UI::Banner("HiveNotice", m_notice, UI::BannerKind::Success, Tr("common.dismiss")))
        m_notice.clear();
}

// --------------------------------------------------
// Overview
// --------------------------------------------------

void HiveDetailScreen::DrawOverview()
{
    using namespace Theme::Colors;

    UI::BeginScrollPane("Overview", ImVec2(0.0f, 0.0f), UI::Em(UI::Pane::ContentMinEm), ImGuiChildFlags_None);

    const float width = std::min(ImGui::GetContentRegionAvail().x, UI::Em(UI::SettingsView::ContentMaxWidthEm));

    ImGui::PushStyleColor(ImGuiCol_ChildBg, UI::WithAlpha(Surface, 0.0f));
    ImGui::BeginChild("OverviewBody", ImVec2(width, 0.0f), ImGuiChildFlags_AutoResizeY);
    ImGui::PopStyleColor();

    auto beginTable = [](const char* id)
    {
        const bool open = ImGui::BeginTable(id, 2, ImGuiTableFlags_SizingFixedFit);

        if (open)
        {
            ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, UI::Em(13.0f));
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
        }

        return open;
    };

    // ---------------- Hive ----------------
    UI::BeginCard("HiveInfo");
    UI::TextStyled(UI::Text::Title, TextPrimary, Tr("hive_detail.section_hive"));
    ImGui::Spacing();

    if (beginTable("HiveRows"))
    {
        const std::string setup = Display::Date(m_hive.setupDate);
        const std::string colony = Display::Date(m_hive.colonyEstablishedDate);

        InfoRow(Tr("hive.form.type"), Display::HiveType(m_hive));
        InfoRow(Tr("hive.form.apiary"), m_apiary.name);
        InfoRow(Tr("hive.form.setup_date"), setup.empty() ? std::string(Tr("hive.setup_not_set")) : setup);
        InfoRow(Tr("hive.form.colony_date"), colony.empty() ? std::string(Tr("hive.colony_not_set")) : colony);

        if (!m_hive.notes.empty())
            InfoRow(Tr("hive.form.notes"), m_hive.notes);

        ImGui::EndTable();
    }

    UI::EndCard();
    ImGui::Spacing();

    // ---------------- Equipment ----------------
    int frames = 0;
    int feeders = 0;
    int due = 0;

    for (const Box& box : m_stack)
    {
        for (const Frame& frame : box.frames)
        {
            if (frame.frameType == "feeder")
                ++feeders;
            else if (HiveEquipment::HasComb(frame.frameType))
                ++frames;

            if (IsReplacementDue(frame))
                ++due;
        }
    }

    UI::BeginCard("Equipment");
    UI::TextStyled(UI::Text::Title, TextPrimary, Tr("hive_detail.section_equipment"));
    ImGui::Spacing();

    if (m_stack.empty())
    {
        UI::TextStyledWrapped(UI::Text::Body, TextSecondary, Tr("boxes.empty"));
    }
    else if (beginTable("EquipmentRows"))
    {
        InfoRow(Tr("hive_detail.boxes"), std::to_string(m_stack.size()));
        InfoRow(Tr("hive_detail.frames"), std::to_string(frames));

        if (feeders > 0)
            InfoRow(Tr("hive_detail.feeder_frames"), std::to_string(feeders));

        if (due > 0)
            InfoRow(Tr("hive_detail.replacement_due"), std::to_string(due));

        ImGui::EndTable();
    }

    UI::EndCard();

    ImGui::EndChild();
    UI::EndScrollPane();
}

// --------------------------------------------------
// Boxes & frames
// --------------------------------------------------

void HiveDetailScreen::DrawBoxesTab()
{
    using namespace Theme::Colors;

    // Toolbar
    const std::string addLabel = Localization::Label("boxes.add", "AddBox");

    ImGui::BeginDisabled(IsReadOnly());

    if (UI::Button(addLabel.c_str(), UI::ButtonKind::Primary))
        m_boxDialog.OpenForAdd(m_hive.id, m_hive.hiveType, m_stack);

    ImGui::EndDisabled();

    // Legend
    const char* legendFeeder = Tr("frames.legend_feeder");
    const char* legendDue = Tr("frames.legend_due");

    UI::SameLineOrWrap(UI::StatusChipWidth(legendFeeder) + UI::StatusChipWidth(legendDue) + UI::Em(1.0f));
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (std::max(UI::Px(UI::Size::ButtonHeight), ImGui::GetFrameHeight()) - UI::Em(1.1f)) * 0.5f);
    UI::StatusChip(legendFeeder, Honey);
    ImGui::SameLine();
    UI::StatusChip(legendDue, Danger);

    ImGui::Spacing();

    // Stack + details: side by side when wide, stacked when narrow.
    const ImVec2 available = ImGui::GetContentRegionAvail();
    const bool sideBySide = available.x >= UI::Em(UI::HiveStack::SideBySideMinEm);
    const float gutter = UI::Px(UI::Spacing::Gutter);

    const float panelWidth = std::min(UI::Em(UI::HiveStack::DetailPanelWidthEm), available.x * 0.45f);

    const ImVec2 stackSize = sideBySide
        ? ImVec2(available.x - panelWidth - gutter, 0.0f)
        : ImVec2(0.0f, std::max(available.y * 0.58f, UI::Em(12.0f)));

    UI::BeginScrollPane("Stack", stackSize, UI::Em(UI::HiveStack::StackMinEm));
    DrawStack();
    UI::EndScrollPane();

    if (sideBySide)
        ImGui::SameLine(0.0f, gutter);

    UI::BeginScrollPane("FramePanel", ImVec2(0.0f, 0.0f), UI::Em(UI::HiveStack::DetailPanelWidthEm * 0.8f));
    DrawFramePanel();
    UI::EndScrollPane();
}

void HiveDetailScreen::DrawStack()
{
    using namespace Theme::Colors;

    if (m_stack.empty())
    {
        UI::TextStyled(UI::Text::Heading, TextPrimary, Tr("boxes.empty_title"));
        ImGui::Spacing();
        UI::TextStyledWrapped(UI::Text::Body, TextSecondary, Tr("boxes.empty"));
        return;
    }

    UI::TextStyled(UI::Text::Caption, TextMuted, Tr("boxes.top_of_hive"));

    const float rowWidth = ImGui::GetContentRegionAvail().x;

    // Top box first so the stack looks like the real hive.
    for (size_t i = m_stack.size(); i-- > 0;)
    {
        ImGui::PushID(m_stack[i].id);
        DrawBox(m_stack[i], i + 1 == m_stack.size(), i == 0, rowWidth);
        ImGui::PopID();
    }

    UI::TextStyled(UI::Text::Caption, TextMuted, Tr("boxes.bottom_of_hive"));
}

void HiveDetailScreen::DrawBox(const Box& box, bool isTop, bool isBottom, float rowWidth)
{
    using namespace Theme::Colors;

    const auto* type = HiveEquipment::FindBoxType(box.boxType);
    const bool holdsFrames = type ? type->holdsFrames : !box.frames.empty();

    UI::BeginCard("Box");

    // ---------------- Header ----------------
    ImGui::BeginGroup();
    UI::TextStyled(UI::Text::Title, TextPrimary, Display::BoxTitle(box).c_str());

    const std::string installed = Display::Date(box.installedAt);

    if (!installed.empty())
    {
        const std::string text = Localization::Format("boxes.installed_on", { { "date", installed } });
        UI::TextStyled(UI::Text::Caption, TextMuted, text.c_str());
    }

    ImGui::EndGroup();

    // Actions (right-aligned, wrap on narrow panes)
    const std::string upLabel = Localization::WithId("\xE2\x86\x91", "Up");       // U+2191
    const std::string downLabel = Localization::WithId("\xE2\x86\x93", "Down");   // U+2193
    const std::string editLabel = Localization::Label("common.edit", "EditBox");
    const std::string removeLabel = Localization::Label("common.remove", "RemoveBox");

    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    const float actionsWidth = UI::ButtonWidth(upLabel.c_str()) * 2.0f + UI::ButtonWidth(editLabel.c_str())
        + UI::ButtonWidth(removeLabel.c_str()) + spacing * 3.0f;

    UI::SameLineOrWrap(actionsWidth);

    const float free = ImGui::GetContentRegionAvail().x - actionsWidth;

    if (free > 0.0f)
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + free);

    ImGui::BeginDisabled(IsReadOnly());

    ImGui::BeginDisabled(isTop);

    if (UI::Button(upLabel.c_str(), UI::ButtonKind::Secondary, -1.0f))
    {
        if (!m_boxRepository.MoveBox(box.id, +1))
            m_error = Localization::Format("errors.save_failed", { { "details", m_boxRepository.GetLastError() } });
        RequestReload();
    }

    ImGui::SetItemTooltip("%s", Tr("boxes.move_up"));
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::BeginDisabled(isBottom);

    if (UI::Button(downLabel.c_str(), UI::ButtonKind::Secondary, -1.0f))
    {
        if (!m_boxRepository.MoveBox(box.id, -1))
            m_error = Localization::Format("errors.save_failed", { { "details", m_boxRepository.GetLastError() } });
        RequestReload();
    }

    ImGui::SetItemTooltip("%s", Tr("boxes.move_down"));
    ImGui::EndDisabled();

    ImGui::SameLine();

    if (UI::Button(editLabel.c_str(), UI::ButtonKind::Secondary, -1.0f))
        m_boxDialog.OpenForEdit(box, m_hive.hiveType);

    ImGui::SameLine();

    if (UI::Button(removeLabel.c_str(), UI::ButtonKind::Secondary, -1.0f))
    {
        m_removeCandidate = box;
        m_removeDateText = TodayText();
        m_removeDateError.clear();
        m_openRemoveBox = true;
    }

    ImGui::EndDisabled();

    ImGui::Spacing();

    // ---------------- Frames ----------------
    if (holdsFrames)
    {
        // Each box's frame row scrolls sideways on its own, so
        // box titles and buttons stay in view for long
        // (horizontal) hives. Vertical wheel still scrolls the
        // stack: the row cannot scroll vertically.
        const ImGuiStyle& style = ImGui::GetStyle();
        const float gap = UI::Px(UI::HiveStack::FrameGap);
        const float width = TileWidth();
        const float height = TileHeight();
        const int count = static_cast<int>(box.frames.size());

        const float needed = count > 0 ? count * width + (count - 1) * gap : 0.0f;
        const bool overflows = needed > ImGui::GetContentRegionAvail().x;
        const float rowHeight = height + (overflows ? style.ScrollbarSize + UI::Px(4.0f) : 0.0f);

        ImGui::SetNextWindowContentSize(ImVec2(needed, height));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, UI::WithAlpha(Surface, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

        ImGui::BeginChild(
            "Frames",
            ImVec2(0.0f, rowHeight),
            ImGuiChildFlags_None,
            ImGuiWindowFlags_HorizontalScrollbar
        );

        ImGui::PopStyleVar();
        ImGui::PopStyleColor();

        for (int i = 0; i < count; ++i)
        {
            if (i > 0)
                ImGui::SameLine(0.0f, gap);

            DrawFrameTile(box.frames[i], width, height);
        }

        ImGui::EndChild();
    }
    else
    {
        // Bar for boxes without frames.
        const float barWidth = std::max(0.0f, rowWidth - UI::Px(UI::Spacing::Panel) * 2.0f);
        const float barHeight = UI::Em(UI::HiveStack::NoFrameBarEm);
        const ImVec2 min = ImGui::GetCursorScreenPos();
        const ImVec2 max(min.x + barWidth, min.y + barHeight);

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const bool excluder = box.boxType == "queen_excluder";

        drawList->AddRectFilled(min, max,
            ImGui::GetColorU32(excluder ? SurfaceRaised : UI::Mix(Surface, Honey, 0.25f)),
            UI::Px(UI::Radius::Frame));

        if (excluder)
        {
            // Grid lines suggest the excluder mesh.
            const float step = UI::Px(10.0f);

            for (float x = min.x + step; x < max.x; x += step)
                drawList->AddLine(ImVec2(x, min.y + 2.0f), ImVec2(x, max.y - 2.0f), ImGui::GetColorU32(Border), 1.0f);
        }

        const std::string text = Tr(excluder ? "boxes.excluder_bar" : "boxes.feeder_bar");
        const ImVec2 size = ImGui::CalcTextSize(text.c_str());

        drawList->AddText(
            ImVec2(min.x + (barWidth - size.x) * 0.5f, min.y + (barHeight - size.y) * 0.5f),
            ImGui::GetColorU32(TextSecondary),
            text.c_str());

        ImGui::Dummy(ImVec2(barWidth, barHeight));
    }

    if (!box.notes.empty())
    {
        ImGui::Spacing();
        UI::TextStyledWrapped(UI::Text::Caption, TextSecondary, box.notes.c_str());
    }

    UI::EndCard();
}

void HiveDetailScreen::DrawFrameTile(const Frame& frame, float width, float height)
{
    using namespace Theme::Colors;

    ImGui::PushID(frame.id);

    const ImVec2 min = ImGui::GetCursorScreenPos();
    const ImVec2 max(min.x + width, min.y + height);

    const bool pressed = ImGui::InvisibleButton("##Frame", ImVec2(width, height));
    const bool hovered = ImGui::IsItemHovered();
    const bool selected = frame.id == m_selectedFrameId;
    const bool due = IsReplacementDue(frame);

    ImVec4 fill = SurfaceRaised;

    if (frame.frameType == "feeder")
        fill = UI::Mix(Surface, Honey, 0.45f);
    else if (frame.frameType == "follower_board")
        fill = UI::Mix(Surface, Theme::Colors::Border, 0.7f);
    else if (frame.frameType == "flow_frame")
        fill = UI::Mix(SurfaceRaised, Honey, 0.12f);

    if (hovered)
        fill = UI::Mix(fill, Sage, 0.35f);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const float rounding = UI::Px(UI::Radius::Frame);

    drawList->AddRectFilled(min, max, ImGui::GetColorU32(fill), rounding);

    if (selected)
        drawList->AddRect(min, max, ImGui::GetColorU32(Honey), rounding, 0, UI::Px(UI::HiveStack::SelectedBorder));
    else if (due)
        drawList->AddRect(min, max, ImGui::GetColorU32(Danger), rounding, 0, UI::Px(UI::HiveStack::AlertBorder));
    else
        drawList->AddRect(min, max, ImGui::GetColorU32(Theme::Colors::Border), rounding, 0, UI::Px(UI::Border::Frame));

    // Number (top), type (middle), comb age (bottom)
    const std::string number = std::to_string(frame.position);
    const std::string type = Display::FrameTypeShort(frame.frameType);
    const std::string age = HiveEquipment::HasComb(frame.frameType) ? Display::CombAgeShort(frame.installedAt) : "";

    ImFont* titleFont = Fonts::Get(UI::Text::Title.face);
    ImFont* captionFont = Fonts::Get(UI::Text::Caption.face);
    const float titleSize = UI::LineHeight(UI::Text::Title);
    const float captionSize = UI::LineHeight(UI::Text::Caption);

    const float pad = UI::Em(0.4f);

    // Centred when it fits; otherwise left-aligned and
    // clipped to the tile (long translations). The tooltip
    // always shows the full text.
    const ImVec4 clip(min.x + pad * 0.5f, min.y, max.x - pad * 0.5f, max.y);

    auto centered = [&](ImFont* font, float size, float y, const std::string& text, const ImVec4& color)
    {
        const ImVec2 textSize = font->CalcTextSizeA(size, FLT_MAX, 0.0f, text.c_str());
        const float x = textSize.x <= width - pad
            ? min.x + (width - textSize.x) * 0.5f
            : clip.x;

        drawList->AddText(font, size, ImVec2(x, y), ImGui::GetColorU32(color),
            text.c_str(), nullptr, 0.0f, &clip);
    };

    centered(titleFont, titleSize, min.y + pad, number, TextPrimary);
    centered(captionFont, captionSize, min.y + (height - captionSize) * 0.5f, type, TextSecondary);

    if (!age.empty())
        centered(captionFont, captionSize, max.y - pad - captionSize, age, due ? Danger : TextMuted);

    // Tooltip with the full details.
    if (ImGui::BeginItemTooltip())
    {
        const std::string title = Localization::Format("frames.tile_tooltip", { { "number", number } });
        UI::TextStyled(UI::Text::Label, TextPrimary, title.c_str());
        ImGui::TextUnformatted(Display::FrameType(frame.frameType).c_str());

        if (!frame.label.empty())
            ImGui::TextUnformatted(frame.label.c_str());

        if (HiveEquipment::HasComb(frame.frameType) && !frame.installedAt.empty())
        {
            const std::string text = Localization::Format("frames.installed_age",
                { { "date", Display::Date(frame.installedAt) }, { "age", Display::CombAge(frame.installedAt) } });
            UI::TextStyled(UI::Text::Caption, TextSecondary, text.c_str());
        }

        if (due)
            UI::TextStyled(UI::Text::Caption, Danger, Tr("frames.legend_due"));

        ImGui::EndTooltip();
    }

    if (pressed && frame.id != m_selectedFrameId)
    {
        if (IsFrameDirty())
        {
            m_pendingFrameId = frame.id;
            m_openDiscardConfirm = true;
        }
        else
        {
            SelectFrame(frame.id);
        }
    }

    ImGui::PopID();
}

void HiveDetailScreen::DrawFramePanel()
{
    using namespace Theme::Colors;

    const Box* box = nullptr;
    const Frame* frame = FindFrame(m_selectedFrameId, &box);

    if (!frame || !box)
    {
        UI::TextStyled(UI::Text::Title, TextPrimary, Tr("frames.panel_heading"));
        ImGui::Spacing();
        UI::TextStyledWrapped(UI::Text::Body, TextSecondary, Tr("frames.select_hint"));
        return;
    }

    const std::string title = Localization::Format(
        "frames.panel_title",
        { { "box", std::to_string(box->position) }, { "frame", std::to_string(frame->position) } }
    );

    UI::TextStyled(UI::Text::Title, TextPrimary, title.c_str());
    UI::TextStyled(UI::Text::Caption, TextMuted, Display::BoxTitle(*box).c_str());
    ImGui::Spacing();

    ImGui::BeginDisabled(IsReadOnly());

    // Type
    UI::FormLabel(Tr("frames.type"));
    FrameTypeCombo("##FrameType", m_draftTypeIndex);

    const bool hasComb = HiveEquipment::FrameTypes()[m_draftTypeIndex].hasComb;

    if (!hasComb)
        UI::FieldHelp(Tr("frames.no_comb_help"));

    ImGui::Spacing();

    // Label
    UI::FormLabel(Tr("frames.label"));
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputTextWithHint("##FrameLabel", Tr("frames.label_hint"), &m_draftLabel);
    ImGui::Spacing();

    // Installed
    UI::FormLabel(Tr("frames.installed"), true);

    const std::string todayLabel = Localization::Label("common.today", "Today");

    if (UI::DateInput("##FrameInstalled", m_draftDateText, Display::DatePattern(), Display::DateHint().c_str(), todayLabel.c_str()))
        m_draftDateError.clear();

    UI::FieldError(m_draftDateError);

    // Comb age
    if (hasComb && HiveEquipment::HasComb(frame->frameType))
    {
        const std::string age = Display::CombAge(frame->installedAt);

        if (!age.empty())
        {
            const std::string text = Localization::Format("frames.comb_age", { { "age", age } });
            UI::TextStyled(UI::Text::Caption, TextSecondary, text.c_str());
        }

        const std::string interval = ReplacementIntervalText();

        if (!interval.empty())
            UI::TextStyled(UI::Text::Caption, TextMuted, interval.c_str());

        if (IsReplacementDue(*frame))
            UI::StatusChip(Tr("frames.legend_due"), Danger);
    }

    ImGui::Spacing();

    // Notes
    UI::FormLabel(Tr("frames.notes"));
    ImGui::InputTextMultiline(
        "##FrameNotes",
        &m_draftNotes,
        ImVec2(-FLT_MIN, UI::Em(4.0f)),
        ImGuiInputTextFlags_WordWrap
    );

    ImGui::Spacing();

    // Actions
    const std::string saveLabel = Localization::Label("frames.save", "SaveFrame");
    const std::string replaceLabel = Localization::Label("frames.replace", "ReplaceFrame");

    ImGui::BeginDisabled(!IsFrameDirty());

    if (UI::Button(saveLabel.c_str(), UI::ButtonKind::Primary))
    {
        std::string installedIso;

        if (Display::ParseDateInput(m_draftDateText, true, Display::FutureDates::Rejected, installedIso, m_draftDateError))
        {
            Frame updated = *frame;
            updated.frameType = HiveEquipment::FrameTypes()[m_draftTypeIndex].key;
            updated.label = m_draftLabel;
            updated.installedAt = installedIso;
            updated.notes = m_draftNotes;

            if (m_boxRepository.UpdateFrame(updated))
            {
                m_notice = Localization::Format("frames.saved_notice", { { "frame", std::to_string(updated.position) } });
                RequestReload();
            }
            else
            {
                m_error = Localization::Format("errors.save_failed", { { "details", m_boxRepository.GetLastError() } });
            }
        }
    }

    ImGui::EndDisabled();

    UI::SameLineOrWrap(UI::ButtonWidth(replaceLabel.c_str()));

    if (UI::Button(replaceLabel.c_str()))
    {
        // Default: same type as now (feeder -> the box's
        // usual frame type).
        m_replaceTypeIndex = HiveEquipment::HasComb(frame->frameType)
            ? FrameTypeIndex(frame->frameType)
            : FrameTypeIndex(HiveEquipment::DefaultFrameType(box->boxType, m_hive.hiveType));
        m_replaceDateText = TodayText();
        m_replaceDateError.clear();
        m_openReplaceFrame = true;
    }

    ImGui::SetItemTooltip("%s", Tr("frames.replace_help"));

    ImGui::EndDisabled();

    if (IsFrameDirty())
        UI::TextStyled(UI::Text::Caption, Warning, Tr("frames.unsaved"));
}

// --------------------------------------------------
// Dialogs
// --------------------------------------------------

void HiveDetailScreen::DrawRemoveBoxDialog()
{
    if (m_openRemoveBox)
    {
        ImGui::OpenPopup(RemoveBoxPopupId);
        m_openRemoveBox = false;
    }

    UI::SetNextDialogLayout(UI::Dialog::ConfirmWidthEm);

    const std::string title = Localization::Label("boxes.remove_dialog.title", "RemoveBoxDialog");

    if (!ImGui::BeginPopupModal(title.c_str(), nullptr, ImGuiWindowFlags_NoSavedSettings))
        return;

    const std::string body = Localization::Format(
        "boxes.remove_dialog.body",
        { { "box", Display::BoxTitle(m_removeCandidate) } }
    );

    UI::TextStyledWrapped(UI::Text::Body, Theme::Colors::TextPrimary, body.c_str());
    ImGui::Spacing();

    UI::FormLabel(Tr("boxes.remove_dialog.date"), true);

    const std::string todayLabel = Localization::Label("common.today", "Today");

    if (UI::DateInput("##RemovedOn", m_removeDateText, Display::DatePattern(), Display::DateHint().c_str(), todayLabel.c_str()))
        m_removeDateError.clear();

    UI::FieldError(m_removeDateError);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    const std::string confirmLabel = Localization::Label("boxes.remove_dialog.confirm", "Confirm");
    const std::string cancelLabel = Localization::Label("common.cancel", "Cancel");

    if (UI::Button(confirmLabel.c_str(), UI::ButtonKind::Danger))
    {
        std::string removedIso;

        if (Display::ParseDateInput(m_removeDateText, true, Display::FutureDates::Rejected, removedIso, m_removeDateError))
        {
            if (m_boxRepository.RemoveBox(m_removeCandidate.id, removedIso))
            {
                m_notice = Localization::Format("boxes.removed_notice", { { "box", Display::BoxTitle(m_removeCandidate) } });
                RequestReload();
            }
            else
            {
                m_error = Localization::Format("errors.save_failed", { { "details", m_boxRepository.GetLastError() } });
            }

            ImGui::CloseCurrentPopup();
        }
    }

    UI::SameLineOrWrap(UI::ButtonWidth(cancelLabel.c_str()));

    if (UI::Button(cancelLabel.c_str()) || ImGui::IsKeyPressed(ImGuiKey_Escape, false))
        ImGui::CloseCurrentPopup();

    ImGui::EndPopup();
}

void HiveDetailScreen::DrawReplaceFrameDialog()
{
    if (m_openReplaceFrame)
    {
        ImGui::OpenPopup(ReplaceFramePopupId);
        m_openReplaceFrame = false;
    }

    UI::SetNextDialogLayout(UI::Dialog::ConfirmWidthEm);

    const std::string title = Localization::Label("frames.replace_dialog.title", "ReplaceFrameDialog");

    if (!ImGui::BeginPopupModal(title.c_str(), nullptr, ImGuiWindowFlags_NoSavedSettings))
        return;

    const Box* box = nullptr;
    const Frame* frame = FindFrame(m_selectedFrameId, &box);

    if (!frame || !box)
    {
        ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
        return;
    }

    const std::string body = Localization::Format(
        "frames.replace_dialog.body",
        { { "frame", std::to_string(frame->position) }, { "box", std::to_string(box->position) } }
    );

    UI::TextStyledWrapped(UI::Text::Body, Theme::Colors::TextPrimary, body.c_str());
    ImGui::Spacing();

    UI::FormLabel(Tr("frames.replace_dialog.new_type"));
    FrameTypeCombo("##NewFrameType", m_replaceTypeIndex);
    ImGui::Spacing();

    UI::FormLabel(Tr("frames.installed"), true);

    const std::string todayLabel = Localization::Label("common.today", "Today");

    if (UI::DateInput("##ReplacedOn", m_replaceDateText, Display::DatePattern(), Display::DateHint().c_str(), todayLabel.c_str()))
        m_replaceDateError.clear();

    UI::FieldError(m_replaceDateError);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    const std::string confirmLabel = Localization::Label("frames.replace_dialog.confirm", "Confirm");
    const std::string cancelLabel = Localization::Label("common.cancel", "Cancel");

    if (UI::Button(confirmLabel.c_str(), UI::ButtonKind::Primary))
    {
        std::string dateIso;

        if (Display::ParseDateInput(m_replaceDateText, true, Display::FutureDates::Rejected, dateIso, m_replaceDateError))
        {
            Frame replacement;
            const int position = frame->position;

            if (m_boxRepository.ReplaceFrame(frame->id, HiveEquipment::FrameTypes()[m_replaceTypeIndex].key, dateIso, replacement))
            {
                m_selectedFrameId = replacement.id;
                m_notice = Localization::Format("frames.replaced_notice", { { "frame", std::to_string(position) } });
                RequestReload();
            }
            else
            {
                m_error = Localization::Format("errors.save_failed", { { "details", m_boxRepository.GetLastError() } });
            }

            ImGui::CloseCurrentPopup();
        }
    }

    UI::SameLineOrWrap(UI::ButtonWidth(cancelLabel.c_str()));

    if (UI::Button(cancelLabel.c_str()) || ImGui::IsKeyPressed(ImGuiKey_Escape, false))
        ImGui::CloseCurrentPopup();

    ImGui::EndPopup();
}

void HiveDetailScreen::DrawDiscardFrameDialog()
{
    if (m_openDiscardConfirm)
    {
        ImGui::OpenPopup(DiscardFramePopupId);
        m_openDiscardConfirm = false;
    }

    UI::SetNextDialogLayout(UI::Dialog::ConfirmWidthEm);

    const std::string title = Localization::Label("frames.discard_dialog.title", "DiscardFrameDialog");

    if (!ImGui::BeginPopupModal(title.c_str(), nullptr, ImGuiWindowFlags_NoSavedSettings))
        return;

    UI::TextStyledWrapped(UI::Text::Body, Theme::Colors::TextPrimary, Tr("frames.discard_dialog.body"));

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    const std::string discardLabel = Localization::Label("frames.discard_dialog.discard", "Discard");
    const std::string stayLabel = Localization::Label("frames.discard_dialog.stay", "Stay");

    if (UI::Button(discardLabel.c_str(), UI::ButtonKind::Danger))
    {
        SelectFrame(m_pendingFrameId);
        m_pendingFrameId = 0;
        ImGui::CloseCurrentPopup();
    }

    UI::SameLineOrWrap(UI::ButtonWidth(stayLabel.c_str()));

    if (UI::Button(stayLabel.c_str()) || ImGui::IsKeyPressed(ImGuiKey_Escape, false))
    {
        m_pendingFrameId = 0;
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}
