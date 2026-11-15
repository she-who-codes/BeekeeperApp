
#pragma once

#include "models/Hive.h"
#include "database/DatabaseManager.h"

#include <vector>
#include <string>

class HiveRepository
{
public:
    explicit HiveRepository(DatabaseManager& database);

    bool Create(Hive& hive);
    bool GetById(int id, Hive& hive);
    bool GetByApiary(int apiaryId, std::vector<Hive>& hives);

    std::string GetLastError() const;

private:
    DatabaseManager& m_database;
    std::string m_lastError;
};
