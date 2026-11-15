
#pragma once

#include "models/Apiary.h"
#include "database/DatabaseManager.h"

#include <vector>
#include <string>

class ApiaryRepository
{
public:
    explicit ApiaryRepository(DatabaseManager& database);

    bool Create(Apiary& apiary);
    bool GetById(int id, Apiary& apiary);
    bool GetAll(std::vector<Apiary>& apiaries);

    std::string GetLastError() const;

private:
    DatabaseManager& m_database;
    std::string m_lastError;
};
