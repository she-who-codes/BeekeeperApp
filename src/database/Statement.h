
#pragma once

#include "sqlite3.h"

#include <optional>
#include <string>

// ==================================================
// Statement
// --------------------------------------------------
// Small RAII wrapper for an SQLite prepared statement,
// shared by the repositories.
// ==================================================

class Statement
{
public:
    Statement(sqlite3* db, const char* sql)
    {
        if (sqlite3_prepare_v2(db, sql, -1, &m_statement, nullptr) != SQLITE_OK)
            m_statement = nullptr;
    }

    ~Statement()
    {
        sqlite3_finalize(m_statement);
    }

    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;

    bool Ok() const { return m_statement != nullptr; }
    sqlite3_stmt* Get() const { return m_statement; }

    void Text(int index, const std::string& value)
    {
        sqlite3_bind_text(m_statement, index, value.c_str(), -1, SQLITE_TRANSIENT);
    }

    void OptionalText(int index, const std::string& value)
    {
        if (value.empty())
            sqlite3_bind_null(m_statement, index);
        else
            Text(index, value);
    }

    void Int(int index, int value)
    {
        sqlite3_bind_int(m_statement, index, value);
    }

    void OptionalDouble(int index, const std::optional<double>& value)
    {
        if (value)
            sqlite3_bind_double(m_statement, index, *value);
        else
            sqlite3_bind_null(m_statement, index);
    }

    int Step()
    {
        return sqlite3_step(m_statement);
    }

    void Reset()
    {
        sqlite3_reset(m_statement);
        sqlite3_clear_bindings(m_statement);
    }

    std::string ColumnText(int column) const
    {
        const unsigned char* value = sqlite3_column_text(m_statement, column);
        return value ? reinterpret_cast<const char*>(value) : "";
    }

    int ColumnInt(int column) const
    {
        return sqlite3_column_int(m_statement, column);
    }

    std::optional<double> ColumnOptionalDouble(int column) const
    {
        if (sqlite3_column_type(m_statement, column) == SQLITE_NULL)
            return std::nullopt;

        return sqlite3_column_double(m_statement, column);
    }

private:
    sqlite3_stmt* m_statement = nullptr;
};

