
#include "MiteRepository.h"

#include "database/Statement.h"

namespace
{
    constexpr const char* CheckColumns =
        "SELECT id, hive_id, COALESCE(inspection_id, 0), method_id, checked_on, sample_size, mite_count, notes "
        "FROM mite_checks ";

    void ReadCheck(const Statement& s, MiteCheck& c)
    {
        c.id = s.ColumnInt(0);
        c.hiveId = s.ColumnInt(1);
        c.inspectionId = s.ColumnInt(2);
        c.methodId = s.ColumnInt(3);
        c.checkedOn = s.ColumnText(4);
        c.sampleSize = s.ColumnInt(5);
        c.miteCount = s.ColumnInt(6);
        c.notes = s.ColumnText(7);
    }
}

MiteRepository::MiteRepository(DatabaseManager& database)
    : m_database(database)
{
}

std::string MiteRepository::GetLastError() const
{
    return m_lastError;
}

bool MiteRepository::Fail()
{
    sqlite3* db = m_database.GetHandle();
    m_lastError = db ? sqlite3_errmsg(db) : "Database is not open.";
    return false;
}

bool MiteRepository::SaveCheck(MiteCheck& c)
{
    sqlite3* db = m_database.GetHandle();

    if (!db)
        return Fail();

    Statement statement(db, c.id == 0
        ? "INSERT INTO mite_checks (hive_id, inspection_id, method_id, checked_on, sample_size, mite_count, notes) "
          "VALUES (?, ?, ?, ?, ?, ?, ?);"
        : "UPDATE mite_checks SET hive_id = ?, inspection_id = ?, method_id = ?, checked_on = ?, "
          "sample_size = ?, mite_count = ?, notes = ? WHERE id = ?;");

    if (!statement.Ok())
        return Fail();

    statement.Int(1, c.hiveId);

    if (c.inspectionId > 0)
        statement.Int(2, c.inspectionId);
    else
        sqlite3_bind_null(statement.Get(), 2);

    statement.Int(3, c.methodId);
    statement.Text(4, c.checkedOn);
    statement.Int(5, c.sampleSize);
    statement.Int(6, c.miteCount);
    statement.OptionalText(7, c.notes);

    if (c.id != 0)
        statement.Int(8, c.id);

    if (statement.Step() != SQLITE_DONE)
        return Fail();

    if (c.id == 0)
        c.id = static_cast<int>(sqlite3_last_insert_rowid(db));

    return true;
}

bool MiteRepository::DeleteCheck(int id)
{
    Statement statement(m_database.GetHandle(), "DELETE FROM mite_checks WHERE id = ?;");

    if (!statement.Ok())
        return Fail();

    statement.Int(1, id);
    return statement.Step() == SQLITE_DONE ? true : Fail();
}

bool MiteRepository::ListChecks(int hiveId, std::vector<MiteCheck>& checks)
{
    checks.clear();

    Statement query(m_database.GetHandle(),
        (std::string(CheckColumns) + "WHERE hive_id = ? ORDER BY checked_on DESC, id DESC;").c_str());

    if (!query.Ok())
        return Fail();

    query.Int(1, hiveId);

    int step = SQLITE_OK;

    while ((step = query.Step()) == SQLITE_ROW)
    {
        MiteCheck c;
        ReadCheck(query, c);
        checks.push_back(c);
    }

    return step == SQLITE_DONE ? true : Fail();
}

bool MiteRepository::LatestCheck(int hiveId, MiteCheck& check)
{
    std::vector<MiteCheck> checks;

    if (!ListChecks(hiveId, checks) || checks.empty())
        return false;

    check = checks.front();
    return true;
}

bool MiteRepository::SaveTreatment(Treatment& t)
{
    sqlite3* db = m_database.GetHandle();

    if (!db)
        return Fail();

    Statement statement(db, t.id == 0
        ? "INSERT INTO treatments (hive_id, started_on, product, notes) VALUES (?, ?, ?, ?);"
        : "UPDATE treatments SET hive_id = ?, started_on = ?, product = ?, notes = ? WHERE id = ?;");

    if (!statement.Ok())
        return Fail();

    statement.Int(1, t.hiveId);
    statement.Text(2, t.startedOn);
    statement.Text(3, t.product);
    statement.OptionalText(4, t.notes);

    if (t.id != 0)
        statement.Int(5, t.id);

    if (statement.Step() != SQLITE_DONE)
        return Fail();

    if (t.id == 0)
        t.id = static_cast<int>(sqlite3_last_insert_rowid(db));

    return true;
}

bool MiteRepository::DeleteTreatment(int id)
{
    Statement statement(m_database.GetHandle(), "DELETE FROM treatments WHERE id = ?;");

    if (!statement.Ok())
        return Fail();

    statement.Int(1, id);
    return statement.Step() == SQLITE_DONE ? true : Fail();
}

bool MiteRepository::ListTreatments(int hiveId, std::vector<Treatment>& treatments)
{
    treatments.clear();

    Statement query(m_database.GetHandle(),
        "SELECT id, hive_id, started_on, product, notes FROM treatments "
        "WHERE hive_id = ? ORDER BY started_on DESC, id DESC;");

    if (!query.Ok())
        return Fail();

    query.Int(1, hiveId);

    int step = SQLITE_OK;

    while ((step = query.Step()) == SQLITE_ROW)
    {
        Treatment t;
        t.id = query.ColumnInt(0);
        t.hiveId = query.ColumnInt(1);
        t.startedOn = query.ColumnText(2);
        t.product = query.ColumnText(3);
        t.notes = query.ColumnText(4);
        treatments.push_back(t);
    }

    return step == SQLITE_DONE ? true : Fail();
}
