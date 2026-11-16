
#pragma once

#include <optional>
#include <set>
#include <string>
#include <vector>

// ==================================================
// Inspection model
// --------------------------------------------------
// Choice fields hold stable keys (see InspectionOptions);
// empty = not recorded. Temperature is stored in degrees
// Celsius and shown in the keeper's unit.
// ==================================================

struct FrameObservation
{
    int id = 0;
    int inspectionId = 0;
    int frameId = 0;
    int boxId = 0;
    int framePosition = 0;

    // Coverage of the frame (both sides together) in 25 %
    // steps. Empty = 100 - brood - honey - pollen.
    int broodPct = 0;
    int honeyPct = 0;
    int pollenPct = 0;

    bool eggs = false;
    bool larvae = false;
    bool cappedBrood = false;
    bool droneBrood = false;
    bool queenSeen = false;
    bool swarmCells = false;
    bool supersedureCells = false;

    std::string combDrawn;      // "none" | "partial" | "full" | ""
    std::string notes;

    int EmptyPct() const { return 100 - broodPct - honeyPct - pollenPct; }

    bool operator==(const FrameObservation& o) const
    {
        return broodPct == o.broodPct && honeyPct == o.honeyPct && pollenPct == o.pollenPct
            && eggs == o.eggs && larvae == o.larvae && cappedBrood == o.cappedBrood
            && droneBrood == o.droneBrood && queenSeen == o.queenSeen
            && swarmCells == o.swarmCells && supersedureCells == o.supersedureCells
            && combDrawn == o.combDrawn && notes == o.notes;
    }
};

struct Inspection
{
    int id = 0;
    int hiveId = 0;

    std::string date;           // ISO "YYYY-MM-DD"
    std::string time;           // "HH:MM" 24 h, optional

    std::optional<double> temperatureC;
    std::string weather;        // key or ""
    std::string temperament;
    std::string queenStatus;
    std::string strength;
    std::string stores;

    std::set<std::string> findings;     // pest / disease keys
    std::string notes;

    bool completed = false;
    std::string completedAt;

    std::vector<FrameObservation> frames;
};

// One row in the inspection list / overview.
struct InspectionSummary
{
    int id = 0;
    std::string date;
    std::string time;
    bool completed = false;

    std::string queenStatus;

    int framesInspected = 0;
    // Sum of coverage / 100, e.g. 6.5 frames of brood.
    double broodFrames = 0.0;
    double honeyFrames = 0.0;
    double pollenFrames = 0.0;

    int findings = 0;
    bool queenSeenOnFrame = false;
    bool queenCells = false;
};
