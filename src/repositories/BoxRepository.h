
#pragma once

#include "database/DatabaseManager.h"
#include "models/Box.h"

#include <map>
#include <string>
#include <vector>

// ==================================================
// BoxRepository
// --------------------------------------------------
// Boxes and frames of a hive. Nothing is deleted:
// removed boxes get removed_at, replaced frames get
// is_active = 0 and removed_at, so inspection history
// that refers to them stays valid.
//
// Box positions of active boxes are always 1..N
// (1 = bottom). Frame positions are 1..capacity.
// ==================================================

class BoxRepository
{
public:
    explicit BoxRepository(DatabaseManager& database);

    // Active boxes of a hive, bottom first, with their
    // active frames.
    bool GetStack(int hiveId, std::vector<Box>& boxes);

    // Box / frame counts for every hive of an apiary.
    bool GetCountsByApiary(int apiaryId, std::map<int, HiveEquipmentCounts>& counts);

    // Adds a box on top of the stack and creates
    // box.frameCapacity frames of 'frameType' installed on
    // box.installedAt. Sets box.id and box.position.
    bool AddBox(Box& box, const std::string& frameType);

    // Updates role, label, notes, installed date and frame
    // capacity. A larger capacity adds frames of
    // 'newFrameType' at the end; a smaller one retires the
    // frames above the new capacity (history kept).
    bool UpdateBox(const Box& box, const std::string& newFrameType, const std::string& today);

    // Swaps the box with its neighbour (+1 = up, -1 = down).
    bool MoveBox(int boxId, int direction);

    // Marks the box removed on 'date' and closes the gap in
    // the positions of the boxes above it.
    bool RemoveBox(int boxId, const std::string& date);

    // Updates type, label, installed date and notes.
    bool UpdateFrame(const Frame& frame);

    // Retires the frame (removed_at = date) and puts a new
    // frame of 'frameType' installed on 'date' in the same
    // position. Sets 'replacement'.
    bool ReplaceFrame(int frameId, const std::string& frameType, const std::string& date, Frame& replacement);

    std::string GetLastError() const;

private:
    DatabaseManager& m_database;
    std::string m_lastError;

    bool Fail();
    bool InsertFrames(int boxId, int fromPosition, int toPosition, const std::string& frameType, const std::string& date);
    bool RenumberBoxes(int hiveId);
    bool GetBox(int boxId, Box& box);
};
