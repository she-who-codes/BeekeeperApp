
#pragma once

#include "database/DatabaseManager.h"

#include <string>

// ==================================================
// Transaction
// --------------------------------------------------
// RAII savepoint: everything between construction and
// Commit() is applied together or not at all. Savepoints
// nest, so a repository method that uses a Transaction can
// be called from inside another one.
//
//   Transaction transaction(database);
//   if (!transaction.Begin()) ...
//   ... statements ...
//   return transaction.Commit();   // otherwise rolled back
// ==================================================

class Transaction
{
public:
    explicit Transaction(DatabaseManager& database)
        : m_database(database)
    {
    }

    ~Transaction()
    {
        if (m_active)
        {
            m_database.Execute("ROLLBACK TO " + Name() + ";");
            m_database.Execute("RELEASE " + Name() + ";");
            --Depth();
        }
    }

    Transaction(const Transaction&) = delete;
    Transaction& operator=(const Transaction&) = delete;

    bool Begin()
    {
        m_depth = ++Depth();
        m_active = m_database.Execute("SAVEPOINT " + Name() + ";");

        if (!m_active)
            --Depth();

        return m_active;
    }

    bool Commit()
    {
        if (!m_active)
            return false;

        m_active = false;
        --Depth();

        return m_database.Execute("RELEASE " + Name() + ";");
    }

private:
    DatabaseManager& m_database;
    bool m_active = false;
    int m_depth = 0;

    std::string Name() const
    {
        return "bk_sp_" + std::to_string(m_depth);
    }

    // Nesting depth (single-threaded UI).
    static int& Depth()
    {
        static int depth = 0;
        return depth;
    }
};
