
#include "ApiaryRepository.h"

namespace
{
    std::string GetText(sqlite3_stmt* statement, int column)
    {
        const unsigned char* value = sqlite3_column_text(statement, column);

        return value
            ? reinterpret_cast<const char*>(value)
            : "";
    }

    void ReadApiary(sqlite3_stmt* statement, Apiary& apiary)
    {
        apiary.id       = sqlite3_column_int(statement, 0);
        apiary.name     = GetText(statement, 1);
        apiary.location = GetText(statement, 2);
        apiary.notes    = GetText(statement, 3);
        apiary.isActive = sqlite3_column_int(statement, 4) != 0;
    }
}

ApiaryRepository::ApiaryRepository(DatabaseManager& database)
    : m_database(database)
{
}

bool ApiaryRepository::Create(Apiary& apiary)
{
    sqlite3* db = m_database.GetHandle();

    if (!db)
    {
        m_lastError = "Database is not open.";
        return false;
    }

    const char* sql =
        "INSERT INTO apiaries (name, location, notes, is_active) "
        "VALUES (?, ?, ?, ?);";

    sqlite3_stmt* statement = nullptr;

    if (sqlite3_prepare_v2(db, sql, -1, &statement, nullptr) != SQLITE_OK)
    {
        m_lastError = sqlite3_errmsg(db);
        return false;
    }

    sqlite3_bind_text(statement, 1, apiary.name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 2, apiary.location.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 3, apiary.notes.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(statement, 4, apiary.isActive ? 1 : 0);

    const int result = sqlite3_step(statement);

    if (result == SQLITE_DONE)
    {
        apiary.id = static_cast<int>(sqlite3_last_insert_rowid(db));
    }
    else
    {
        m_lastError = sqlite3_errmsg(db);
    }

    sqlite3_finalize(statement);

    return result == SQLITE_DONE;
}

bool ApiaryRepository::GetById(int id, Apiary& apiary)
{
    sqlite3* db = m_database.GetHandle();

    if (!db)
    {
        m_lastError = "Database is not open.";
        return false;
    }

    const char* sql =
        "SELECT id, name, location, notes, is_active "
        "FROM apiaries WHERE id = ?;";

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
        ReadApiary(statement, apiary);
    }
    else if (result != SQLITE_DONE)
    {
        m_lastError = sqlite3_errmsg(db);
    }

    sqlite3_finalize(statement);

    return result == SQLITE_ROW;
}

bool ApiaryRepository::GetAll(std::vector<Apiary>& apiaries)
{
    apiaries.clear();

    sqlite3* db = m_database.GetHandle();

    if (!db)
    {
        m_lastError = "Database is not open.";
        return false;
    }

    const char* sql =
        "SELECT id, name, location, notes, is_active "
        "FROM apiaries ORDER BY name COLLATE NOCASE;";

    sqlite3_stmt* statement = nullptr;

    if (sqlite3_prepare_v2(db, sql, -1, &statement, nullptr) != SQLITE_OK)
    {
        m_lastError = sqlite3_errmsg(db);
        return false;
    }

    int result = SQLITE_OK;

    while ((result = sqlite3_step(statement)) == SQLITE_ROW)
    {
        Apiary apiary;
        ReadApiary(statement, apiary);
        apiaries.push_back(apiary);
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

std::string ApiaryRepository::GetLastError() const
{
    return m_lastError;
}
