
#pragma once

#include "models/Hive.h"
#include "database/DatabaseManager.h"

#include <vector>
#include <string>

class HiveRepository
{
public:
    enum class ArchiveFilter
    {
        ActiveOnly,
        ArchivedOnly,
        All
    };

    explicit HiveRepository(DatabaseManager& database);

    bool Create(Hive& hive);
    bool GetById(int id, Hive& hive);

    // Active hives first, then by name.
    bool GetByApiary(
        int apiaryId,
        std::vector<Hive>& hives,
        ArchiveFilter filter = ArchiveFilter::ActiveOnly
    );

    // Updates the editable fields (apiary, name, type,
    // set-up date, colony date, notes) and updated_at.
    bool Update(const Hive& hive);

    // Archive (archived = true) or restore a hive.
    // Records are never deleted.
    bool SetArchived(int id, bool archived);

    std::string GetLastError() const;

private:
    DatabaseManager& m_database;
    std::string m_lastError;
};
