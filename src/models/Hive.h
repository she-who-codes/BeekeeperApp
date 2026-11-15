
#pragma once

#include <string>

struct Hive
{
    int id = 0;
    int apiaryId = 0;

    std::string name;
    std::string hiveType;
    std::string installationDate;
    std::string notes;

    bool isActive = true;
};
