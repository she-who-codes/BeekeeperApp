
#pragma once

#include <string>
#include "sqlite3.h"

class DatabaseManager
{
public:
    DatabaseManager() = default;
    ~DatabaseManager();
    sqlite3* GetHandle() const;
    // Prevent accidental copying of the database connection.
    DatabaseManager(const DatabaseManager &) = delete;
    DatabaseManager &operator=(const DatabaseManager &) = delete;

    bool Open(const std::string &databasePath);
    void Close();

    bool IsOpen() const;
    bool Execute(const std::string &sql);

    bool GetUserVersion(int &version);
    bool SetUserVersion(int version);

    std::string GetLastError() const;

private:
    sqlite3 *m_database = nullptr;
    std::string m_lastError;
};
