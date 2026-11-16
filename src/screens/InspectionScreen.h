
#pragma once

#include "models/Box.h"
#include "models/Hive.h"
#include "models/Inspection.h"
#include "models/Mites.h"
#include "models/Settings.h"
#include "repositories/BoxRepository.h"
#include "repositories/InspectionRepository.h"
#include "repositories/MiteRepository.h"
#include "screens/MiteDialogs.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

// ==================================================
// InspectionScreen
// --------------------------------------------------
// Records one inspection. Every change is saved at once
// (drafts survive closing the app). Tabs:
//   Frames            box/frame navigator + large-button
//                     frame editor (coverage, brood signs,
//                     queen cells, comb, notes)
//   Colony & weather  date, time, temperature, weather,
//                     temperament, queen, strength, stores,
//                     pest / disease signs, mite checks
//                     recorded with this inspection, notes
//   Summary           totals and per-box breakdown
// Coverage: brood + honey + pollen never exceed 100 %;
// empty is the remainder. Feeder frames and divider
// boards have no coverage and cannot be selected.
// ==================================================

class InspectionScreen
{
public:
    enum class Result
    {
        None,
        Closed,         // back to the hive (draft kept)
        Finished,       // marked completed
        Deleted
    };

    InspectionScreen(
        InspectionRepository& inspectionRepository,
        BoxRepository& boxRepository,
        MiteRepository& miteRepository,
        const SettingsData& settings
    );

    bool Open(int inspectionId, const Hive& hive, bool readOnly);

    // Draws into the current window.
    Result Render();

    const Inspection& Current() const { return m_inspection; }

private:
    InspectionRepository& m_inspectionRepository;
    BoxRepository& m_boxRepository;
    MiteRepository& m_miteRepository;
    const SettingsData& m_settings;

    // Mite checks linked to this inspection.
    std::vector<MiteCheck> m_miteChecks;
    MiteCheckDialog m_miteDialog;

    Hive m_hive;
    Inspection m_inspection;
    std::vector<Box> m_stack;
    bool m_readOnly = false;

    // Colony & weather fields being typed.
    std::string m_dateText;
    std::string m_timeText;
    std::optional<double> m_temperature;    // display unit
    std::string m_dateError;
    std::string m_timeError;

    int m_selectedFrameId = 0;
    float m_navigationHeight = 0.0f;        // pinned frame navigation bar (measured)
    int m_editorFrameId = 0;                // frame the editor last showed
    bool m_showMap = false;                 // frame map on narrow windows
    bool m_narrow = false;                  // frames tab stacked (set while drawing)
    int m_requestTab = -1;                  // select a tab next frame

    std::string m_error;
    bool m_openDelete = false;

    // "Start from last inspection"
    bool m_hasPrevious = false;
    std::string m_previousDate;

    // Data
    void Load(int inspectionId);
    std::vector<std::pair<const Box*, const Frame*>> FrameOrder() const;   // top box first
    FrameObservation ObservationFor(const Box& box, const Frame& frame) const;
    const FrameObservation* FindObservation(int frameId) const;
    void StoreObservation(FrameObservation observation);
    void ClearObservation(int frameId);
    void SaveHeader();
    void SaveFindings();
    void LoadMiteChecks();
    bool ValidateForFinish();

    // UI
    Result DrawHeader();
    void DrawFramesTab();
    void DrawNavigator();
    void DrawFrameEditor();
    void DrawFrameNavigation();             // pinned: Previous / Next / Clear

    // Comb frames in navigator order, and the position of
    // the selected one in it (-1 if none).
    std::vector<std::pair<const Box*, const Frame*>> CombFrames(int& selectedIndex) const;
    void DrawColonyTab();
    void DrawMiteChecks();
    void DrawSummaryTab();
    void DrawDeleteDialog(Result& result);
};
