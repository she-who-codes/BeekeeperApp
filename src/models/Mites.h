
#pragma once

#include <optional>
#include <string>

// ==================================================
// Mite checks and treatments
// ==================================================

struct MiteCheck
{
    int id = 0;
    int hiveId = 0;
    int inspectionId = 0;       // 0 = not linked
    int methodId = 0;

    std::string checkedOn;      // ISO date

    // Bees sampled, days on the board or drone cells
    // uncapped (depends on the method's unit).
    int sampleSize = 0;

    // Mites counted, or infested drone cells.
    int miteCount = 0;

    std::string notes;
};

struct Treatment
{
    int id = 0;
    int hiveId = 0;
    std::string startedOn;      // ISO date
    std::string product;
    std::string notes;
};

enum class MiteAlert
{
    None,
    Watch,      // amber
    Act         // red
};
