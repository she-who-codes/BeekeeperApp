
#include "MaintenanceRepository.h"

#include "database/Statement.h"

MaintenanceRepository::MaintenanceRepository(DatabaseManager& database)
    : m_database(database)
{
}

std::string MaintenanceRepository::GetLastError() const
{
    return m_lastError;
}

bool MaintenanceRepository::Fail()
{
    sqlite3* db = m_database.GetHandle();
    m_lastError = db ? sqlite3_errmsg(db) : "Database is not open.";
    return false;
}

bool MaintenanceRepository::MarkDone(int hiveId, int taskTypeId, const std::string& date)
{
    Statement insert(m_database.GetHandle(),
        "INSERT INTO maintenance_log (hive_id, task_type_id, done_on) VALUES (?, ?, ?);");

    if (!insert.Ok())
        return Fail();

    insert.Int(1, hiveId);
    insert.Int(2, taskTypeId);
    insert.Text(3, date);

    return insert.Step() == SQLITE_DONE ? true : Fail();
}

bool MaintenanceRepository::Postpone(int hiveId, int taskTypeId, const std::string& today, const std::string& until)
{
    Statement upsert(m_database.GetHandle(),
        "INSERT INTO maintenance_postponements (hive_id, task_type_id, postponed_on, postponed_until) "
        "VALUES (?, ?, ?, ?) ON CONFLICT(hive_id, task_type_id) DO UPDATE SET "
        "postponed_on = excluded.postponed_on, postponed_until = excluded.postponed_until;");

    if (!upsert.Ok())
        return Fail();

    upsert.Int(1, hiveId);
    upsert.Int(2, taskTypeId);
    upsert.Text(3, today);
    upsert.Text(4, until);

    return upsert.Step() == SQLITE_DONE ? true : Fail();
}

bool MaintenanceRepository::LoadHistory(int hiveId, MaintenanceSchedule::HiveHistory& h)
{
    sqlite3* db = m_database.GetHandle();

    if (!db)
        return Fail();

    h = MaintenanceSchedule::HiveHistory();
    h.hiveId = hiveId;

    // Single-value dates.
    Statement dates(db,
        "SELECT "
        " (SELECT MAX(inspected_at) FROM inspections WHERE hive_id = ?1 AND status = 'completed'), "
        " (SELECT MAX(checked_on) FROM mite_checks WHERE hive_id = ?1), "
        " (SELECT MAX(started_on) FROM treatments WHERE hive_id = ?1), "
        " (SELECT colony_established_date FROM hives WHERE id = ?1);");

    if (!dates.Ok())
        return Fail();

    dates.Int(1, hiveId);

    if (dates.Step() != SQLITE_ROW)
        return Fail();

    h.lastInspection = dates.ColumnText(0);
    h.lastMiteCheck = dates.ColumnText(1);
    h.latestTreatment = dates.ColumnText(2);
    h.colonyEstablished = dates.ColumnText(3);

    // Active comb frames in boxes still in the hive.
    Statement frames(db,
        "SELECT f.installed_at FROM frames f JOIN boxes b ON b.id = f.box_id "
        "WHERE b.hive_id = ? AND b.removed_at IS NULL AND f.is_active = 1 "
        "AND f.installed_at IS NOT NULL "
        "AND (f.frame_type IS NULL OR f.frame_type NOT IN ('feeder', 'follower_board'));");

    if (!frames.Ok())
        return Fail();

    frames.Int(1, hiveId);

    int step = SQLITE_OK;

    while ((step = frames.Step()) == SQLITE_ROW)
        h.combFrameInstallDates.push_back(frames.ColumnText(0));

    if (step != SQLITE_DONE)
        return Fail();

    Statement done(db,
        "SELECT task_type_id, MAX(done_on) FROM maintenance_log WHERE hive_id = ? GROUP BY task_type_id;");

    if (!done.Ok())
        return Fail();

    done.Int(1, hiveId);

    while ((step = done.Step()) == SQLITE_ROW)
        h.lastDone[done.ColumnInt(0)] = done.ColumnText(1);

    if (step != SQLITE_DONE)
        return Fail();

    Statement postponed(db,
        "SELECT task_type_id, postponed_on, postponed_until FROM maintenance_postponements WHERE hive_id = ?;");

    if (!postponed.Ok())
        return Fail();

    postponed.Int(1, hiveId);

    while ((step = postponed.Step()) == SQLITE_ROW)
        h.postponements[postponed.ColumnInt(0)] = { postponed.ColumnText(1), postponed.ColumnText(2) };

    return step == SQLITE_DONE ? true : Fail();
}

bool MaintenanceRepository::DueItems(int hiveId, const SettingsData& settings, const DateFormat::Date& today, std::vector<DueItem>& items)
{
    items.clear();
    MaintenanceSchedule::HiveHistory history;

    if (!LoadHistory(hiveId, history))
        return false;

    items = MaintenanceSchedule::Compute(history, settings, today);
    return true;
}

bool MaintenanceRepository::ActiveHives(std::vector<HiveRef>& hives)
{
    hives.clear();

    Statement query(m_database.GetHandle(),
        "SELECT h.id, h.name, a.id, a.name FROM hives h JOIN apiaries a ON a.id = h.apiary_id "
        "WHERE h.is_active = 1 AND a.is_active = 1 ORDER BY a.name COLLATE NOCASE, h.name COLLATE NOCASE;");

    if (!query.Ok())
        return Fail();

    int step = SQLITE_OK;

    while ((step = query.Step()) == SQLITE_ROW)
        hives.push_back({ query.ColumnInt(0), query.ColumnText(1), query.ColumnInt(2), query.ColumnText(3) });

    return step == SQLITE_DONE ? true : Fail();
}
