
#pragma once

#include "database/DatabaseManager.h"
#include "models/Mites.h"

#include <string>
#include <vector>

// ==================================================
// MiteRepository: mite checks and treatments.
// ==================================================

class MiteRepository
{
public:
    explicit MiteRepository(DatabaseManager& database);

    bool SaveCheck(MiteCheck& check);               // insert (id 0) or update
    bool DeleteCheck(int id);
    bool ListChecks(int hiveId, std::vector<MiteCheck>& checks);   // newest first
    bool LatestCheck(int hiveId, MiteCheck& check); // false if none

    bool SaveTreatment(Treatment& treatment);
    bool DeleteTreatment(int id);
    bool ListTreatments(int hiveId, std::vector<Treatment>& treatments);   // newest first

    std::string GetLastError() const;

private:
    DatabaseManager& m_database;
    std::string m_lastError;

    bool Fail();
};
