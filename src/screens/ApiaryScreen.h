
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
    // Repositories
    ApiaryRepository& m_apiaryRepository;
    HiveRepository& m_hiveRepository;

    // Data
    std::vector<Apiary> m_apiaries;
    std::vector<Hive> m_hives;

    int m_selectedApiaryId = 0;

    // Apiary form
    char m_apiaryName[128] = "";
    char m_apiaryLocation[256] = "";
    char m_apiaryNotes[512] = "";

    // Hive form
    char m_hiveName[128] = "";
    char m_hiveType[128] = "Langstroth";
    char m_hiveNotes[512] = "";

    // UI state
    std::string m_errorMessage;

    // Data operations
    void LoadApiaries();
    void LoadHives();

    // UI sections
    void DrawApiaryList();
    void DrawHivePanel();

    // Dialogs
    void DrawAddApiaryPopup();
    void DrawAddHivePopup();
};
