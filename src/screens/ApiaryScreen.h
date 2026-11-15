
#pragma once

#include "repositories/ApiaryRepository.h"
#include "repositories/HiveRepository.h"

#include <vector>
#include <string>

class ApiaryScreen
{
public:
    ApiaryScreen(
        ApiaryRepository& apiaryRepository,
        HiveRepository& hiveRepository
    );

    void Render();

private:
    ApiaryRepository& m_apiaryRepository;
    HiveRepository& m_hiveRepository;

    std::vector<Apiary> m_apiaries;
    std::vector<Hive> m_hives;

    int m_selectedApiaryId = 0;

    char m_apiaryName[128] = "";
    char m_apiaryLocation[256] = "";
    char m_apiaryNotes[512] = "";

    char m_hiveName[128] = "";
    char m_hiveType[128] = "Langstroth";
    char m_hiveNotes[512] = "";

    std::string m_errorMessage;

    void LoadApiaries();
    void LoadHives();

    void DrawApiaryList();
    void DrawHivePanel();
    void DrawAddApiaryPopup();
    void DrawAddHivePopup();
};
