
#include "DatabaseMigrations.h"
#include "DatabaseManager.h"

#include <iostream>
#include <vector>
#include <string>

namespace
{
    struct Migration
    {
        int version;
        const char* description;
        const char* sql;
    };

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
