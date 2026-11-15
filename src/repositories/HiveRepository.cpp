
#include "HiveRepository.h"

namespace
{
    std::string GetText(sqlite3_stmt* statement, int column)
    {
        const unsigned char* value = sqlite3_column_text(statement, column);

        return value
            ? reinterpret_cast<const char*>(value)
            : "";
    }

    void ReadHive(sqlite3_stmt* statement, Hive& hive)
    {
        hive.id               = sqlite3_column_int(statement, 0);
        hive.apiaryId         = sqlite3_column_int(statement, 1);
        hive.name             = GetText(statement, 2);
        hive.hiveType         = GetText(statement, 3);
        hive.installationDate = GetText(statement, 4);
        hive.notes            = GetText(statement, 5);
        hive.isActive          = sqlite3_column_int(statement, 6) != 0;
    }
}

HiveRepository::HiveRepository(DatabaseManager& database)
    : m_database(database)
{
}

bool HiveRepository::Create(Hive& hive)
{
    sqlite3* db = m_database.GetHandle();

    if (!db)
    {
        m_lastError = "Database is not open.";
        return false;
    }

    const char* sql =
        "INSERT INTO hives "
        "(apiary_id, name, hive_type, installation_date, notes, is_active) "
        "VALUES (?, ?, ?, ?, ?, ?);";

    sqlite3_stmt* statement = nullptr;

    if (sqlite3_prepare_v2(db, sql, -1, &statement, nullptr) != SQLITE_OK)
    {
        m_lastError = sqlite3_errmsg(db);
        return false;
    }

    sqlite3_bind_int(statement, 1, hive.apiaryId);
    sqlite3_bind_text(statement, 2, hive.name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 3, hive.hiveType.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 4, hive.installationDate.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 5, hive.notes.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(statement, 6, hive.isActive ? 1 : 0);

    const int result = sqlite3_step(statement);

    if (result == SQLITE_DONE)
    {
        hive.id = static_cast<int>(sqlite3_last_insert_rowid(db));
    }
    else
    {
        m_lastError = sqlite3_errmsg(db);
    }

    sqlite3_finalize(statement);

    return result == SQLITE_DONE;
}

bool HiveRepository::GetById(int id, Hive& hive)
{
    sqlite3* db = m_database.GetHandle();

    if (!db)
    {
        m_lastError = "Database is not open.";
        return false;
    }

    const char* sql =
        "SELECT id, apiary_id, name, hive_type, installation_date, "
        "notes, is_active FROM hives WHERE id = ?;";

    sqlite3_stmt* statement = nullptr;

    if (sqlite3_prepare_v2(db, sql, -1, &statement, nullptr) != SQLITE_OK)
    {
        m_lastError = sqlite3_errmsg(db);
        return false;
    }

    sqlite3_bind_int(statement, 1, id);

    const int result = sqlite3_step(statement);

    if (result == SQLITE_ROW)
    {
        ReadHive(statement, hive);
    }
    else if (result != SQLITE_DONE)
    {
        m_lastError = sqlite3_errmsg(db);
    }

    sqlite3_finalize(statement);

    return result == SQLITE_ROW;
}

bool HiveRepository::GetByApiary(int apiaryId, std::vector<Hive>& hives)
{
    hives.clear();

    sqlite3* db = m_database.GetHandle();

    if (!db)
    {
        m_lastError = "Database is not open.";
        return false;
    }

    const char* sql =
        "SELECT id, apiary_id, name, hive_type, installation_date, "
        "notes, is_active FROM hives "
        "WHERE apiary_id = ? ORDER BY name COLLATE NOCASE;";

    sqlite3_stmt* statement = nullptr;

    if (sqlite3_prepare_v2(db, sql, -1, &statement, nullptr) != SQLITE_OK)
    {
        m_lastError = sqlite3_errmsg(db);
        return false;
    }

    sqlite3_bind_int(statement, 1, apiaryId);

    int result = SQLITE_OK;

    while ((result = sqlite3_step(statement)) == SQLITE_ROW)
    {
        Hive hive;
        ReadHive(statement, hive);
        hives.push_back(hive);
    }

    if (result != SQLITE_DONE)
    {
        m_lastError = sqlite3_errmsg(db);
        sqlite3_finalize(statement);
        return false;
    }

    sqlite3_finalize(statement);
    return true;
}

std::string HiveRepository::GetLastError() const
{
    return m_lastError;
}
