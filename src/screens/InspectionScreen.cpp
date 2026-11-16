
#include "InspectionScreen.h"

#include "config/LayoutUI.h"
#include "config/Theme.h"
#include "localization/DateFormat.h"
#include "localization/Localization.h"
#include "models/HiveEquipment.h"
#include "models/InspectionOptions.h"
#include "screens/MaintenanceUI.h"
#include "ui/Display.h"
#include "ui/Widgets.h"

#include "imgui.h"
#include "imgui_stdlib.h"

#include <algorithm>
#include <cstdio>

namespace
{
    constexpr const char* DeletePopupId = "###DeleteInspectionDialog";

    enum Tab
    {
        TabFrames = 0,
        TabColony = 1,
        TabSummary = 2
    };

    std::string Percent(int value)
    {
        return Localization::Format("inspection.percent", { { "value", std::to_string(value) } });
    }

    std::string FramesCount(double frames)
    {
        char buffer[32];
        std::snprintf(buffer, sizeof(buffer), "%g", frames);
        return Localization::Format("inspection.frames_of", { { "count", buffer } });
    }

    // Stacked coverage bar: brood, honey, pollen, empty.
    void CoverageBar(ImDrawList* drawList, ImVec2 min, ImVec2 max, const FrameObservation& o)
    {
        using namespace Theme::Colors;

        const float width = max.x - min.x;
        float x = min.x;

        const std::pair<int, ImVec4> parts[] =
        {
            { o.broodPct, Brood }, { o.honeyPct, Honey }, { o.pollenPct, Pollen }, { o.EmptyPct(), Background }
        };

        for (const auto& [pct, color] : parts)
        {
            if (pct <= 0)
                continue;

            const float w = width * pct / 100.0f;
            drawList->AddRectFilled(ImVec2(x, min.y), ImVec2(x + w, max.y), ImGui::GetColorU32(color));
            x += w;
        }

        drawList->AddRect(min, max, ImGui::GetColorU32(Theme::Colors::Border));
    }

    // Option list -> translated labels for SegmentedControl.
    struct Labels
    {
        std::vector<std::string> text;
        std::vector<const char*> pointers;

        explicit Labels(const std::vector<InspectionOptions::Option>& options)
        {
            for (const auto& option : options)
                text.push_back(Tr(option.labelKey));
            for (const auto& t : text)
                pointers.push_back(t.c_str());
        }
    };

    // Segmented choice bound to a key ("" = not recorded).
    bool ChoiceRow(const char* id, const char* label, const std::vector<InspectionOptions::Option>& options, std::string& key)
    {
        UI::FormLabel(label);

        Labels labels(options);
        int index = InspectionOptions::IndexOf(options, key);

        if (UI::SegmentedControl(id, labels.pointers.data(), static_cast<int>(options.size()), index, 0.0f, true))
        {
            key = index >= 0 ? options[index].key : "";
            return true;
        }

        return false;
    }
}

InspectionScreen::InspectionScreen(
    InspectionRepository& inspectionRepository,
    BoxRepository& boxRepository,
    MiteRepository& miteRepository,
    const SettingsData& settings)
    : m_inspectionRepository(inspectionRepository),
      m_boxRepository(boxRepository),
      m_miteRepository(miteRepository),
      m_settings(settings),
      m_miteDialog(miteRepository, settings)
{
}

// --------------------------------------------------
// Data
// --------------------------------------------------

bool InspectionScreen::Open(int inspectionId, const Hive& hive, bool readOnly)
{
    m_hive = hive;
    m_readOnly = readOnly;
    m_error.clear();
    m_selectedFrameId = 0;
    m_requestTab = TabFrames;

    Load(inspectionId);

    if (m_inspection.id == 0)
        return false;

    // Start with the first frame that carries comb.
    for (const auto& [box, frame] : FrameOrder())
    {
        if (HiveEquipment::HasComb(frame->frameType))
        {
            m_selectedFrameId = frame->id;
            break;
        }
    }

    return true;
}

void InspectionScreen::Load(int inspectionId)
{
    if (!m_inspectionRepository.GetById(inspectionId, m_inspection))
    {
        m_error = Localization::Format("errors.load_failed", { { "details", m_inspectionRepository.GetLastError() } });
        m_inspection = Inspection();
        return;
    }

    if (!m_boxRepository.GetStack(m_hive.id, m_stack))
        m_error = Localization::Format("errors.load_failed", { { "details", m_boxRepository.GetLastError() } });

    m_dateText = Display::Date(m_inspection.date);
    m_timeText = Display::Time(m_inspection.time);
    m_temperature = Display::CelsiusToDisplay(m_inspection.temperatureC);
    m_dateError.clear();
    m_timeError.clear();

    InspectionSummary previous;
    m_hasPrevious = m_inspectionRepository.LatestCompleted(m_hive.id, previous) && previous.id != m_inspection.id;
    m_previousDate = m_hasPrevious ? Display::Date(previous.date) : "";

    LoadMiteChecks();
}

void InspectionScreen::LoadMiteChecks()
{
    m_miteChecks.clear();

    std::vector<MiteCheck> all;

    if (!m_miteRepository.ListChecks(m_hive.id, all))
    {
        m_error = Localization::Format("errors.load_failed", { { "details", m_miteRepository.GetLastError() } });
        return;
    }

    for (const MiteCheck& check : all)
    {
        if (check.inspectionId == m_inspection.id)
            m_miteChecks.push_back(check);
    }
}

std::vector<std::pair<const Box*, const Frame*>> InspectionScreen::FrameOrder() const
{
    std::vector<std::pair<const Box*, const Frame*>> order;

    for (auto box = m_stack.rbegin(); box != m_stack.rend(); ++box)
    {
        for (const Frame& frame : box->frames)
            order.emplace_back(&*box, &frame);
    }

    return order;
}

const FrameObservation* InspectionScreen::FindObservation(int frameId) const
{
    for (const FrameObservation& o : m_inspection.frames)
    {
        if (o.frameId == frameId)
            return &o;
    }

    return nullptr;
}

FrameObservation InspectionScreen::ObservationFor(const Box& box, const Frame& frame) const
{
    if (const FrameObservation* existing = FindObservation(frame.id))
        return *existing;

    FrameObservation fresh;
    fresh.inspectionId = m_inspection.id;
    fresh.frameId = frame.id;
    fresh.boxId = box.id;
    fresh.framePosition = frame.position;
    return fresh;
}

void InspectionScreen::StoreObservation(FrameObservation observation)
{
    if (!m_inspectionRepository.SaveObservation(observation))
    {
        m_error = Localization::Format("errors.save_failed", { { "details", m_inspectionRepository.GetLastError() } });
        return;
    }

    for (FrameObservation& o : m_inspection.frames)
    {
        if (o.frameId == observation.frameId)
        {
            o = observation;
            return;
        }
    }

    m_inspection.frames.push_back(observation);
}

void InspectionScreen::ClearObservation(int frameId)
{
    if (!m_inspectionRepository.DeleteObservation(m_inspection.id, frameId))
    {
        m_error = Localization::Format("errors.save_failed", { { "details", m_inspectionRepository.GetLastError() } });
        return;
    }

    auto& frames = m_inspection.frames;
    frames.erase(std::remove_if(frames.begin(), frames.end(),
        [frameId](const FrameObservation& o) { return o.frameId == frameId; }), frames.end());
}

void InspectionScreen::SaveHeader()
{
    if (!m_inspectionRepository.UpdateHeader(m_inspection))
        m_error = Localization::Format("errors.save_failed", { { "details", m_inspectionRepository.GetLastError() } });
}

void InspectionScreen::SaveFindings()
{
    if (!m_inspectionRepository.SetFindings(m_inspection.id, m_inspection.findings))
        m_error = Localization::Format("errors.save_failed", { { "details", m_inspectionRepository.GetLastError() } });
}

bool InspectionScreen::ValidateForFinish()
{
    std::string iso;

    if (!Display::ParseDateInput(m_dateText, true, Display::FutureDates::Rejected, iso, m_dateError))
        return false;

    std::string time;

    return Display::ParseTimeInput(m_timeText, time, m_timeError);
}

// --------------------------------------------------
// Frame
// --------------------------------------------------

InspectionScreen::Result InspectionScreen::Render()
{
    ImGui::PushID("Inspection");

    Result result = DrawHeader();

    if (!m_error.empty() && UI::Banner("InspectionError", m_error, UI::BannerKind::Error, Tr("common.dismiss")))
        m_error.clear();

    const std::string framesLabel = Localization::Label("inspection.tab_frames", "TabFrames");
    const std::string colonyLabel = Localization::Label("inspection.tab_colony", "TabColony");
    const std::string summaryLabel = Localization::Label("inspection.tab_summary", "TabSummary");

    if (ImGui::BeginTabBar("InspectionTabs"))
    {
        auto flags = [this](int tab)
        {
            return m_requestTab == tab ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
        };

        if (ImGui::BeginTabItem(framesLabel.c_str(), nullptr, flags(TabFrames)))
        {
            ImGui::Spacing();
            DrawFramesTab();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem(colonyLabel.c_str(), nullptr, flags(TabColony)))
        {
            ImGui::Spacing();
            DrawColonyTab();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem(summaryLabel.c_str(), nullptr, flags(TabSummary)))
        {
            ImGui::Spacing();
            DrawSummaryTab();
            ImGui::EndTabItem();
        }

        // Narrow windows: frame map toggle sits in the tab bar
        // (saves a row for the frame editor).
        if (m_narrow)
        {
            const std::string mapLabel = Localization::Label(m_showMap ? "inspection.hide_map" : "inspection.show_map", "FrameMap");

            if (ImGui::TabItemButton(mapLabel.c_str(), ImGuiTabItemFlags_Trailing))
                m_showMap = !m_showMap;
        }

        m_requestTab = -1;
        ImGui::EndTabBar();
    }

    DrawDeleteDialog(result);

    if (m_miteDialog.Draw() != MiteCheckDialog::Result::None)
        LoadMiteChecks();

    ImGui::PopID();
    return result;
}

InspectionScreen::Result InspectionScreen::DrawHeader()
{
    using namespace Theme::Colors;

    Result result = Result::None;

    ImGui::BeginGroup();

    const std::string title = Localization::Format(
        "inspection.title", { { "date", m_dateText.empty() ? Display::Date(m_inspection.date) : m_dateText } });

    // Short windows: smaller title keeps room for the frame editor.
    const bool compact = ImGui::GetContentRegionAvail().y < UI::Em(UI::Inspection::CompactHeightEm);

    UI::TextStyled(compact ? UI::Text::Heading : UI::Text::Display, TextPrimary, title.c_str());

    if (compact)
        ImGui::SameLine();

    if (m_inspection.completed)
        UI::StatusChip(Tr("inspection.status_completed"), Success);
    else
        UI::StatusChip(Tr("inspection.status_draft"), Warning);

    if (!compact || m_readOnly)
    {
        ImGui::SameLine();
        UI::TextStyled(UI::Text::Caption, TextMuted, Tr(m_readOnly ? "inspection.read_only" : "inspection.autosave"));
    }

    ImGui::EndGroup();

    // Actions (right-aligned, wrap when narrow)
    const std::string deleteLabel = Localization::Label("inspection.delete", "DeleteInspection");
    const std::string closeLabel = Localization::Label(m_inspection.completed ? "inspection.close" : "inspection.continue_later", "CloseInspection");
    const std::string finishLabel = Localization::Label("inspection.finish", "FinishInspection");

    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    float width = UI::ButtonWidth(closeLabel.c_str());

    if (!m_readOnly)
        width += UI::ButtonWidth(deleteLabel.c_str()) + spacing;

    if (!m_readOnly && !m_inspection.completed)
        width += UI::ButtonWidth(finishLabel.c_str()) + spacing;

    UI::SameLineOrWrap(width);

    const float free = ImGui::GetContentRegionAvail().x - width;

    if (free > 0.0f)
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + free);

    if (!m_readOnly)
    {
        if (UI::Button(deleteLabel.c_str(), UI::ButtonKind::Danger))
            m_openDelete = true;

        ImGui::SameLine();
    }

    if (UI::Button(closeLabel.c_str()))
        result = Result::Closed;

    if (!m_readOnly && !m_inspection.completed)
    {
        ImGui::SameLine();

        if (UI::Button(finishLabel.c_str(), UI::ButtonKind::Primary))
        {
            if (!ValidateForFinish())
            {
                m_requestTab = TabColony;
                m_error = Tr("inspection.fix_before_finish");
            }
            else if (m_inspectionRepository.Complete(m_inspection.id))
            {
                m_inspection.completed = true;
                result = Result::Finished;
            }
            else
            {
                m_error = Localization::Format("errors.save_failed", { { "details", m_inspectionRepository.GetLastError() } });
            }
        }
    }

    ImGui::Spacing();
    return result;
}

// --------------------------------------------------
// Frames tab
// --------------------------------------------------

void InspectionScreen::DrawFramesTab()
{
    using namespace Theme::Colors;

    if (m_stack.empty())
    {
        UI::TextStyledWrapped(UI::Text::Body, TextSecondary, Tr("inspection.no_boxes"));
        return;
    }

    // Start from last inspection (only before any frame is recorded).
    if (!m_readOnly && m_hasPrevious && m_inspection.frames.empty())
    {
        const std::string copyLabel = Localization::WithId(
            Localization::Format("inspection.copy_last", { { "date", m_previousDate } }), "CopyLast");

        if (UI::Button(copyLabel.c_str()))
        {
            int copied = 0;

            if (m_inspectionRepository.CopyFromLatest(m_inspection.id, m_hive.id, copied))
                Load(m_inspection.id);
            else
                m_error = Localization::Format("errors.save_failed", { { "details", m_inspectionRepository.GetLastError() } });
        }

        ImGui::SetItemTooltip("%s", Tr("inspection.copy_last_help"));
        ImGui::Spacing();
    }

    const bool sideBySide = ImGui::GetContentRegionAvail().x >= UI::Em(UI::HiveStack::SideBySideMinEm);
    const float gutter = UI::Px(UI::Spacing::Gutter);

    // Narrow windows: the frame map is optional (Previous /
    // Next already walk through the frames), so the editor
    // gets the height. Its toggle is in the tab bar.
    m_narrow = !sideBySide;

    const ImVec2 available = ImGui::GetContentRegionAvail();

    if (sideBySide || m_showMap)
    {
        const ImVec2 navigatorSize = sideBySide
            ? ImVec2(std::min(UI::Em(UI::Inspection::NavigatorWidthEm), available.x * 0.42f), 0.0f)
            : ImVec2(0.0f, std::max(available.y * UI::Inspection::MapHeightRatio, UI::Em(6.0f)));

        UI::BeginScrollPane("Navigator", navigatorSize, UI::Em(UI::Inspection::NavigatorMinEm));
        DrawNavigator();
        UI::EndScrollPane();

        if (sideBySide)
            ImGui::SameLine(0.0f, gutter);
    }

    // Editor: the frame's fields scroll, the Previous / Next
    // bar stays pinned at the bottom (no scrolling between
    // frames in the field).
    ImGui::BeginChild("EditorArea", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    if (m_navigationHeight <= 0.0f)
        m_navigationHeight = UI::Px(UI::Size::TouchTarget) + ImGui::GetStyle().ItemSpacing.y * 3.0f;

    UI::BeginScrollPane("FrameEditor", ImVec2(0.0f, -m_navigationHeight), UI::Em(UI::Inspection::EditorMinEm), ImGuiChildFlags_None);
    DrawFrameEditor();
    UI::EndScrollPane();

    const float start = ImGui::GetCursorPosY();
    DrawFrameNavigation();
    m_navigationHeight = ImGui::GetCursorPosY() - start;

    ImGui::EndChild();
}

std::vector<std::pair<const Box*, const Frame*>> InspectionScreen::CombFrames(int& selectedIndex) const
{
    std::vector<std::pair<const Box*, const Frame*>> frames;
    selectedIndex = -1;

    for (const auto& entry : FrameOrder())
    {
        if (!HiveEquipment::HasComb(entry.second->frameType))
            continue;

        if (entry.second->id == m_selectedFrameId)
            selectedIndex = static_cast<int>(frames.size());

        frames.push_back(entry);
    }

    return frames;
}

void InspectionScreen::DrawFrameNavigation()
{
    int index = -1;
    const auto frames = CombFrames(index);

    ImGui::Separator();
    ImGui::Spacing();

    if (index < 0)
        return;

    const float navHeight = UI::Px(UI::Size::TouchTarget);
    const std::string previousLabel = Localization::Label("inspection.previous_frame", "PreviousFrame");
    const std::string nextLabel = Localization::Label("inspection.next_frame", "NextFrame");

    ImGui::BeginDisabled(index == 0);

    if (UI::Button(previousLabel.c_str(), UI::ButtonKind::Secondary, navHeight))
        m_selectedFrameId = frames[index - 1].second->id;

    ImGui::EndDisabled();
    UI::SameLineOrWrap(UI::ButtonWidth(nextLabel.c_str()));
    ImGui::BeginDisabled(index + 1 >= static_cast<int>(frames.size()));

    if (UI::Button(nextLabel.c_str(), UI::ButtonKind::Primary, navHeight))
        m_selectedFrameId = frames[index + 1].second->id;

    ImGui::EndDisabled();

    const int frameId = frames[index].second->id;

    if (FindObservation(frameId) && !m_readOnly)
    {
        const std::string clearLabel = Localization::Label("inspection.clear_frame", "ClearFrame");
        UI::SameLineOrWrap(UI::ButtonWidth(clearLabel.c_str()));

        if (UI::Button(clearLabel.c_str(), UI::ButtonKind::Secondary, navHeight))
            ClearObservation(frameId);

        ImGui::SetItemTooltip("%s", Tr("inspection.clear_frame_help"));
    }
}

void InspectionScreen::DrawNavigator()
{
    using namespace Theme::Colors;

    const float tile = UI::Px(UI::Size::TouchTarget);
    const float tileHeight = UI::Em(UI::Inspection::TileHeightEm);
    const float gap = UI::Px(UI::HiveStack::FrameGap);
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // Progress: frames with a record / frames with comb.
    int recordable = 0;

    for (const auto& [box, frame] : FrameOrder())
        recordable += HiveEquipment::HasComb(frame->frameType) ? 1 : 0;

    const std::string progress = Localization::Format(
        "inspection.progress",
        { { "done", std::to_string(m_inspection.frames.size()) }, { "total", std::to_string(recordable) } });

    UI::TextStyled(UI::Text::Label, TextSecondary, progress.c_str());
    ImGui::Spacing();

    for (auto box = m_stack.rbegin(); box != m_stack.rend(); ++box)
    {
        ImGui::PushID(box->id);
        UI::TextStyled(UI::Text::Caption, TextSecondary, Display::BoxTitle(*box).c_str());

        if (box->frames.empty())
        {
            UI::TextStyled(UI::Text::Caption, TextMuted, Tr("boxes.dialog.no_frames"));
        }

        // Tiles wrap inside the navigator.
        for (size_t i = 0; i < box->frames.size(); ++i)
        {
            const Frame& frame = box->frames[i];

            if (i > 0)
                UI::SameLineOrWrap(tile);

            ImGui::PushID(frame.id);

            const bool hasComb = HiveEquipment::HasComb(frame.frameType);
            const FrameObservation* observation = FindObservation(frame.id);
            const bool selected = frame.id == m_selectedFrameId;

            const ImVec2 min = ImGui::GetCursorScreenPos();
            const ImVec2 max(min.x + tile, min.y + tileHeight);

            ImGui::BeginDisabled(!hasComb);
            const bool pressed = ImGui::InvisibleButton("##NavFrame", ImVec2(tile, tileHeight));
            const bool hovered = ImGui::IsItemHovered();
            ImGui::EndDisabled();

            ImVec4 fill = hasComb ? SurfaceRaised : UI::Mix(Surface, Honey, 0.25f);

            if (hovered && hasComb)
                fill = UI::Mix(fill, Sage, 0.35f);

            const float rounding = UI::Px(UI::Radius::Frame);
            drawList->AddRectFilled(min, max, ImGui::GetColorU32(fill), rounding);
            drawList->AddRect(min, max, ImGui::GetColorU32(selected ? Honey : Theme::Colors::Border), rounding, 0,
                UI::Px(selected ? UI::HiveStack::SelectedBorder : UI::Border::Frame));

            const std::string number = std::to_string(frame.position);
            const ImVec2 size = ImGui::CalcTextSize(number.c_str());
            drawList->AddText(ImVec2(min.x + (tile - size.x) * 0.5f, min.y + UI::Em(0.3f)),
                ImGui::GetColorU32(hasComb ? TextPrimary : TextMuted), number.c_str());

            if (observation)
            {
                const float pad = UI::Em(0.35f);
                CoverageBar(drawList, ImVec2(min.x + pad, max.y - pad - UI::Em(0.6f)), ImVec2(max.x - pad, max.y - pad), *observation);
            }
            else if (!hasComb)
            {
                const std::string type = Display::FrameTypeShort(frame.frameType);
                const ImVec2 typeSize = ImGui::CalcTextSize(type.c_str());
                const ImVec4 clip(min.x + 2.0f, min.y, max.x - 2.0f, max.y);
                drawList->AddText(nullptr, 0.0f, ImVec2(min.x + std::max(2.0f, (tile - typeSize.x) * 0.5f), max.y - UI::Em(1.3f)),
                    ImGui::GetColorU32(TextMuted), type.c_str(), nullptr, 0.0f, &clip);
            }

            if (pressed && hasComb)
                m_selectedFrameId = frame.id;

            ImGui::PopID();
        }

        ImGui::Dummy(ImVec2(0.0f, gap));
        ImGui::PopID();
    }
}

void InspectionScreen::DrawFrameEditor()
{
    using namespace Theme::Colors;

    int index = -1;
    const auto frames = CombFrames(index);

    if (index < 0)
    {
        UI::TextStyledWrapped(UI::Text::Body, TextSecondary, Tr("inspection.select_frame"));
        return;
    }

    const Box& box = *frames[index].first;
    const Frame& frame = *frames[index].second;

    // A different frame starts at the top of the editor.
    if (frame.id != m_editorFrameId)
    {
        ImGui::SetScrollY(0.0f);
        m_editorFrameId = frame.id;
    }

    FrameObservation o = ObservationFor(box, frame);
    const FrameObservation before = o;
    const bool recorded = FindObservation(frame.id) != nullptr;

    const std::string title = Localization::Format(
        "frames.panel_title", { { "box", std::to_string(box.position) }, { "frame", std::to_string(frame.position) } });

    UI::TextStyled(UI::Text::Heading, TextPrimary, title.c_str());

    const std::string subtitle = Display::BoxTitle(box) + "  \xC2\xB7  " + Display::FrameType(frame.frameType);
    UI::TextStyled(UI::Text::Caption, TextMuted, subtitle.c_str());

    if (!recorded)
        UI::TextStyled(UI::Text::Caption, TextSecondary, Tr("inspection.not_recorded"));

    ImGui::Spacing();

    ImGui::BeginDisabled(m_readOnly);

    // ---------------- Coverage ----------------
    UI::TextStyled(UI::Text::Title, TextPrimary, Tr("inspection.coverage"));
    UI::FieldHelp(Tr("inspection.coverage_help"));
    ImGui::Spacing();

    std::vector<std::string> stepText;
    std::vector<const char*> stepLabels;

    for (int step : InspectionOptions::CoverageSteps)
        stepText.push_back(Percent(step));
    for (const auto& t : stepText)
        stepLabels.push_back(t.c_str());

    const float labelWidth = UI::Em(UI::Inspection::CoverageLabelEm);

    auto coverageRow = [&](const char* id, const char* label, const ImVec4& color, int& value, int others)
    {
        ImGui::PushID(id);

        // Colour swatch + label, fixed width so rows line up.
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        const float rowHeight = UI::Px(UI::Size::TouchTarget);
        const float swatch = UI::Em(0.9f);

        ImGui::GetWindowDrawList()->AddRectFilled(
            ImVec2(pos.x, pos.y + (rowHeight - swatch) * 0.5f),
            ImVec2(pos.x + swatch, pos.y + (rowHeight + swatch) * 0.5f),
            ImGui::GetColorU32(color), UI::Px(3.0f));

        ImGui::SetCursorScreenPos(ImVec2(pos.x + swatch + UI::Em(0.5f), pos.y + (rowHeight - ImGui::GetTextLineHeight()) * 0.5f));
        UI::TextStyled(UI::Text::Label, TextPrimary, label);

        ImGui::SetCursorScreenPos(ImVec2(pos.x + labelWidth, pos.y));

        bool enabled[InspectionOptions::CoverageStepCount];
        int selected = 0;

        for (int i = 0; i < InspectionOptions::CoverageStepCount; ++i)
        {
            enabled[i] = InspectionOptions::CoverageSteps[i] + others <= 100;

            if (InspectionOptions::CoverageSteps[i] == value)
                selected = i;
        }

        if (UI::SegmentedControl("Steps", stepLabels.data(), InspectionOptions::CoverageStepCount, selected, 0.0f, false, enabled))
            value = InspectionOptions::CoverageSteps[selected];

        ImGui::PopID();
    };

    coverageRow("Brood", Tr("inspection.brood"), Brood, o.broodPct, o.honeyPct + o.pollenPct);
    coverageRow("Honey", Tr("inspection.honey"), Honey, o.honeyPct, o.broodPct + o.pollenPct);
    coverageRow("Pollen", Tr("inspection.pollen"), Pollen, o.pollenPct, o.broodPct + o.honeyPct);

    // Empty = the rest
    {
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        const float swatch = UI::Em(0.9f);

        ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(pos.x, pos.y + UI::Em(0.15f)),
            ImVec2(pos.x + swatch, pos.y + UI::Em(0.15f) + swatch), ImGui::GetColorU32(Background), UI::Px(3.0f));
        ImGui::GetWindowDrawList()->AddRect(ImVec2(pos.x, pos.y + UI::Em(0.15f)),
            ImVec2(pos.x + swatch, pos.y + UI::Em(0.15f) + swatch), ImGui::GetColorU32(Theme::Colors::Border), UI::Px(3.0f));

        ImGui::SetCursorScreenPos(ImVec2(pos.x + swatch + UI::Em(0.5f), pos.y));

        const std::string empty = Localization::Format("inspection.empty_value", { { "value", Percent(o.EmptyPct()) } });
        UI::TextStyled(UI::Text::Label, TextPrimary, empty.c_str());
    }

    // Preview bar
    {
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        const float width = std::min(ImGui::GetContentRegionAvail().x, UI::Em(26.0f));
        CoverageBar(ImGui::GetWindowDrawList(), pos, ImVec2(pos.x + width, pos.y + UI::Em(0.8f)), o);
        ImGui::Dummy(ImVec2(width, UI::Em(0.8f)));
    }

    ImGui::Spacing();

    // ---------------- Seen on this frame ----------------
    UI::TextStyled(UI::Text::Title, TextPrimary, Tr("inspection.seen"));
    ImGui::Spacing();

    struct Flag { const char* key; bool* value; };
    const Flag seen[] =
    {
        { "inspection.eggs", &o.eggs }, { "inspection.larvae", &o.larvae }, { "inspection.capped_brood", &o.cappedBrood },
        { "inspection.drone_brood", &o.droneBrood }, { "inspection.queen_seen", &o.queenSeen },
    };

    for (size_t i = 0; i < sizeof(seen) / sizeof(seen[0]); ++i)
    {
        const std::string label = Localization::Label(seen[i].key, seen[i].key);

        if (i > 0)
            UI::SameLineOrWrap(UI::ButtonWidth(label.c_str()));

        UI::ToggleButton(label.c_str(), *seen[i].value, Success);
    }

    ImGui::Spacing();
    UI::FormLabel(Tr("inspection.queen_cells"));

    const std::string swarm = Localization::Label("inspection.swarm_cells", "Swarm");
    const std::string supersedure = Localization::Label("inspection.supersedure_cells", "Supersedure");

    UI::ToggleButton(swarm.c_str(), o.swarmCells, Warning);
    UI::SameLineOrWrap(UI::ButtonWidth(supersedure.c_str()));
    UI::ToggleButton(supersedure.c_str(), o.supersedureCells, Warning);

    ImGui::Spacing();

    // ---------------- Comb ----------------
    {
        std::string comb = o.combDrawn;

        if (ChoiceRow("Comb", Tr("inspection.comb_label"), InspectionOptions::CombDrawn(), comb))
            o.combDrawn = comb;
    }

    ImGui::Spacing();

    // ---------------- Notes ----------------
    UI::FormLabel(Tr("frames.notes"));
    ImGui::InputTextMultiline("##FrameNotes", &o.notes, ImVec2(-FLT_MIN, UI::Em(4.0f)), ImGuiInputTextFlags_WordWrap);

    ImGui::EndDisabled();

    // Save as soon as anything changed.
    if (!m_readOnly && !(o == before))
        StoreObservation(o);
}

// --------------------------------------------------
// Colony & weather tab
// --------------------------------------------------

void InspectionScreen::DrawColonyTab()
{
    using namespace Theme::Colors;

    UI::BeginScrollPane("Colony", ImVec2(0.0f, 0.0f), UI::Em(UI::Inspection::EditorMinEm), ImGuiChildFlags_None);

    const float width = std::min(ImGui::GetContentRegionAvail().x, UI::Em(UI::SettingsView::ContentMaxWidthEm));

    ImGui::PushStyleColor(ImGuiCol_ChildBg, UI::WithAlpha(Surface, 0.0f));
    ImGui::BeginChild("ColonyBody", ImVec2(width, 0.0f), ImGuiChildFlags_AutoResizeY);
    ImGui::PopStyleColor();

    ImGui::BeginDisabled(m_readOnly);

    bool headerChanged = false;

    // Date (required) and time
    UI::FormLabel(Tr("inspection.date"), true);

    const std::string todayLabel = Localization::Label("common.today", "Today");

    if (UI::DateInput("##InspectionDate", m_dateText, Display::DatePattern(), Display::DateHint().c_str(), todayLabel.c_str()))
    {
        std::string iso;

        if (Display::ParseDateInput(m_dateText, true, Display::FutureDates::Rejected, iso, m_dateError))
        {
            m_inspection.date = iso;
            headerChanged = true;
        }
    }

    UI::FieldError(m_dateError);
    ImGui::Spacing();

    UI::FormLabel(Tr("inspection.time"));

    const std::string nowLabel = Localization::Label("inspection.now", "Now");
    const std::string timeHint = Display::TimeHint();

    if (UI::TimeInput("##InspectionTime", m_timeText, timeHint.c_str(), nowLabel.c_str(), Display::TimeNow()))
    {
        std::string storage;

        if (Display::ParseTimeInput(m_timeText, storage, m_timeError))
        {
            m_inspection.time = storage;
            headerChanged = true;
        }
    }

    UI::FieldError(m_timeError);
    ImGui::Spacing();

    // Temperature in the keeper's unit, stored as Celsius
    UI::FormLabel(Tr("inspection.temperature"));
    ImGui::SetNextItemWidth(UI::Em(UI::Controls::NumberFieldEm));

    if (UI::OptionalNumberInput("##Temperature", m_temperature, "-", true))
    {
        m_inspection.temperatureC = Display::DisplayToCelsius(m_temperature);
        headerChanged = true;
    }

    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(Display::TemperatureSymbol());
    ImGui::Spacing();

    // Choices (tap again to clear)
    UI::FieldHelp(Tr("inspection.choice_help"));
    ImGui::Spacing();

    headerChanged = ChoiceRow("Weather", Tr("inspection.weather_label"), InspectionOptions::Weather(), m_inspection.weather) || headerChanged;
    ImGui::Spacing();
    headerChanged = ChoiceRow("Temperament", Tr("inspection.temperament_label"), InspectionOptions::Temperament(), m_inspection.temperament) || headerChanged;
    ImGui::Spacing();
    headerChanged = ChoiceRow("Queen", Tr("inspection.queen_label"), InspectionOptions::QueenStatus(), m_inspection.queenStatus) || headerChanged;
    ImGui::Spacing();
    headerChanged = ChoiceRow("Strength", Tr("inspection.strength_label"), InspectionOptions::Strength(), m_inspection.strength) || headerChanged;
    ImGui::Spacing();
    headerChanged = ChoiceRow("Stores", Tr("inspection.stores_label"), InspectionOptions::Stores(), m_inspection.stores) || headerChanged;
    ImGui::Spacing();

    // Pest / disease signs
    UI::FormLabel(Tr("inspection.findings_label"));
    UI::FieldHelp(Tr("inspection.findings_help"));

    bool findingsChanged = false;
    const auto& findings = InspectionOptions::Findings();

    for (size_t i = 0; i < findings.size(); ++i)
    {
        const std::string label = Localization::Label(findings[i].labelKey, findings[i].key);
        bool on = m_inspection.findings.count(findings[i].key) > 0;

        if (i > 0)
            UI::SameLineOrWrap(UI::ButtonWidth(label.c_str()));

        if (UI::ToggleButton(label.c_str(), on, Danger))
        {
            if (on)
                m_inspection.findings.insert(findings[i].key);
            else
                m_inspection.findings.erase(findings[i].key);

            findingsChanged = true;
        }
    }

    ImGui::Spacing();

    ImGui::EndDisabled();

    if (m_settings.app.miteTrackingEnabled)
    {
        DrawMiteChecks();
        ImGui::Spacing();
    }

    ImGui::BeginDisabled(m_readOnly);

    UI::FormLabel(Tr("inspection.notes"));

    if (ImGui::InputTextMultiline("##InspectionNotes", &m_inspection.notes, ImVec2(-FLT_MIN, UI::Em(6.0f)), ImGuiInputTextFlags_WordWrap))
        headerChanged = true;

    ImGui::EndDisabled();

    if (!m_readOnly && headerChanged)
        SaveHeader();

    if (!m_readOnly && findingsChanged)
        SaveFindings();

    ImGui::EndChild();
    UI::EndScrollPane();
}

// Mite checks recorded with this inspection (saved on
// their own, so they are listed on the hive's Mites tab
// too). Read-only inspections only list them.
void InspectionScreen::DrawMiteChecks()
{
    using namespace Theme::Colors;

    UI::FormLabel(Tr("inspection.mite_checks_label"));

    for (const MiteCheck& check : m_miteChecks)
    {
        ImGui::PushID(check.id);

        const std::string result = MiteText::Result(m_settings, check);
        const std::string text = MiteText::MethodName(m_settings, check.methodId) + " \xC2\xB7 "
            + (result.empty() ? std::string(Tr("mites.no_result")) : result);

        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(text.c_str());

        const MiteAlert alert = MiteText::Alert(m_settings, check);

        if (alert != MiteAlert::None)
        {
            ImGui::SameLine();
            MaintenanceUI::MiteAlertChip(alert);
        }

        if (!m_readOnly)
        {
            const std::string editLabel = Localization::Label("common.edit", "EditMiteCheck");
            UI::SameLineOrWrap(UI::ButtonWidth(editLabel.c_str()));

            if (UI::Button(editLabel.c_str(), UI::ButtonKind::Secondary, -1.0f))
                m_miteDialog.OpenForEdit(check);
        }

        ImGui::PopID();
    }

    if (m_readOnly)
    {
        if (m_miteChecks.empty())
            UI::TextStyled(UI::Text::Body, TextMuted, Tr("inspection.not_recorded_short"));

        return;
    }

    const std::string recordLabel = Localization::Label("inspection.record_mite_check", "RecordMiteCheck");

    if (UI::Button(recordLabel.c_str()))
        m_miteDialog.OpenForAdd(m_hive.id, m_inspection.id, m_inspection.date);
}

// --------------------------------------------------
// Summary tab
// --------------------------------------------------

void InspectionScreen::DrawSummaryTab()
{
    using namespace Theme::Colors;

    UI::BeginScrollPane("Summary", ImVec2(0.0f, 0.0f), UI::Em(UI::Inspection::EditorMinEm), ImGuiChildFlags_None);

    double brood = 0.0, honey = 0.0, pollen = 0.0;
    int eggs = 0, swarm = 0, supersedure = 0, queenFrames = 0;

    for (const FrameObservation& o : m_inspection.frames)
    {
        brood += o.broodPct / 100.0;
        honey += o.honeyPct / 100.0;
        pollen += o.pollenPct / 100.0;
        eggs += o.eggs ? 1 : 0;
        swarm += o.swarmCells ? 1 : 0;
        supersedure += o.supersedureCells ? 1 : 0;
        queenFrames += o.queenSeen ? 1 : 0;
    }

    auto row = [](const char* label, const std::string& value)
    {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        UI::TextStyled(UI::Text::Label, TextSecondary, label);
        ImGui::TableNextColumn();
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextUnformatted(value.c_str());
        ImGui::PopTextWrapPos();
    };

    UI::BeginCard("Totals");
    UI::TextStyled(UI::Text::Title, TextPrimary, Tr("inspection.summary_totals"));
    ImGui::Spacing();

    if (ImGui::BeginTable("TotalsRows", 2, ImGuiTableFlags_SizingFixedFit))
    {
        ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, UI::Em(14.0f));
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

        row(Tr("inspection.frames_inspected"), std::to_string(m_inspection.frames.size()));
        row(Tr("inspection.brood"), FramesCount(brood));
        row(Tr("inspection.honey"), FramesCount(honey));
        row(Tr("inspection.pollen"), FramesCount(pollen));
        row(Tr("inspection.frames_with_eggs"), std::to_string(eggs));

        const std::string queen = InspectionOptions::Label(InspectionOptions::QueenStatus(), m_inspection.queenStatus);
        row(Tr("inspection.queen_label"), queen.empty() ? std::string(Tr("inspection.not_recorded_short")) : queen);

        if (queenFrames > 0)
            row(Tr("inspection.queen_seen"), Localization::FormatPlural("inspection.on_frames", queenFrames));

        if (swarm + supersedure > 0)
        {
            row(Tr("inspection.queen_cells"),
                Localization::Format("inspection.cells_value",
                    { { "swarm", std::to_string(swarm) }, { "supersedure", std::to_string(supersedure) } }));
        }

        std::string findings;

        for (const auto& option : InspectionOptions::Findings())
        {
            if (m_inspection.findings.count(option.key))
                findings += (findings.empty() ? "" : ", ") + std::string(Tr(option.labelKey));
        }

        row(Tr("inspection.findings_label"), findings.empty() ? std::string(Tr("inspection.no_findings")) : findings);

        if (m_inspection.temperatureC || !m_inspection.weather.empty())
        {
            std::string weather = Display::Temperature(m_inspection.temperatureC);
            const std::string conditions = InspectionOptions::Label(InspectionOptions::Weather(), m_inspection.weather);

            if (!conditions.empty())
                weather += (weather.empty() ? "" : "  \xC2\xB7  ") + conditions;

            row(Tr("inspection.weather_label"), weather);
        }

        ImGui::EndTable();
    }

    UI::EndCard();
    ImGui::Spacing();

    // Per box (table scrolls sideways when narrow)
    UI::BeginCard("PerBox");
    UI::TextStyled(UI::Text::Title, TextPrimary, Tr("inspection.summary_boxes"));
    ImGui::Spacing();

    const ImGuiTableFlags flags = ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg |
        ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_ScrollX;

    const float tableHeight = ImGui::GetFrameHeightWithSpacing() * (static_cast<float>(m_stack.size()) + 1.5f)
        + ImGui::GetStyle().ScrollbarSize;

    if (ImGui::BeginTable("PerBoxRows", 5, flags, ImVec2(0.0f, tableHeight)))
    {
        ImGui::TableSetupScrollFreeze(1, 1);
        ImGui::TableSetupColumn(Tr("inspection.col_box"));
        ImGui::TableSetupColumn(Tr("inspection.col_inspected"));
        ImGui::TableSetupColumn(Tr("inspection.brood"));
        ImGui::TableSetupColumn(Tr("inspection.honey"));
        ImGui::TableSetupColumn(Tr("inspection.pollen"));
        ImGui::TableHeadersRow();

        for (auto box = m_stack.rbegin(); box != m_stack.rend(); ++box)
        {
            int inspected = 0;
            double b = 0.0, h = 0.0, p = 0.0;

            for (const FrameObservation& o : m_inspection.frames)
            {
                if (o.boxId != box->id)
                    continue;

                ++inspected;
                b += o.broodPct / 100.0;
                h += o.honeyPct / 100.0;
                p += o.pollenPct / 100.0;
            }

            char bText[16], hText[16], pText[16];
            std::snprintf(bText, sizeof(bText), "%g", b);
            std::snprintf(hText, sizeof(hText), "%g", h);
            std::snprintf(pText, sizeof(pText), "%g", p);

            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::TextUnformatted(Display::BoxTitle(*box).c_str());
            ImGui::TableNextColumn(); ImGui::Text("%d / %d", inspected, static_cast<int>(box->frames.size()));
            ImGui::TableNextColumn(); ImGui::TextUnformatted(bText);
            ImGui::TableNextColumn(); ImGui::TextUnformatted(hText);
            ImGui::TableNextColumn(); ImGui::TextUnformatted(pText);
        }

        ImGui::EndTable();
    }

    UI::EndCard();
    UI::EndScrollPane();
}

// --------------------------------------------------
// Delete
// --------------------------------------------------

void InspectionScreen::DrawDeleteDialog(Result& result)
{
    if (m_openDelete)
    {
        ImGui::OpenPopup(DeletePopupId);
        m_openDelete = false;
    }

    UI::SetNextDialogLayout(UI::Dialog::ConfirmWidthEm);

    const std::string title = Localization::Label("inspection.delete_dialog.title", "DeleteInspectionDialog");

    if (!ImGui::BeginPopupModal(title.c_str(), nullptr, ImGuiWindowFlags_NoSavedSettings))
        return;

    const std::string body = Localization::Format(
        "inspection.delete_dialog.body", { { "date", Display::Date(m_inspection.date) } });

    UI::TextStyledWrapped(UI::Text::Body, Theme::Colors::TextPrimary, body.c_str());

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    const std::string confirmLabel = Localization::Label("inspection.delete_dialog.confirm", "Confirm");
    const std::string cancelLabel = Localization::Label("common.cancel", "Cancel");

    if (UI::Button(confirmLabel.c_str(), UI::ButtonKind::Danger))
    {
        if (m_inspectionRepository.Delete(m_inspection.id))
            result = Result::Deleted;
        else
            m_error = Localization::Format("errors.save_failed", { { "details", m_inspectionRepository.GetLastError() } });

        ImGui::CloseCurrentPopup();
    }

    UI::SameLineOrWrap(UI::ButtonWidth(cancelLabel.c_str()));

    if (UI::Button(cancelLabel.c_str()) || ImGui::IsKeyPressed(ImGuiKey_Escape, false))
        ImGui::CloseCurrentPopup();

    ImGui::EndPopup();
}
