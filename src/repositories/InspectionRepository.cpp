
#include "InspectionRepository.h"

#include "database/Statement.h"
#include "database/Transaction.h"

namespace
{
    constexpr const char* ObservationColumns =
        "SELECT id, inspection_id, frame_id, box_id, frame_position, brood_pct, honey_pct, pollen_pct, "
        "eggs_seen, larvae_seen, capped_brood, drone_brood, queen_seen, swarm_cells, supersedure_cells, "
        "comb_drawn, notes FROM frame_observations ";

    void ReadObservation(const Statement& s, FrameObservation& o)
    {
        o.id = s.ColumnInt(0);
        o.inspectionId = s.ColumnInt(1);
        o.frameId = s.ColumnInt(2);
        o.boxId = s.ColumnInt(3);
        o.framePosition = s.ColumnInt(4);
        o.broodPct = s.ColumnInt(5);
        o.honeyPct = s.ColumnInt(6);
        o.pollenPct = s.ColumnInt(7);
        o.eggs = s.ColumnInt(8) != 0;
        o.larvae = s.ColumnInt(9) != 0;
        o.cappedBrood = s.ColumnInt(10) != 0;
        o.droneBrood = s.ColumnInt(11) != 0;
        o.queenSeen = s.ColumnInt(12) != 0;
        o.swarmCells = s.ColumnInt(13) != 0;
        o.supersedureCells = s.ColumnInt(14) != 0;
        o.combDrawn = s.ColumnText(15);
        o.notes = s.ColumnText(16);
    }

    // Summary columns shared by ListByHive / LatestCompleted.
    constexpr const char* SummarySelect =
        "SELECT i.id, i.inspected_at, i.inspected_time, i.status, i.queen_status, "
        "  COUNT(o.id), COALESCE(SUM(o.brood_pct), 0), COALESCE(SUM(o.honey_pct), 0), "
        "  COALESCE(SUM(o.pollen_pct), 0), COALESCE(MAX(o.queen_seen), 0), "
        "  COALESCE(MAX(o.swarm_cells + o.supersedure_cells), 0), "
        "  (SELECT COUNT(*) FROM inspection_findings f WHERE f.inspection_id = i.id) "
        "FROM inspections i LEFT JOIN frame_observations o ON o.inspection_id = i.id ";
}

InspectionRepository::InspectionRepository(DatabaseManager& database)
    : m_database(database)
{
}

std::string InspectionRepository::GetLastError() const
{
    return m_lastError;
}

bool InspectionRepository::Fail()
{
    sqlite3* db = m_database.GetHandle();
    m_lastError = db ? sqlite3_errmsg(db) : "Database is not open.";
    return false;
}

// --------------------------------------------------
// Create / read
// --------------------------------------------------

bool InspectionRepository::CreateDraft(Inspection& inspection)
{
    sqlite3* db = m_database.GetHandle();

    if (!db)
        return Fail();

    Statement insert(db,
        "INSERT INTO inspections (hive_id, inspected_at, inspected_time, status, updated_at) "
        "VALUES (?, ?, ?, 'draft', CURRENT_TIMESTAMP);");

    if (!insert.Ok())
        return Fail();

    insert.Int(1, inspection.hiveId);
    insert.Text(2, inspection.date);
    insert.OptionalText(3, inspection.time);

    if (insert.Step() != SQLITE_DONE)
        return Fail();

    inspection.id = static_cast<int>(sqlite3_last_insert_rowid(db));
    inspection.completed = false;
    return true;
}

bool InspectionRepository::FindDraft(int hiveId, Inspection& draft)
{
    draft = Inspection();

    Statement query(m_database.GetHandle(),
        "SELECT id FROM inspections WHERE hive_id = ? AND status = 'draft' "
        "ORDER BY id DESC LIMIT 1;");

    if (!query.Ok())
        return Fail();

    query.Int(1, hiveId);

    const int result = query.Step();

    if (result == SQLITE_DONE)
        return false;

    if (result != SQLITE_ROW)
        return Fail();

    return GetById(query.ColumnInt(0), draft);
}

bool InspectionRepository::GetById(int id, Inspection& inspection)
{
    sqlite3* db = m_database.GetHandle();

    if (!db)
        return Fail();

    Statement header(db,
        "SELECT id, hive_id, inspected_at, inspected_time, weather_temperature, weather_conditions, "
        "temperament, queen_status, colony_strength, stores, notes, status, completed_at "
        "FROM inspections WHERE id = ?;");

    if (!header.Ok())
        return Fail();

    header.Int(1, id);

    const int result = header.Step();

    if (result == SQLITE_DONE)
    {
        m_lastError = "Inspection not found.";
        return false;
    }

    if (result != SQLITE_ROW)
        return Fail();

    inspection = Inspection();
    inspection.id = header.ColumnInt(0);
    inspection.hiveId = header.ColumnInt(1);
    inspection.date = header.ColumnText(2);
    inspection.time = header.ColumnText(3);
    inspection.temperatureC = header.ColumnOptionalDouble(4);
    inspection.weather = header.ColumnText(5);
    inspection.temperament = header.ColumnText(6);
    inspection.queenStatus = header.ColumnText(7);
    inspection.strength = header.ColumnText(8);
    inspection.stores = header.ColumnText(9);
    inspection.notes = header.ColumnText(10);
    inspection.completed = header.ColumnText(11) == "completed";
    inspection.completedAt = header.ColumnText(12);

    Statement findings(db, "SELECT finding_key FROM inspection_findings WHERE inspection_id = ?;");
    Statement frames(db, (std::string(ObservationColumns) + "WHERE inspection_id = ? ORDER BY box_id, frame_position;").c_str());

    if (!findings.Ok() || !frames.Ok())
        return Fail();

    findings.Int(1, id);

    int step = SQLITE_OK;

    while ((step = findings.Step()) == SQLITE_ROW)
        inspection.findings.insert(findings.ColumnText(0));

    if (step != SQLITE_DONE)
        return Fail();

    frames.Int(1, id);

    while ((step = frames.Step()) == SQLITE_ROW)
    {
        FrameObservation observation;
        ReadObservation(frames, observation);
        inspection.frames.push_back(observation);
    }

    return step == SQLITE_DONE ? true : Fail();
}

bool InspectionRepository::Summaries(const char* where, int hiveId, std::vector<InspectionSummary>& out)
{
    out.clear();

    Statement query(m_database.GetHandle(), (std::string(SummarySelect) + where).c_str());

    if (!query.Ok())
        return Fail();

    query.Int(1, hiveId);

    int step = SQLITE_OK;

    while ((step = query.Step()) == SQLITE_ROW)
    {
        InspectionSummary s;
        s.id = query.ColumnInt(0);
        s.date = query.ColumnText(1);
        s.time = query.ColumnText(2);
        s.completed = query.ColumnText(3) == "completed";
        s.queenStatus = query.ColumnText(4);
        s.framesInspected = query.ColumnInt(5);
        s.broodFrames = query.ColumnInt(6) / 100.0;
        s.honeyFrames = query.ColumnInt(7) / 100.0;
        s.pollenFrames = query.ColumnInt(8) / 100.0;
        s.queenSeenOnFrame = query.ColumnInt(9) != 0;
        s.queenCells = query.ColumnInt(10) != 0;
        s.findings = query.ColumnInt(11);
        out.push_back(s);
    }

    return step == SQLITE_DONE ? true : Fail();
}

bool InspectionRepository::ListByHive(int hiveId, std::vector<InspectionSummary>& summaries)
{
    return Summaries(
        "WHERE i.hive_id = ? GROUP BY i.id "
        "ORDER BY i.inspected_at DESC, COALESCE(i.inspected_time, '') DESC, i.id DESC;",
        hiveId, summaries);
}

bool InspectionRepository::LatestCompleted(int hiveId, InspectionSummary& summary)
{
    std::vector<InspectionSummary> list;

    if (!Summaries(
        "WHERE i.hive_id = ? AND i.status = 'completed' GROUP BY i.id "
        "ORDER BY i.inspected_at DESC, COALESCE(i.inspected_time, '') DESC, i.id DESC LIMIT 1;",
        hiveId, list))
        return false;

    if (list.empty())
        return false;

    summary = list.front();
    return true;
}

// --------------------------------------------------
// Update
// --------------------------------------------------

bool InspectionRepository::UpdateHeader(const Inspection& i)
{
    Statement update(m_database.GetHandle(),
        "UPDATE inspections SET inspected_at = ?, inspected_time = ?, weather_temperature = ?, "
        "weather_conditions = ?, temperament = ?, queen_status = ?, colony_strength = ?, stores = ?, "
        "notes = ?, updated_at = CURRENT_TIMESTAMP WHERE id = ?;");

    if (!update.Ok())
        return Fail();

    update.Text(1, i.date);
    update.OptionalText(2, i.time);
    update.OptionalDouble(3, i.temperatureC);
    update.OptionalText(4, i.weather);
    update.OptionalText(5, i.temperament);
    update.OptionalText(6, i.queenStatus);
    update.OptionalText(7, i.strength);
    update.OptionalText(8, i.stores);
    update.OptionalText(9, i.notes);
    update.Int(10, i.id);

    return update.Step() == SQLITE_DONE ? true : Fail();
}

bool InspectionRepository::SetFindings(int inspectionId, const std::set<std::string>& findings)
{
    sqlite3* db = m_database.GetHandle();

    Transaction transaction(m_database);

    if (!db || !transaction.Begin())
        return Fail();

    Statement clear(db, "DELETE FROM inspection_findings WHERE inspection_id = ?;");
    Statement insert(db, "INSERT INTO inspection_findings (inspection_id, finding_key) VALUES (?, ?);");

    if (!clear.Ok() || !insert.Ok())
        return Fail();

    clear.Int(1, inspectionId);

    if (clear.Step() != SQLITE_DONE)
        return Fail();

    for (const std::string& key : findings)
    {
        insert.Reset();
        insert.Int(1, inspectionId);
        insert.Text(2, key);

        if (insert.Step() != SQLITE_DONE)
            return Fail();
    }

    return transaction.Commit() ? true : Fail();
}

bool InspectionRepository::SaveObservation(FrameObservation& o)
{
    sqlite3* db = m_database.GetHandle();

    if (!db)
        return Fail();

    Statement upsert(db,
        "INSERT INTO frame_observations (inspection_id, frame_id, box_id, frame_position, brood_pct, "
        "honey_pct, pollen_pct, eggs_seen, larvae_seen, capped_brood, drone_brood, queen_seen, swarm_cells, "
        "supersedure_cells, comb_drawn, notes, updated_at) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, CURRENT_TIMESTAMP) "
        "ON CONFLICT(inspection_id, frame_id) DO UPDATE SET "
        "brood_pct = excluded.brood_pct, honey_pct = excluded.honey_pct, pollen_pct = excluded.pollen_pct, "
        "eggs_seen = excluded.eggs_seen, larvae_seen = excluded.larvae_seen, capped_brood = excluded.capped_brood, "
        "drone_brood = excluded.drone_brood, queen_seen = excluded.queen_seen, swarm_cells = excluded.swarm_cells, "
        "supersedure_cells = excluded.supersedure_cells, comb_drawn = excluded.comb_drawn, notes = excluded.notes, "
        "updated_at = CURRENT_TIMESTAMP;");

    if (!upsert.Ok())
        return Fail();

    int n = 1;
    upsert.Int(n++, o.inspectionId);
    upsert.Int(n++, o.frameId);
    upsert.Int(n++, o.boxId);
    upsert.Int(n++, o.framePosition);
    upsert.Int(n++, o.broodPct);
    upsert.Int(n++, o.honeyPct);
    upsert.Int(n++, o.pollenPct);
    upsert.Int(n++, o.eggs ? 1 : 0);
    upsert.Int(n++, o.larvae ? 1 : 0);
    upsert.Int(n++, o.cappedBrood ? 1 : 0);
    upsert.Int(n++, o.droneBrood ? 1 : 0);
    upsert.Int(n++, o.queenSeen ? 1 : 0);
    upsert.Int(n++, o.swarmCells ? 1 : 0);
    upsert.Int(n++, o.supersedureCells ? 1 : 0);
    upsert.OptionalText(n++, o.combDrawn);
    upsert.OptionalText(n++, o.notes);

    if (upsert.Step() != SQLITE_DONE)
        return Fail();

    if (o.id == 0)
    {
        Statement id(db, "SELECT id FROM frame_observations WHERE inspection_id = ? AND frame_id = ?;");
        id.Int(1, o.inspectionId);
        id.Int(2, o.frameId);

        if (id.Step() == SQLITE_ROW)
            o.id = id.ColumnInt(0);
    }

    return true;
}

bool InspectionRepository::DeleteObservation(int inspectionId, int frameId)
{
    Statement remove(m_database.GetHandle(),
        "DELETE FROM frame_observations WHERE inspection_id = ? AND frame_id = ?;");

    if (!remove.Ok())
        return Fail();

    remove.Int(1, inspectionId);
    remove.Int(2, frameId);

    return remove.Step() == SQLITE_DONE ? true : Fail();
}

bool InspectionRepository::Complete(int inspectionId)
{
    sqlite3* db = m_database.GetHandle();

    Statement update(db,
        "UPDATE inspections SET status = 'completed', completed_at = CURRENT_TIMESTAMP, "
        "updated_at = CURRENT_TIMESTAMP WHERE id = ?;");

    if (!update.Ok())
        return Fail();

    update.Int(1, inspectionId);

    if (update.Step() != SQLITE_DONE)
        return Fail();

    if (sqlite3_changes(db) == 0)
    {
        m_lastError = "Inspection not found.";
        return false;
    }

    return true;
}

bool InspectionRepository::Delete(int inspectionId)
{
    sqlite3* db = m_database.GetHandle();

    Statement remove(db, "DELETE FROM inspections WHERE id = ?;");

    if (!remove.Ok())
        return Fail();

    remove.Int(1, inspectionId);

    if (remove.Step() != SQLITE_DONE)
        return Fail();

    if (sqlite3_changes(db) == 0)
    {
        m_lastError = "Inspection not found.";
        return false;
    }

    return true;
}

bool InspectionRepository::CopyFromLatest(int inspectionId, int hiveId, int& copied)
{
    copied = 0;

    sqlite3* db = m_database.GetHandle();

    if (!db)
        return Fail();

    InspectionSummary latest;
    m_lastError.clear();

    // No completed inspection yet: nothing to copy (not an
    // error). A database error sets m_lastError.
    if (!LatestCompleted(hiveId, latest))
        return m_lastError.empty();

    // Only frames that are still active in a box that is
    // still in the hive.
    Statement copy(db,
        "INSERT OR IGNORE INTO frame_observations (inspection_id, frame_id, box_id, frame_position, "
        "brood_pct, honey_pct, pollen_pct, eggs_seen, larvae_seen, capped_brood, drone_brood, queen_seen, "
        "swarm_cells, supersedure_cells, comb_drawn, notes, updated_at) "
        "SELECT ?, o.frame_id, o.box_id, o.frame_position, o.brood_pct, o.honey_pct, o.pollen_pct, "
        "o.eggs_seen, o.larvae_seen, o.capped_brood, o.drone_brood, o.queen_seen, o.swarm_cells, "
        "o.supersedure_cells, o.comb_drawn, NULL, CURRENT_TIMESTAMP "
        "FROM frame_observations o "
        "JOIN frames f ON f.id = o.frame_id AND f.is_active = 1 "
        "JOIN boxes b ON b.id = f.box_id AND b.removed_at IS NULL AND b.hive_id = ? "
        "WHERE o.inspection_id = ?;");

    if (!copy.Ok())
        return Fail();

    copy.Int(1, inspectionId);
    copy.Int(2, hiveId);
    copy.Int(3, latest.id);

    if (copy.Step() != SQLITE_DONE)
        return Fail();

    copied = sqlite3_changes(db);
    return true;
}
