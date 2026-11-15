
#include "HiveRepository.h"

namespace
{
    constexpr const char* SelectColumns =
        "SELECT id, apiary_id, name, hive_type, hive_type_other, "
        "setup_date, colony_established_date, notes, "
        "is_active, archived_at "
        "FROM hives ";

    std::string GetText(sqlite3_stmt* statement, int column)
    {
        const unsigned char* value = sqlite3_column_text(statement, column);

        return value
            ? reinterpret_cast<const char*>(value)
            : "";
    }

    // Stores NULL instead of an empty string.
    void BindOptionalText(sqlite3_stmt* statement, int index, const std::string& value)
    {
        if (value.empty())
            sqlite3_bind_null(statement, index);
        else
            sqlite3_bind_text(statement, index, value.c_str(), -1, SQLITE_TRANSIENT);
    }

    void ReadHive(sqlite3_stmt* statement, Hive& hive)
    {
        hive.id               = sqlite3_column_int(statement, 0);
        hive.apiaryId         = sqlite3_column_int(statement, 1);
        hive.name             = GetText(statement, 2);
        hive.hiveType         = GetText(statement, 3);
        hive.hiveTypeOther    = GetText(statement, 4);
        hive.setupDate        = GetText(statement, 5);
        hive.colonyEstablishedDate = GetText(statement, 6);
        hive.notes            = GetText(statement, 7);
        hive.isActive         = sqlite3_column_int(statement, 8) != 0;
        hive.archivedAt       = GetText(statement, 9);
    }

    // Binds the editable fields starting at 'first'.
    // Order: apiary_id, name, hive_type, hive_type_other,
    //        setup_date, colony_established_date, notes
    constexpr int EditableFieldCount = 7;

    void BindEditableFields(sqlite3_stmt* statement, int first, const Hive& hive)
    {
        sqlite3_bind_int(statement, first, hive.apiaryId);
        sqlite3_bind_text(statement, first + 1, hive.name.c_str(), -1, SQLITE_TRANSIENT);
        BindOptionalText(statement, first + 2, hive.hiveType);
        BindOptionalText(statement, first + 3, hive.hiveTypeOther);
        BindOptionalText(statement, first + 4, hive.setupDate);
        BindOptionalText(statement, first + 5, hive.colonyEstablishedDate);
        BindOptionalText(statement, first + 6, hive.notes);
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
        "(apiary_id, name, hive_type, hive_type_other, "
        "setup_date, colony_established_date, notes, is_active) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, 1);";

    sqlite3_stmt* statement = nullptr;

    if (sqlite3_prepare_v2(db, sql, -1, &statement, nullptr) != SQLITE_OK)
    {
        m_lastError = sqlite3_errmsg(db);
        return false;
    }

    BindEditableFields(statement, 1, hive);

    const int result = sqlite3_step(statement);

    if (result == SQLITE_DONE)
    {
        hive.id = static_cast<int>(sqlite3_last_insert_rowid(db));
        hive.isActive = true;
        hive.archivedAt.clear();
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

    const std::string sql = std::string(SelectColumns) + "WHERE id = ?;";

    sqlite3_stmt* statement = nullptr;

    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &statement, nullptr) != SQLITE_OK)
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
    else if (result == SQLITE_DONE)
    {
        m_lastError = "Hive not found.";
    }
    else
    {
        m_lastError = sqlite3_errmsg(db);
    }

    sqlite3_finalize(statement);

    return result == SQLITE_ROW;
}

bool HiveRepository::GetByApiary(
    int apiaryId,
    std::vector<Hive>& hives,
    ArchiveFilter filter)
{
    hives.clear();

    sqlite3* db = m_database.GetHandle();

    if (!db)
    {
        m_lastError = "Database is not open.";
        return false;
    }

    std::string sql = std::string(SelectColumns) + "WHERE apiary_id = ? ";

    switch (filter)
    {
        case ArchiveFilter::ActiveOnly:   sql += "AND is_active = 1 "; break;
        case ArchiveFilter::ArchivedOnly: sql += "AND is_active = 0 "; break;
        case ArchiveFilter::All:          break;
    }

    sql += "ORDER BY is_active DESC, name COLLATE NOCASE;";

    sqlite3_stmt* statement = nullptr;

    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &statement, nullptr) != SQLITE_OK)
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

bool HiveRepository::Update(const Hive& hive)
{
    sqlite3* db = m_database.GetHandle();

    if (!db)
    {
        m_lastError = "Database is not open.";
        return false;
    }

    const char* sql =
        "UPDATE hives SET "
        "apiary_id = ?, name = ?, hive_type = ?, hive_type_other = ?, "
        "setup_date = ?, colony_established_date = ?, notes = ?, "
        "updated_at = CURRENT_TIMESTAMP "
        "WHERE id = ?;";

    sqlite3_stmt* statement = nullptr;

    if (sqlite3_prepare_v2(db, sql, -1, &statement, nullptr) != SQLITE_OK)
    {
        m_lastError = sqlite3_errmsg(db);
        return false;
    }

    BindEditableFields(statement, 1, hive);
    sqlite3_bind_int(statement, EditableFieldCount + 1, hive.id);

    const int result = sqlite3_step(statement);
    bool success = result == SQLITE_DONE;

    if (!success)
    {
        m_lastError = sqlite3_errmsg(db);
    }
    else if (sqlite3_changes(db) == 0)
    {
        m_lastError = "Hive not found.";
        success = false;
    }

    sqlite3_finalize(statement);
    return success;
}

bool HiveRepository::SetArchived(int id, bool archived)
{
    sqlite3* db = m_database.GetHandle();

    if (!db)
    {
        m_lastError = "Database is not open.";
        return false;
    }

    const char* sql = archived
        ? "UPDATE hives SET is_active = 0, "
          "archived_at = CURRENT_TIMESTAMP, "
          "updated_at = CURRENT_TIMESTAMP "
          "WHERE id = ?;"
        : "UPDATE hives SET is_active = 1, "
          "archived_at = NULL, "
          "updated_at = CURRENT_TIMESTAMP "
          "WHERE id = ?;";

    sqlite3_stmt* statement = nullptr;

    if (sqlite3_prepare_v2(db, sql, -1, &statement, nullptr) != SQLITE_OK)
    {
        m_lastError = sqlite3_errmsg(db);
        return false;
    }

    sqlite3_bind_int(statement, 1, id);

    const int result = sqlite3_step(statement);
    bool success = result == SQLITE_DONE;

    if (!success)
    {
        m_lastError = sqlite3_errmsg(db);
    }
    else if (sqlite3_changes(db) == 0)
    {
        m_lastError = "Hive not found.";
        success = false;
    }

    sqlite3_finalize(statement);
    return success;
}

std::string HiveRepository::GetLastError() const
{
    return m_lastError;
}
