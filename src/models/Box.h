
#pragma once

#include <string>
#include <vector>

// ==================================================
// Boxes & frames of a hive
// --------------------------------------------------
// Box position 1 = bottom of the stack. Frame position
// 1..N = left to right, standing behind the hive.
// Keys (box type, role, frame type) are stable text
// stored in the database; see models/HiveEquipment.h.
// ==================================================

struct Frame
{
    int id = 0;
    int boxId = 0;

    int position = 1;

    std::string frameType;      // "wax_foundation", "feeder"...
    std::string label;          // optional keeper label
    std::string installedAt;    // ISO date (comb age)
    std::string removedAt;      // set when replaced
    std::string notes;

    bool isActive = true;
};

struct Box
{
    int id = 0;
    int hiveId = 0;

    int position = 1;

    std::string boxType;        // "deep", "queen_excluder"...
    std::string role;           // "brood", "honey_super" or ""
    std::string label;          // optional keeper label
    int frameCapacity = 0;      // 0 for boxes without frames

    std::string installedAt;    // ISO date
    std::string removedAt;      // set when removed from the hive
    std::string notes;

    // Active frames, ordered by position.
    std::vector<Frame> frames;
};

// Summary used on hive cards.
struct HiveEquipmentCounts
{
    int boxes = 0;
    int frames = 0;
};
