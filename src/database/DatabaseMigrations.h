
#pragma once

class DatabaseManager;

namespace DatabaseMigrations
{
    bool Run(DatabaseManager& database);
}
