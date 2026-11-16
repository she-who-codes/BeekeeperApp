
#include "DatabaseMigrations.h"
#include "DatabaseManager.h"

#include <iostream>
#include "sqlite3.h"
#include <vector>
#include <string>

namespace
{
    struct Migration
    {
        int version;
        const char* description;
        const char* sql;

        // True for migrations that rebuild a table (SQLite
        // cannot drop a constraint in place). Foreign key
        // enforcement is switched off around the transaction
        // and PRAGMA foreign_key_check must pass before
        // COMMIT, as recommended by the SQLite docs.
        bool rebuildsTables = false;
    };

    // Returns false (and a description) if any foreign key
    // reference is broken.
    bool ForeignKeysValid(DatabaseManager& database, std::string& problem)
    {
        sqlite3_stmt* statement = nullptr;

        if (sqlite3_prepare_v2(database.GetHandle(), "PRAGMA foreign_key_check;", -1, &statement, nullptr) != SQLITE_OK)
        {
            problem = sqlite3_errmsg(database.GetHandle());
            return false;
        }

        const int result = sqlite3_step(statement);

        if (result == SQLITE_ROW)
        {
            const unsigned char* table = sqlite3_column_text(statement, 0);
            problem = std::string("Broken foreign key in table ")
                + (table ? reinterpret_cast<const char*>(table) : "?");
        }
        else if (result != SQLITE_DONE)
        {
            problem = sqlite3_errmsg(database.GetHandle());
        }

        sqlite3_finalize(statement);
        return result == SQLITE_DONE;
    }

    const std::vector<Migration> migrations =
    {
        {
            1,
            "Initial beekeeping database schema",

            R"SQL(

            CREATE TABLE apiaries
            (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                name TEXT NOT NULL,
                location TEXT,
                notes TEXT,
                created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
                updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
                is_active INTEGER NOT NULL DEFAULT 1
                    CHECK (is_active IN (0, 1))
            );

            CREATE TABLE hives
            (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                apiary_id INTEGER NOT NULL,
                name TEXT NOT NULL,
                hive_type TEXT,
                installation_date TEXT,
                notes TEXT,
                created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
                updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
                is_active INTEGER NOT NULL DEFAULT 1
                    CHECK (is_active IN (0, 1)),

                FOREIGN KEY (apiary_id)
                    REFERENCES apiaries(id)
                    ON DELETE RESTRICT
            );

            CREATE INDEX idx_hives_apiary
                ON hives(apiary_id);

            CREATE TABLE boxes
            (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                hive_id INTEGER NOT NULL,
                name TEXT NOT NULL,
                box_type TEXT,
                position INTEGER NOT NULL DEFAULT 0,
                installed_at TEXT,
                removed_at TEXT,
                created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,

                FOREIGN KEY (hive_id)
                    REFERENCES hives(id)
                    ON DELETE RESTRICT
            );

            CREATE INDEX idx_boxes_hive
                ON boxes(hive_id);

            CREATE TABLE frames
            (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                box_id INTEGER NOT NULL,
                position INTEGER NOT NULL,
                frame_label TEXT,
                frame_type TEXT,
                created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
                is_active INTEGER NOT NULL DEFAULT 1
                    CHECK (is_active IN (0, 1)),

                FOREIGN KEY (box_id)
                    REFERENCES boxes(id)
                    ON DELETE RESTRICT,

                UNIQUE(box_id, position)
            );

            CREATE INDEX idx_frames_box
                ON frames(box_id);

            CREATE TABLE inspections
            (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                hive_id INTEGER NOT NULL,
                inspected_at TEXT NOT NULL,
                weather_temperature REAL,
                weather_conditions TEXT,
                overall_assessment TEXT,
                notes TEXT,
                created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,

                FOREIGN KEY (hive_id)
                    REFERENCES hives(id)
                    ON DELETE RESTRICT
            );

            CREATE INDEX idx_inspections_hive_date
                ON inspections(hive_id, inspected_at);

            CREATE TABLE frame_observations
            (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                inspection_id INTEGER NOT NULL,
                frame_id INTEGER NOT NULL,
                box_id INTEGER NOT NULL,
                frame_position INTEGER NOT NULL,

                notes TEXT,
                created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,

                FOREIGN KEY (inspection_id)
                    REFERENCES inspections(id)
                    ON DELETE CASCADE,

                FOREIGN KEY (frame_id)
                    REFERENCES frames(id)
                    ON DELETE RESTRICT,

                FOREIGN KEY (box_id)
                    REFERENCES boxes(id)
                    ON DELETE RESTRICT,

                UNIQUE(inspection_id, frame_id)
            );

            CREATE INDEX idx_frame_observations_inspection
                ON frame_observations(inspection_id);

            CREATE INDEX idx_frame_observations_frame
                ON frame_observations(frame_id);

            )SQL"
        },

        {
            2,
            "Hive management: type keys, custom type, set-up and colony dates, archiving",

            R"SQL(

            -- The single "installation date" was ambiguous. It is
            -- split into two dates:
            --   setup_date              : hive equipment set up at the apiary
            --   colony_established_date : current colony established
            -- Existing values are kept as the set-up date.
            ALTER TABLE hives RENAME COLUMN installation_date TO setup_date;

            ALTER TABLE hives ADD COLUMN colony_established_date TEXT;

            UPDATE hives
               SET setup_date = NULL
             WHERE trim(setup_date) = '';

            -- Free-text description used when hive_type = 'other'.
            ALTER TABLE hives ADD COLUMN hive_type_other TEXT;

            -- UTC timestamp of when a hive was archived.
            ALTER TABLE hives ADD COLUMN archived_at TEXT;

            -- hive_type now stores a stable, locale-independent
            -- key. Convert the legacy free-text values.
            UPDATE hives
               SET hive_type = 'langstroth_10'
             WHERE lower(trim(hive_type)) = 'langstroth';

            -- Preserve any other legacy text as an 'other' type
            -- so no user data is lost.
            UPDATE hives
               SET hive_type_other = trim(hive_type),
                   hive_type = 'other'
             WHERE hive_type IS NOT NULL
               AND trim(hive_type) <> ''
               AND hive_type NOT IN
                   ('langstroth_8', 'langstroth_10', 'long_langstroth',
                    'top_bar', 'warre', 'layens', 'flow', 'other');

            UPDATE hives
               SET archived_at = updated_at
             WHERE is_active = 0
               AND archived_at IS NULL;

            CREATE INDEX idx_hives_apiary_active
                ON hives(apiary_id, is_active);

            )SQL"
        },

        {
            3,
            "Settings: app settings, mite check methods, maintenance task types",

            R"SQL(

            -- Scalar settings as key/value text
            -- (locale, date pattern, temperature unit, seasons...).
            CREATE TABLE app_settings
            (
                key TEXT PRIMARY KEY,
                value TEXT NOT NULL,
                updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
            );

            -- Built-in methods have method_key; keeper-defined
            -- methods have custom_name. Never deleted: archived.
            CREATE TABLE mite_check_methods
            (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                method_key TEXT UNIQUE,
                custom_name TEXT,
                unit TEXT NOT NULL
                    CHECK (unit IN ('percent_bees', 'mites_per_day', 'percent_cells')),
                sample_size INTEGER NOT NULL DEFAULT 0
                    CHECK (sample_size >= 0),
                is_enabled INTEGER NOT NULL DEFAULT 1
                    CHECK (is_enabled IN (0, 1)),
                is_archived INTEGER NOT NULL DEFAULT 0
                    CHECK (is_archived IN (0, 1)),
                sort_order INTEGER NOT NULL DEFAULT 0,
                created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
                updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,

                CHECK (method_key IS NOT NULL OR custom_name IS NOT NULL)
            );

            -- Watch (amber) / Act (red) levels per season.
            -- NULL = no alert for that season.
            CREATE TABLE mite_method_thresholds
            (
                method_id INTEGER NOT NULL,
                season TEXT NOT NULL
                    CHECK (season IN ('spring', 'summer', 'fall', 'winter')),
                watch_level REAL CHECK (watch_level IS NULL OR watch_level >= 0),
                act_level REAL CHECK (act_level IS NULL OR act_level >= 0),

                PRIMARY KEY (method_id, season),

                FOREIGN KEY (method_id)
                    REFERENCES mite_check_methods(id)
                    ON DELETE CASCADE,

                CHECK (watch_level IS NULL OR act_level IS NULL
                       OR watch_level <= act_level)
            );

            CREATE TABLE maintenance_task_types
            (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                task_key TEXT UNIQUE,
                custom_name TEXT,
                is_enabled INTEGER NOT NULL DEFAULT 1
                    CHECK (is_enabled IN (0, 1)),
                interval_value INTEGER NOT NULL
                    CHECK (interval_value > 0),
                interval_unit TEXT NOT NULL
                    CHECK (interval_unit IN ('days', 'weeks', 'months', 'years')),
                task_trigger TEXT NOT NULL DEFAULT 'recurring'
                    CHECK (task_trigger IN ('recurring', 'after_treatment')),
                is_archived INTEGER NOT NULL DEFAULT 0
                    CHECK (is_archived IN (0, 1)),
                sort_order INTEGER NOT NULL DEFAULT 0,
                created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
                updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,

                CHECK (task_key IS NOT NULL OR custom_name IS NOT NULL)
            );

            )SQL"
        },

        {
            4,
            "Boxes & frames: box role and capacity, frame history",

            R"SQL(

            -- Boxes ---------------------------------------------
            -- position: 1 = bottom box. removed_at set = removed
            -- from the hive (history kept, never deleted).
            ALTER TABLE boxes ADD COLUMN box_role TEXT
                CHECK (box_role IS NULL OR box_role IN ('brood', 'honey_super'));

            ALTER TABLE boxes ADD COLUMN frame_capacity INTEGER NOT NULL DEFAULT 0
                CHECK (frame_capacity >= 0);

            ALTER TABLE boxes ADD COLUMN notes TEXT;

            CREATE INDEX idx_boxes_hive_position
                ON boxes(hive_id, removed_at, position);

            -- Frames --------------------------------------------
            -- Rebuilt so a position can hold one ACTIVE frame
            -- while replaced frames (is_active = 0) keep their
            -- history. The old UNIQUE(box_id, position) allowed
            -- only one frame per position ever.
            CREATE TABLE frames_new
            (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                box_id INTEGER NOT NULL,
                position INTEGER NOT NULL,
                frame_label TEXT,
                frame_type TEXT,
                installed_at TEXT,
                removed_at TEXT,
                notes TEXT,
                created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
                is_active INTEGER NOT NULL DEFAULT 1
                    CHECK (is_active IN (0, 1)),

                FOREIGN KEY (box_id)
                    REFERENCES boxes(id)
                    ON DELETE RESTRICT
            );

            INSERT INTO frames_new
                (id, box_id, position, frame_label, frame_type, created_at, is_active)
            SELECT
                id, box_id, position, frame_label, frame_type, created_at, is_active
            FROM frames;

            DROP TABLE frames;

            ALTER TABLE frames_new RENAME TO frames;

            CREATE INDEX idx_frames_box
                ON frames(box_id);

            CREATE UNIQUE INDEX idx_frames_active_position
                ON frames(box_id, position)
                WHERE is_active = 1;

            )SQL",

            true    // rebuilds the frames table
        },

        {
            5,
            "Inspections: colony conditions, drafts, frame coverage, findings",

            R"SQL(

            -- Inspections ---------------------------------------
            -- weather_temperature is stored in degrees Celsius and
            -- shown in the keeper's unit (Settings).
            -- status: 'draft' while being recorded (saved as the
            -- keeper goes), 'completed' once finished.
            ALTER TABLE inspections ADD COLUMN inspected_time TEXT;   -- "HH:MM" (24 h)
            ALTER TABLE inspections ADD COLUMN status TEXT NOT NULL DEFAULT 'completed'
                CHECK (status IN ('draft', 'completed'));
            ALTER TABLE inspections ADD COLUMN temperament TEXT
                CHECK (temperament IS NULL OR temperament IN ('calm', 'nervous', 'defensive'));
            ALTER TABLE inspections ADD COLUMN queen_status TEXT
                CHECK (queen_status IS NULL OR queen_status IN ('seen', 'eggs_only', 'not_seen', 'queenless'));
            ALTER TABLE inspections ADD COLUMN colony_strength TEXT
                CHECK (colony_strength IS NULL OR colony_strength IN ('weak', 'moderate', 'strong'));
            ALTER TABLE inspections ADD COLUMN stores TEXT
                CHECK (stores IS NULL OR stores IN ('low', 'adequate', 'plentiful'));
            ALTER TABLE inspections ADD COLUMN completed_at TEXT;
            ALTER TABLE inspections ADD COLUMN updated_at TEXT;

            CREATE INDEX idx_inspections_hive_status
                ON inspections(hive_id, status, inspected_at);

            -- Pest / disease signs ticked during an inspection.
            CREATE TABLE inspection_findings
            (
                inspection_id INTEGER NOT NULL,
                finding_key TEXT NOT NULL,

                PRIMARY KEY (inspection_id, finding_key),

                FOREIGN KEY (inspection_id)
                    REFERENCES inspections(id)
                    ON DELETE CASCADE
            );

            -- Frame observations --------------------------------
            -- Coverage in 25 % steps; empty = 100 - the others.
            ALTER TABLE frame_observations ADD COLUMN brood_pct INTEGER NOT NULL DEFAULT 0
                CHECK (brood_pct IN (0, 25, 50, 75, 100));
            ALTER TABLE frame_observations ADD COLUMN honey_pct INTEGER NOT NULL DEFAULT 0
                CHECK (honey_pct IN (0, 25, 50, 75, 100));
            ALTER TABLE frame_observations ADD COLUMN pollen_pct INTEGER NOT NULL DEFAULT 0
                CHECK (pollen_pct IN (0, 25, 50, 75, 100));
            ALTER TABLE frame_observations ADD COLUMN eggs_seen INTEGER NOT NULL DEFAULT 0
                CHECK (eggs_seen IN (0, 1));
            ALTER TABLE frame_observations ADD COLUMN larvae_seen INTEGER NOT NULL DEFAULT 0
                CHECK (larvae_seen IN (0, 1));
            ALTER TABLE frame_observations ADD COLUMN capped_brood INTEGER NOT NULL DEFAULT 0
                CHECK (capped_brood IN (0, 1));
            ALTER TABLE frame_observations ADD COLUMN drone_brood INTEGER NOT NULL DEFAULT 0
                CHECK (drone_brood IN (0, 1));
            ALTER TABLE frame_observations ADD COLUMN queen_seen INTEGER NOT NULL DEFAULT 0
                CHECK (queen_seen IN (0, 1));
            ALTER TABLE frame_observations ADD COLUMN swarm_cells INTEGER NOT NULL DEFAULT 0
                CHECK (swarm_cells IN (0, 1));
            ALTER TABLE frame_observations ADD COLUMN supersedure_cells INTEGER NOT NULL DEFAULT 0
                CHECK (supersedure_cells IN (0, 1));
            ALTER TABLE frame_observations ADD COLUMN comb_drawn TEXT
                CHECK (comb_drawn IS NULL OR comb_drawn IN ('none', 'partial', 'full'));
            ALTER TABLE frame_observations ADD COLUMN updated_at TEXT;

            -- A frame can never be more than 100 % covered.
            CREATE TRIGGER frame_observations_coverage_insert
            BEFORE INSERT ON frame_observations
            WHEN NEW.brood_pct + NEW.honey_pct + NEW.pollen_pct > 100
            BEGIN
                SELECT RAISE(ABORT, 'Frame coverage exceeds 100%');
            END;

            CREATE TRIGGER frame_observations_coverage_update
            BEFORE UPDATE ON frame_observations
            WHEN NEW.brood_pct + NEW.honey_pct + NEW.pollen_pct > 100
            BEGIN
                SELECT RAISE(ABORT, 'Frame coverage exceeds 100%');
            END;

            )SQL"
        },

        {
            6,
            "Mite checks, treatments and maintenance (done / postponed)",

            R"SQL(

            -- Mite checks ---------------------------------------
            -- sample_size: bees, days on board or drone cells
            -- uncapped (depends on the method's unit).
            -- mite_count: mites counted, or infested drone cells.
            CREATE TABLE mite_checks
            (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                hive_id INTEGER NOT NULL,
                inspection_id INTEGER,
                method_id INTEGER NOT NULL,
                checked_on TEXT NOT NULL,
                sample_size INTEGER NOT NULL CHECK (sample_size > 0),
                mite_count INTEGER NOT NULL CHECK (mite_count >= 0),
                notes TEXT,
                created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,

                FOREIGN KEY (hive_id) REFERENCES hives(id) ON DELETE RESTRICT,
                FOREIGN KEY (inspection_id) REFERENCES inspections(id) ON DELETE SET NULL,
                FOREIGN KEY (method_id) REFERENCES mite_check_methods(id) ON DELETE RESTRICT
            );

            CREATE INDEX idx_mite_checks_hive_date
                ON mite_checks(hive_id, checked_on);

            -- Mite treatments (start the follow-up check clock).
            CREATE TABLE treatments
            (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                hive_id INTEGER NOT NULL,
                started_on TEXT NOT NULL,
                product TEXT NOT NULL,
                notes TEXT,
                created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,

                FOREIGN KEY (hive_id) REFERENCES hives(id) ON DELETE RESTRICT
            );

            CREATE INDEX idx_treatments_hive_date
                ON treatments(hive_id, started_on);

            -- Maintenance -------------------------------------
            -- "Done" marks a task complete on a date.
            CREATE TABLE maintenance_log
            (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                hive_id INTEGER NOT NULL,
                task_type_id INTEGER NOT NULL,
                done_on TEXT NOT NULL,
                notes TEXT,
                created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,

                FOREIGN KEY (hive_id) REFERENCES hives(id) ON DELETE RESTRICT,
                FOREIGN KEY (task_type_id) REFERENCES maintenance_task_types(id) ON DELETE RESTRICT
            );

            CREATE INDEX idx_maintenance_log_hive_task
                ON maintenance_log(hive_id, task_type_id, done_on);

            -- "Postpone": the task is not due before
            -- postponed_until. Ignored once the task is done
            -- after postponed_on. One per hive and task.
            CREATE TABLE maintenance_postponements
            (
                hive_id INTEGER NOT NULL,
                task_type_id INTEGER NOT NULL,
                postponed_on TEXT NOT NULL,
                postponed_until TEXT NOT NULL,

                PRIMARY KEY (hive_id, task_type_id),

                FOREIGN KEY (hive_id) REFERENCES hives(id) ON DELETE RESTRICT,
                FOREIGN KEY (task_type_id) REFERENCES maintenance_task_types(id) ON DELETE RESTRICT
            );

            )SQL"
        }
    };
}

namespace DatabaseMigrations
{
    bool Run(DatabaseManager& database)
    {
        int currentVersion = 0;

        if (!database.GetUserVersion(currentVersion))
        {
            std::cerr << "Could not read database version: "
                      << database.GetLastError() << std::endl;
            return false;
        }

        for (const Migration& migration : migrations)
        {
            if (migration.version <= currentVersion)
                continue;

            std::cout << "Applying migration "
                      << migration.version << ": "
                      << migration.description << std::endl;

            // Must be changed outside a transaction.
            if (migration.rebuildsTables && !database.Execute("PRAGMA foreign_keys = OFF;"))
                return false;

            // Re-enables foreign keys on every exit path.
            struct ForeignKeysGuard
            {
                DatabaseManager& database;
                bool active;

                ~ForeignKeysGuard()
                {
                    if (active)
                        database.Execute("PRAGMA foreign_keys = ON;");
                }
            } guard { database, migration.rebuildsTables };

            if (!database.Execute("BEGIN IMMEDIATE;"))
                return false;

            if (!database.Execute(migration.sql))
            {
                const std::string error = database.GetLastError();
                database.Execute("ROLLBACK;");

                std::cerr << "Migration failed: "
                          << error << std::endl;
                return false;
            }

            std::string problem;

            if (migration.rebuildsTables && !ForeignKeysValid(database, problem))
            {
                database.Execute("ROLLBACK;");

                std::cerr << "Migration failed: "
                          << problem << std::endl;
                return false;
            }

            if (!database.SetUserVersion(migration.version))
            {
                const std::string error = database.GetLastError();
                database.Execute("ROLLBACK;");

                std::cerr << "Could not update database version: "
                          << error << std::endl;
                return false;
            }

            if (!database.Execute("COMMIT;"))
            {
                const std::string error = database.GetLastError();
                database.Execute("ROLLBACK;");

                std::cerr << "Could not commit migration: "
                          << error << std::endl;
                return false;
            }

            currentVersion = migration.version;

            std::cout << "Migration "
                      << migration.version
                      << " completed successfully." << std::endl;
        }

        return true;
    }
}
