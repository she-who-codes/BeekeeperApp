
#include "BoxRepository.h"

#include "database/Statement.h"
#include "database/Transaction.h"

namespace
{
    constexpr const char* BoxColumns =
        "SELECT id, hive_id, position, box_type, box_role, name, frame_capacity, "
        "installed_at, removed_at, notes FROM boxes ";

    constexpr const char* FrameColumns =
        "SELECT id, box_id, position, frame_type, frame_label, installed_at, "
        "removed_at, notes, is_active FROM frames ";

    void ReadBox(const Statement& s, Box& box)
    {
        box.id = s.ColumnInt(0);
        box.hiveId = s.ColumnInt(1);
        box.position = s.ColumnInt(2);
        box.boxType = s.ColumnText(3);
        box.role = s.ColumnText(4);
        box.label = s.ColumnText(5);
        box.frameCapacity = s.ColumnInt(6);
        box.installedAt = s.ColumnText(7);
        box.removedAt = s.ColumnText(8);
        box.notes = s.ColumnText(9);
    }

    void ReadFrame(const Statement& s, Frame& frame)
    {
        frame.id = s.ColumnInt(0);
        frame.boxId = s.ColumnInt(1);
        frame.position = s.ColumnInt(2);
        frame.frameType = s.ColumnText(3);
        frame.label = s.ColumnText(4);
        frame.installedAt = s.ColumnText(5);
        frame.removedAt = s.ColumnText(6);
        frame.notes = s.ColumnText(7);
        frame.isActive = s.ColumnInt(8) != 0;
    }
}

BoxRepository::BoxRepository(DatabaseManager& database)
    : m_database(database)
{
}

std::string BoxRepository::GetLastError() const
{
    return m_lastError;
}

bool BoxRepository::Fail()
{
    sqlite3* db = m_database.GetHandle();
    m_lastError = db ? sqlite3_errmsg(db) : "Database is not open.";
    return false;
}

// --------------------------------------------------
// Reading
// --------------------------------------------------

bool BoxRepository::GetStack(int hiveId, std::vector<Box>& boxes)
{
    boxes.clear();

    sqlite3* db = m_database.GetHandle();

    if (!db)
        return Fail();

    Statement boxQuery(db, (std::string(BoxColumns) +
        "WHERE hive_id = ? AND removed_at IS NULL ORDER BY position;").c_str());

    Statement frameQuery(db, (std::string(FrameColumns) +
        "WHERE box_id = ? AND is_active = 1 ORDER BY position;").c_str());

    if (!boxQuery.Ok() || !frameQuery.Ok())
        return Fail();

    boxQuery.Int(1, hiveId);

    int result = SQLITE_OK;

    while ((result = boxQuery.Step()) == SQLITE_ROW)
    {
        Box box;
        ReadBox(boxQuery, box);

        frameQuery.Reset();
        frameQuery.Int(1, box.id);

        int frameResult = SQLITE_OK;

        while ((frameResult = frameQuery.Step()) == SQLITE_ROW)
        {
            Frame frame;
            ReadFrame(frameQuery, frame);
            box.frames.push_back(frame);
        }

        if (frameResult != SQLITE_DONE)
            return Fail();

        boxes.push_back(box);
    }

    return result == SQLITE_DONE ? true : Fail();
}

bool BoxRepository::GetCountsByApiary(int apiaryId, std::map<int, HiveEquipmentCounts>& counts)
{
    counts.clear();

    sqlite3* db = m_database.GetHandle();

    if (!db)
        return Fail();

    // Feeder frames and follower boards are not counted as
    // frames.
    Statement query(db,
        "SELECT h.id, "
        "  (SELECT COUNT(*) FROM boxes b "
        "     WHERE b.hive_id = h.id AND b.removed_at IS NULL), "
        "  (SELECT COUNT(*) FROM frames f JOIN boxes b ON f.box_id = b.id "
        "     WHERE b.hive_id = h.id AND b.removed_at IS NULL AND f.is_active = 1 "
        "       AND (f.frame_type IS NULL OR f.frame_type NOT IN ('feeder', 'follower_board'))) "
        "FROM hives h WHERE h.apiary_id = ?;");

    if (!query.Ok())
        return Fail();

    query.Int(1, apiaryId);

    int result = SQLITE_OK;

    while ((result = query.Step()) == SQLITE_ROW)
    {
        HiveEquipmentCounts c;
        c.boxes = query.ColumnInt(1);
        c.frames = query.ColumnInt(2);
        counts[query.ColumnInt(0)] = c;
    }

    return result == SQLITE_DONE ? true : Fail();
}

bool BoxRepository::GetBox(int boxId, Box& box)
{
    Statement query(m_database.GetHandle(), (std::string(BoxColumns) + "WHERE id = ?;").c_str());

    if (!query.Ok())
        return Fail();

    query.Int(1, boxId);

    const int result = query.Step();

    if (result == SQLITE_ROW)
    {
        ReadBox(query, box);
        return true;
    }

    if (result == SQLITE_DONE)
    {
        m_lastError = "Box not found.";
        return false;
    }

    return Fail();
}

// --------------------------------------------------
// Boxes
// --------------------------------------------------

bool BoxRepository::InsertFrames(
    int boxId,
    int fromPosition,
    int toPosition,
    const std::string& frameType,
    const std::string& date)
{
    Statement insert(m_database.GetHandle(),
        "INSERT INTO frames (box_id, position, frame_type, installed_at, is_active) "
        "VALUES (?, ?, ?, ?, 1);");

    if (!insert.Ok())
        return Fail();

    for (int position = fromPosition; position <= toPosition; ++position)
    {
        insert.Reset();
        insert.Int(1, boxId);
        insert.Int(2, position);
        insert.OptionalText(3, frameType);
        insert.OptionalText(4, date);

        if (insert.Step() != SQLITE_DONE)
            return Fail();
    }

    return true;
}

bool BoxRepository::AddBox(Box& box, const std::string& frameType)
{
    sqlite3* db = m_database.GetHandle();

    if (!db)
        return Fail();

    Transaction transaction(m_database);

    if (!transaction.Begin())
        return Fail();

    Statement top(db,
        "SELECT COALESCE(MAX(position), 0) FROM boxes "
        "WHERE hive_id = ? AND removed_at IS NULL;");

    if (!top.Ok())
        return Fail();

    top.Int(1, box.hiveId);

    if (top.Step() != SQLITE_ROW)
        return Fail();

    const int position = top.ColumnInt(0) + 1;

    Statement insert(db,
        "INSERT INTO boxes "
        "(hive_id, name, box_type, box_role, position, frame_capacity, installed_at, notes) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?);");

    if (!insert.Ok())
        return Fail();

    insert.Int(1, box.hiveId);
    insert.Text(2, box.label);                  // name is NOT NULL
    insert.Text(3, box.boxType);
    insert.OptionalText(4, box.role);
    insert.Int(5, position);
    insert.Int(6, box.frameCapacity);
    insert.OptionalText(7, box.installedAt);
    insert.OptionalText(8, box.notes);

    if (insert.Step() != SQLITE_DONE)
        return Fail();

    const int boxId = static_cast<int>(sqlite3_last_insert_rowid(db));

    if (box.frameCapacity > 0
        && !InsertFrames(boxId, 1, box.frameCapacity, frameType, box.installedAt))
        return false;

    if (!transaction.Commit())
        return Fail();

    box.id = boxId;
    box.position = position;
    return true;
}

bool BoxRepository::UpdateBox(const Box& box, const std::string& newFrameType, const std::string& today)
{
    sqlite3* db = m_database.GetHandle();

    if (!db)
        return Fail();

    Transaction transaction(m_database);

    if (!transaction.Begin())
        return Fail();

    Box stored;

    if (!GetBox(box.id, stored))
        return false;

    Statement update(db,
        "UPDATE boxes SET name = ?, box_role = ?, frame_capacity = ?, "
        "installed_at = ?, notes = ? WHERE id = ?;");

    if (!update.Ok())
        return Fail();

    update.Text(1, box.label);
    update.OptionalText(2, box.role);
    update.Int(3, box.frameCapacity);
    update.OptionalText(4, box.installedAt);
    update.OptionalText(5, box.notes);
    update.Int(6, box.id);

    if (update.Step() != SQLITE_DONE)
        return Fail();

    if (box.frameCapacity > stored.frameCapacity)
    {
        if (!InsertFrames(box.id, stored.frameCapacity + 1, box.frameCapacity, newFrameType, today))
            return false;
    }
    else if (box.frameCapacity < stored.frameCapacity)
    {
        Statement retire(db,
            "UPDATE frames SET is_active = 0, removed_at = ? "
            "WHERE box_id = ? AND is_active = 1 AND position > ?;");

        if (!retire.Ok())
            return Fail();

        retire.OptionalText(1, today);
        retire.Int(2, box.id);
        retire.Int(3, box.frameCapacity);

        if (retire.Step() != SQLITE_DONE)
            return Fail();
    }

    return transaction.Commit() ? true : Fail();
}

bool BoxRepository::MoveBox(int boxId, int direction)
{
    sqlite3* db = m_database.GetHandle();

    if (!db)
        return Fail();

    Transaction transaction(m_database);

    if (!transaction.Begin())
        return Fail();

    Box box;

    if (!GetBox(boxId, box))
        return false;

    Statement neighbour(db,
        "SELECT id FROM boxes WHERE hive_id = ? AND removed_at IS NULL AND position = ?;");

    if (!neighbour.Ok())
        return Fail();

    neighbour.Int(1, box.hiveId);
    neighbour.Int(2, box.position + direction);

    const int result = neighbour.Step();

    if (result == SQLITE_DONE)
    {
        m_lastError = "The box cannot move further.";
        return false;
    }

    if (result != SQLITE_ROW)
        return Fail();

    const int otherId = neighbour.ColumnInt(0);

    Statement setPosition(db, "UPDATE boxes SET position = ? WHERE id = ?;");

    if (!setPosition.Ok())
        return Fail();

    setPosition.Int(1, box.position + direction);
    setPosition.Int(2, box.id);

    if (setPosition.Step() != SQLITE_DONE)
        return Fail();

    setPosition.Reset();
    setPosition.Int(1, box.position);
    setPosition.Int(2, otherId);

    if (setPosition.Step() != SQLITE_DONE)
        return Fail();

    return transaction.Commit() ? true : Fail();
}

bool BoxRepository::RenumberBoxes(int hiveId)
{
    sqlite3* db = m_database.GetHandle();

    Statement boxes(db, "SELECT id FROM boxes WHERE hive_id = ? AND removed_at IS NULL ORDER BY position, id;");
    Statement setPosition(db, "UPDATE boxes SET position = ? WHERE id = ?;");

    if (!boxes.Ok() || !setPosition.Ok())
        return Fail();

    boxes.Int(1, hiveId);

    std::vector<int> ids;
    int result = SQLITE_OK;

    while ((result = boxes.Step()) == SQLITE_ROW)
        ids.push_back(boxes.ColumnInt(0));

    if (result != SQLITE_DONE)
        return Fail();

    for (size_t i = 0; i < ids.size(); ++i)
    {
        setPosition.Reset();
        setPosition.Int(1, static_cast<int>(i) + 1);
        setPosition.Int(2, ids[i]);

        if (setPosition.Step() != SQLITE_DONE)
            return Fail();
    }

    return true;
}

bool BoxRepository::RemoveBox(int boxId, const std::string& date)
{
    sqlite3* db = m_database.GetHandle();

    if (!db)
        return Fail();

    Transaction transaction(m_database);

    if (!transaction.Begin())
        return Fail();

    Box box;

    if (!GetBox(boxId, box))
        return false;

    Statement remove(db, "UPDATE boxes SET removed_at = ? WHERE id = ? AND removed_at IS NULL;");

    if (!remove.Ok())
        return Fail();

    remove.Text(1, date);
    remove.Int(2, boxId);

    if (remove.Step() != SQLITE_DONE)
        return Fail();

    if (sqlite3_changes(db) == 0)
    {
        m_lastError = "Box not found.";
        return false;
    }

    if (!RenumberBoxes(box.hiveId))
        return false;

    return transaction.Commit() ? true : Fail();
}

// --------------------------------------------------
// Frames
// --------------------------------------------------

bool BoxRepository::UpdateFrame(const Frame& frame)
{
    sqlite3* db = m_database.GetHandle();

    if (!db)
        return Fail();

    Statement update(db,
        "UPDATE frames SET frame_type = ?, frame_label = ?, installed_at = ?, notes = ? "
        "WHERE id = ? AND is_active = 1;");

    if (!update.Ok())
        return Fail();

    update.OptionalText(1, frame.frameType);
    update.OptionalText(2, frame.label);
    update.OptionalText(3, frame.installedAt);
    update.OptionalText(4, frame.notes);
    update.Int(5, frame.id);

    if (update.Step() != SQLITE_DONE)
        return Fail();

    if (sqlite3_changes(db) == 0)
    {
        m_lastError = "Frame not found.";
        return false;
    }

    return true;
}

bool BoxRepository::ReplaceFrame(int frameId, const std::string& frameType, const std::string& date, Frame& replacement)
{
    sqlite3* db = m_database.GetHandle();

    if (!db)
        return Fail();

    Transaction transaction(m_database);

    if (!transaction.Begin())
        return Fail();

    Statement read(db, (std::string(FrameColumns) + "WHERE id = ? AND is_active = 1;").c_str());

    if (!read.Ok())
        return Fail();

    read.Int(1, frameId);

    const int result = read.Step();

    if (result == SQLITE_DONE)
    {
        m_lastError = "Frame not found.";
        return false;
    }

    if (result != SQLITE_ROW)
        return Fail();

    Frame old;
    ReadFrame(read, old);

    Statement retire(db, "UPDATE frames SET is_active = 0, removed_at = ? WHERE id = ?;");

    if (!retire.Ok())
        return Fail();

    retire.Text(1, date);
    retire.Int(2, frameId);

    if (retire.Step() != SQLITE_DONE)
        return Fail();

    if (!InsertFrames(old.boxId, old.position, old.position, frameType, date))
        return false;

    replacement = Frame();
    replacement.id = static_cast<int>(sqlite3_last_insert_rowid(db));
    replacement.boxId = old.boxId;
    replacement.position = old.position;
    replacement.frameType = frameType;
    replacement.installedAt = date;

    return transaction.Commit() ? true : Fail();
}
