
#pragma once

#include <string>

struct Hive
{
    int id = 0;
    int apiaryId = 0;

    std::string name;

    // Stable key from HiveTypes::All (e.g. "langstroth_10").
    std::string hiveType;

    // Free-text description when hiveType == "other".
    std::string hiveTypeOther;

    // ISO-8601 "YYYY-MM-DD". When the hive equipment was
    // set up at the apiary. Required; never in the future.
    std::string setupDate;

    // ISO-8601 "YYYY-MM-DD". When the current colony was
    // established (package/nuc installed, swarm caught,
    // split made). Optional: empty while the hive has no
    // colony. Never in the future. May be earlier than
    // setupDate (e.g. a nuc moved into this hive).
    std::string colonyEstablishedDate;

    std::string notes;

    bool isActive = true;

    // UTC timestamp set when the hive is archived;
    // empty while active.
    std::string archivedAt;
};
