
#include "ApiaryScreen.h"

#include "imgui.h"

ApiaryScreen::ApiaryScreen(
    ApiaryRepository& apiaryRepository,
    HiveRepository& hiveRepository
)
    : m_apiaryRepository(apiaryRepository),
      m_hiveRepository(hiveRepository)
{
    LoadApiaries();
}

void ApiaryScreen::LoadApiaries()
{
    if (!m_apiaryRepository.GetAll(m_apiaries))
    {
        m_errorMessage = m_apiaryRepository.GetLastError();
        return;
    }

    // Keep the current selection if it still exists.
    bool selectionExists = false;

    for (const Apiary& apiary : m_apiaries)
    {
        if (apiary.id == m_selectedApiaryId)
        {
            selectionExists = true;
            break;
        }
    }

    if (!selectionExists)
    {
        m_selectedApiaryId =
            m_apiaries.empty() ? 0 : m_apiaries.front().id;
    }

    LoadHives();
}

void ApiaryScreen::LoadHives()
{
    m_hives.clear();

    if (m_selectedApiaryId == 0)
        return;

    if (!m_hiveRepository.GetByApiary(
        m_selectedApiaryId, m_hives))
    {
        m_errorMessage = m_hiveRepository.GetLastError();
    }
}

void ApiaryScreen::Render()
{
    ImGui::PushStyleVar(
        ImGuiStyleVar_WindowPadding,
        ImVec2(20, 20)
    );

    ImGui::Begin("Apiary Management");

    ImGui::Text(
        "Manage your apiaries and the colonies they contain."
    );

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (!m_errorMessage.empty())
    {
        ImGui::TextColored(
            ImVec4(0.9f, 0.3f, 0.3f, 1.0f),
            "%s",
            m_errorMessage.c_str()
        );

        ImGui::Spacing();
    }

    if (ImGui::Button("+ Add Apiary", ImVec2(150, 38)))
    {
        m_apiaryName[0] = '\0';
        m_apiaryLocation[0] = '\0';
        m_apiaryNotes[0] = '\0';
        m_errorMessage.clear();

        ImGui::OpenPopup("Add Apiary");
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::BeginChild(
        "ApiaryLayout",
        ImVec2(0, 0),
        ImGuiChildFlags_None
    );

    // Left side: Apiary list.
    ImGui::BeginChild(
        "ApiaryList",
        ImVec2(260, 0),
        ImGuiChildFlags_Borders
    );

    DrawApiaryList();

    ImGui::EndChild();

    ImGui::SameLine();

    // Right side: Selected apiary and its hives.
    ImGui::BeginChild(
        "HivePanel",
        ImVec2(0, 0),
        ImGuiChildFlags_Borders
    );

    DrawHivePanel();

    ImGui::EndChild();

    ImGui::EndChild();

    DrawAddApiaryPopup();
    DrawAddHivePopup();

    ImGui::End();

    ImGui::PopStyleVar();
}

void ApiaryScreen::DrawApiaryList()
{
    ImGui::Text("YOUR APIARIES");
    ImGui::Separator();
    ImGui::Spacing();

    if (m_apiaries.empty())
    {
        ImGui::TextWrapped(
            "No apiaries yet. Add your first apiary to get started."
        );

        return;
    }

    for (const Apiary& apiary : m_apiaries)
    {
        ImGui::PushID(apiary.id);

        const bool selected =
            apiary.id == m_selectedApiaryId;

        if (ImGui::Selectable(
            apiary.name.c_str(),
            selected,
            ImGuiSelectableFlags_None,
            ImVec2(0, 45)
        ))
        {
            m_selectedApiaryId = apiary.id;
            m_errorMessage.clear();
            LoadHives();
        }

        ImGui::PopID();

        if (!apiary.location.empty())
        {
            ImGui::Indent(10);
            ImGui::TextDisabled(
                "%s",
                apiary.location.c_str()
            );
            ImGui::Unindent(10);
        }

        ImGui::Spacing();
    }
}

void ApiaryScreen::DrawHivePanel()
{
    if (m_selectedApiaryId == 0)
    {
        ImGui::Text("Welcome to BeeKeeper!");
        ImGui::Spacing();

        ImGui::TextWrapped(
            "Create an apiary to start organizing your hives."
        );

        return;
    }

    const Apiary* selectedApiary = nullptr;

    for (const Apiary& apiary : m_apiaries)
    {
        if (apiary.id == m_selectedApiaryId)
        {
            selectedApiary = &apiary;
            break;
        }
    }

    if (!selectedApiary)
        return;

    ImGui::Text(
        "%s",
        selectedApiary->name.c_str()
    );

    if (!selectedApiary->location.empty())
    {
        ImGui::TextDisabled(
            "%s",
            selectedApiary->location.c_str()
        );
    }

    ImGui::Spacing();

    ImGui::Text(
        "%zu Hives",
        m_hives.size()
    );

    ImGui::SameLine();

    if (ImGui::Button("+ Add Hive", ImVec2(120, 32)))
    {
        m_hiveName[0] = '\0';
        m_hiveType[0] = '\0';
        m_hiveNotes[0] = '\0';
        m_errorMessage.clear();

        ImGui::OpenPopup("Add Hive");
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (m_hives.empty())
    {
        ImGui::TextWrapped(
            "This apiary doesn't have any hives yet. "
            "Add a hive to begin tracking a colony."
        );

        return;
    }

    for (const Hive& hive : m_hives)
    {
        ImGui::PushID(hive.id);

        ImGui::BeginChild(
            "HiveCard",
            ImVec2(0, 90),
            ImGuiChildFlags_Borders
        );

        ImGui::Text(
            "%s",
            hive.name.c_str()
        );

        ImGui::TextDisabled(
            "%s",
            hive.hiveType.empty()
                ? "Hive type not specified"
                : hive.hiveType.c_str()
        );

        if (!hive.installationDate.empty())
        {
            ImGui::TextDisabled(
                "Installed: %s",
                hive.installationDate.c_str()
            );
        }

        ImGui::EndChild();

        ImGui::PopID();
        ImGui::Spacing();
    }
}

void ApiaryScreen::DrawAddApiaryPopup()
{
    if (ImGui::BeginPopupModal(
        "Add Apiary",
        nullptr,
        ImGuiWindowFlags_AlwaysAutoResize
    ))
    {
        ImGui::Text("Create a new apiary");
        ImGui::Separator();

        ImGui::Text("Name *");
        ImGui::SetNextItemWidth(320);
        ImGui::InputText(
            "##ApiaryName",
            m_apiaryName,
            sizeof(m_apiaryName)
        );

        ImGui::Text("Location");
        ImGui::SetNextItemWidth(320);
        ImGui::InputText(
            "##ApiaryLocation",
            m_apiaryLocation,
            sizeof(m_apiaryLocation)
        );

        ImGui::Text("Notes");
        ImGui::SetNextItemWidth(320);
        ImGui::InputTextMultiline(
            "##ApiaryNotes",
            m_apiaryNotes,
            sizeof(m_apiaryNotes),
            ImVec2(320, 80)
        );

        ImGui::Spacing();

        if (ImGui::Button("Save Apiary", ImVec2(150, 35)))
        {
            if (m_apiaryName[0] == '\0')
            {
                m_errorMessage = "Apiary name is required.";
            }
            else
            {
                Apiary apiary;
                apiary.name = m_apiaryName;
                apiary.location = m_apiaryLocation;
                apiary.notes = m_apiaryNotes;

                if (m_apiaryRepository.Create(apiary))
                {
                    m_selectedApiaryId = apiary.id;
                    m_errorMessage.clear();

                    LoadApiaries();
                    ImGui::CloseCurrentPopup();
                }
                else
                {
                    m_errorMessage =
                        m_apiaryRepository.GetLastError();
                }
            }
        }

        ImGui::SameLine();

        if (ImGui::Button("Cancel", ImVec2(100, 35)))
        {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void ApiaryScreen::DrawAddHivePopup()
{
    if (ImGui::BeginPopupModal(
        "Add Hive",
        nullptr,
        ImGuiWindowFlags_AlwaysAutoResize
    ))
    {
        ImGui::Text("Add a hive to this apiary");
        ImGui::Separator();

        ImGui::Text("Hive Name *");
        ImGui::SetNextItemWidth(320);
        ImGui::InputText(
            "##HiveName",
            m_hiveName,
            sizeof(m_hiveName)
        );

        ImGui::Text("Hive Type");
        ImGui::SetNextItemWidth(320);
        ImGui::InputText(
            "##HiveType",
            m_hiveType,
            sizeof(m_hiveType)
        );

        ImGui::Text("Notes");
        ImGui::SetNextItemWidth(320);
        ImGui::InputTextMultiline(
            "##HiveNotes",
            m_hiveNotes,
            sizeof(m_hiveNotes),
            ImVec2(320, 80)
        );

        ImGui::Spacing();

        if (ImGui::Button("Save Hive", ImVec2(150, 35)))
        {
            if (m_hiveName[0] == '\0')
            {
                m_errorMessage = "Hive name is required.";
            }
            else
            {
                Hive hive;
                hive.apiaryId = m_selectedApiaryId;
                hive.name = m_hiveName;
                hive.hiveType = m_hiveType;
                hive.notes = m_hiveNotes;

                if (m_hiveRepository.Create(hive))
                {
                    m_errorMessage.clear();

                    LoadHives();
                    ImGui::CloseCurrentPopup();
                }
                else
                {
                    m_errorMessage =
                        m_hiveRepository.GetLastError();
                }
            }
        }

        ImGui::SameLine();

        if (ImGui::Button("Cancel", ImVec2(100, 35)))
        {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}
