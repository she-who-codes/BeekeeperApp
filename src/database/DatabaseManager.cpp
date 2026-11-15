
#include "DatabaseManager.h"

DatabaseManager::~DatabaseManager()
{
    Close();
}

bool DatabaseManager::Open(const std::string& databasePath)
{
    if (IsOpen())
        Close();

    const int result = sqlite3_open_v2(
        databasePath.c_str(),
        &m_database,
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
        nullptr
    );

    if (result != SQLITE_OK)
    {
        m_lastError = m_database
            ? sqlite3_errmsg(m_database)
            : "Unable to allocate SQLite connection.";

        Close();
        return false;
    }

    // Enable relational integrity.
    if (!Execute("PRAGMA foreign_keys = ON;"))
    {
        Close();
        return false;
    }

    // Improve reliability and concurrent read behavior.
    if (!Execute("PRAGMA journal_mode = WAL;"))
    {
        Close();
        return false;
    }

    if (!Execute("PRAGMA busy_timeout = 5000;"))
    {
        Close();
        return false;
    }

    m_lastError.clear();
    return true;
}

void DatabaseManager::Close()
{
    if (m_database)
    {
        sqlite3_close(m_database);
        m_database = nullptr;
    }
}

bool DatabaseManager::IsOpen() const
{
    return m_database != nullptr;
}

bool DatabaseManager::Execute(const std::string& sql)
{
    if (!IsOpen())
    {
        m_lastError = "Database is not open.";
        return false;
    }

    char* errorMessage = nullptr;

    const int result = sqlite3_exec(
        m_database,
        sql.c_str(),
        nullptr,
        nullptr,
        &errorMessage
    );

    if (result != SQLITE_OK)
    {
        m_lastError = errorMessage
            ? errorMessage
            : sqlite3_errmsg(m_database);

        sqlite3_free(errorMessage);
        return false;
    }

    return true;
}

std::string DatabaseManager::GetLastError() const
{
    return m_lastError;
}

bool DatabaseManager::GetUserVersion(int& version)
{
    version = 0;

    if (!IsOpen())
    {
        m_lastError = "Database is not open.";
        return false;
    }

    sqlite3_stmt* statement = nullptr;

    const int result = sqlite3_prepare_v2(
        m_database,
        "PRAGMA user_version;",
        -1,
        &statement,
        nullptr
    );

    if (result != SQLITE_OK)
    {
        m_lastError = sqlite3_errmsg(m_database);
        return false;
    }

    if (sqlite3_step(statement) == SQLITE_ROW)
    {
        version = sqlite3_column_int(statement, 0);
    }
    else
    {
        m_lastError = sqlite3_errmsg(m_database);
        sqlite3_finalize(statement);
        return false;
    }

    sqlite3_finalize(statement);
    return true;
}

bool DatabaseManager::SetUserVersion(int version)
{
    if (version < 0)
    {
        m_lastError = "Database version cannot be negative.";
        return false;
    }

    return Execute(
        "PRAGMA user_version = " + std::to_string(version) + ";"
    );
}
sqlite3* DatabaseManager::GetHandle() const
{
    return m_database;
}
