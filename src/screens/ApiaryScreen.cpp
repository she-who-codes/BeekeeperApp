
#include "ApiaryScreen.h"

#include "config/FontManager.h"
#include "config/Theme.h"
#include "config/AppConfig.h"

#include "imgui.h"

#include <algorithm>

// --------------------------------------------------
// Constructor
// --------------------------------------------------

ApiaryScreen::ApiaryScreen(
    ApiaryRepository& apiaryRepository,
    HiveRepository& hiveRepository
)
    : m_apiaryRepository(apiaryRepository),
      m_hiveRepository(hiveRepository)
{
    LoadApiaries();
}

// --------------------------------------------------
// Data Loading
// --------------------------------------------------

void ApiaryScreen::LoadApiaries()
{
    if (!m_apiaryRepository.GetAll(m_apiaries))
    {
        m_errorMessage = m_apiaryRepository.GetLastError();
        return;
    }

    // Preserve the current selection if possible.
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
        m_selectedApiaryId,
        m_hives))
    {
        m_errorMessage = m_hiveRepository.GetLastError();
    }
}

// --------------------------------------------------
// Main Screen
// --------------------------------------------------

void ApiaryScreen::Render()
{
    ImGuiIO& io = ImGui::GetIO();

    // Fill the available application window.
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize);

    ImGui::PushStyleVar(
        ImGuiStyleVar_WindowPadding,
        ImVec2(120, AppConfig::Layout::MainPadding)
    );

    ImGui::Begin(
        "Apiary Management",
        nullptr,
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings
    );

    // ----------------------------------------------
    // Page Header
    // ----------------------------------------------

    ImGui::PushFont(Fonts::Heading);
    ImGui::Text("Apiary Management");
    ImGui::PopFont();

    ImGui::Spacing();

    ImGui::PushFont(Fonts::Small);
    ImGui::TextColored(
        Theme::Colors::TextSecondary,
        "Manage your apiaries and the colonies they contain."
    );
    ImGui::PopFont();

    ImGui::Spacing();

    // ----------------------------------------------
    // Error Message
    // ----------------------------------------------

    if (!m_errorMessage.empty())
    {
        ImGui::PushStyleColor(
            ImGuiCol_Text,
            Theme::Colors::Danger
        );

        ImGui::TextWrapped(
            "%s",
            m_errorMessage.c_str()
        );

        ImGui::PopStyleColor();
        ImGui::Spacing();
    }

    // ----------------------------------------------
    // Toolbar
    // ----------------------------------------------

    if (ImGui::Button(
        "+ Add Apiary",
        ImVec2(150, AppConfig::Layout::ButtonHeight)
    ))
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

    // ----------------------------------------------
    // Main Content Layout
    // ----------------------------------------------

    ImGui::BeginChild(
        "ApiaryLayout",
        ImVec2(0, 0),
        ImGuiChildFlags_None
    );

    ImVec2 available = ImGui::GetContentRegionAvail();

    // Responsive left panel.
    float leftWidth = std::clamp(
        available.x * 0.30f,
        220.0f,
        320.0f
    );

    // Give the content panel more room on smaller windows.
    if (available.x < 850.0f)
    {
        leftWidth = std::clamp(
            available.x * 0.28f,
            180.0f,
            240.0f
        );
    }

    // ----------------------------------------------
    // Left Panel: Apiary List
    // ----------------------------------------------

    ImGui::BeginChild(
        "ApiaryList",
        ImVec2(leftWidth, 0),
        ImGuiChildFlags_Borders
    );

    DrawApiaryList();

    ImGui::EndChild();

    ImGui::SameLine();

    // ----------------------------------------------
    // Right Panel: Selected Apiary
    // ----------------------------------------------

    ImGui::BeginChild(
        "HivePanel",
        ImVec2(0, 0),
        ImGuiChildFlags_Borders
    );

    DrawHivePanel();

    ImGui::EndChild();

    ImGui::EndChild();

    // ----------------------------------------------
    // Dialogs
    // ----------------------------------------------

    DrawAddApiaryPopup();
    DrawAddHivePopup();

    ImGui::End();

    ImGui::PopStyleVar();
}

// --------------------------------------------------
// Apiary List
// --------------------------------------------------

void ApiaryScreen::DrawApiaryList()
{
    ImGui::PushFont(Fonts::Bold);
    ImGui::Text("YOUR APIARIES");
    ImGui::PopFont();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (m_apiaries.empty())
    {
        ImGui::PushFont(Fonts::Small);

        ImGui::TextWrapped(
            "No apiaries yet. Add your first apiary to get started."
        );

        ImGui::PopFont();
        return;
    }

    for (const Apiary& apiary : m_apiaries)
    {
        ImGui::PushID(apiary.id);

        const bool selected =
            apiary.id == m_selectedApiaryId;

        // Selected apiary.
        if (ImGui::Selectable(
            apiary.name.c_str(),
            selected,
            ImGuiSelectableFlags_None,
            ImVec2(0, 42)
        ))
        {
            m_selectedApiaryId = apiary.id;
            m_errorMessage.clear();

            LoadHives();
        }

        // Location subtitle.
        if (!apiary.location.empty())
        {
            ImGui::Indent(10.0f);

            ImGui::PushFont(Fonts::Small);

            ImGui::TextColored(
                Theme::Colors::TextSecondary,
                "%s",
                apiary.location.c_str()
            );

            ImGui::PopFont();

            ImGui::Unindent(10.0f);
        }

        ImGui::Spacing();

        ImGui::PopID();
    }
}

// --------------------------------------------------
// Selected Apiary / Hive Panel
// --------------------------------------------------

void ApiaryScreen::DrawHivePanel()
{
    // No apiary selected.
    if (m_selectedApiaryId == 0)
    {
        ImGui::PushFont(Fonts::Heading);
        ImGui::Text("Welcome to BeeKeeper!");
        ImGui::PopFont();

        ImGui::Spacing();

        ImGui::TextWrapped(
            "Create an apiary to start organizing your hives."
        );

        return;
    }

    // Find selected apiary.
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

    // ----------------------------------------------
    // Apiary Heading
    // ----------------------------------------------

    ImGui::PushFont(Fonts::Heading);

    ImGui::Text(
        "%s",
        selectedApiary->name.c_str()
    );

    ImGui::PopFont();

    if (!selectedApiary->location.empty())
    {
        ImGui::PushFont(Fonts::Small);

        ImGui::TextColored(
            Theme::Colors::TextSecondary,
            "%s",
            selectedApiary->location.c_str()
        );

        ImGui::PopFont();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ----------------------------------------------
    // Hive Toolbar
    // ----------------------------------------------

    ImGui::PushFont(Fonts::Bold);

    ImGui::Text(
        "Hives (%zu)",
        m_hives.size()
    );

    ImGui::PopFont();

    ImGui::SameLine();

    if (ImGui::Button(
        "+ Add Hive",
        ImVec2(120, AppConfig::Layout::ButtonHeight)
    ))
    {
        m_hiveName[0] = '\0';
        m_hiveType[0] = '\0';
        m_hiveNotes[0] = '\0';

        m_errorMessage.clear();

        ImGui::OpenPopup("Add Hive");
    }

    ImGui::Spacing();

    // ----------------------------------------------
    // Empty State
    // ----------------------------------------------

    if (m_hives.empty())
    {
        ImGui::Spacing();

        ImGui::PushFont(Fonts::Heading);
        ImGui::Text("No hives yet");
        ImGui::PopFont();

        ImGui::Spacing();

        ImGui::PushFont(Fonts::Small);

        ImGui::TextColored(
            Theme::Colors::TextSecondary,
            "Add a hive to this apiary to begin tracking a colony."
        );

        ImGui::PopFont();

        return;
    }

    // ----------------------------------------------
    // Hive Cards
    // ----------------------------------------------

    for (const Hive& hive : m_hives)
    {
        ImGui::PushID(hive.id);

        ImGui::BeginChild(
            "HiveCard",
            ImVec2(0, 105),
            ImGuiChildFlags_Borders
        );

        ImGui::Spacing();
        ImGui::Indent(12.0f);

        // Hive name.
        ImGui::PushFont(Fonts::Bold);

        ImGui::Text(
            "%s",
            hive.name.c_str()
        );

        ImGui::PopFont();

        // Hive type.
        ImGui::PushFont(Fonts::Small);

        ImGui::TextColored(
            Theme::Colors::TextSecondary,
            "%s",
            hive.hiveType.empty()
                ? "Hive type not specified"
                : hive.hiveType.c_str()
        );

        if (!hive.installationDate.empty())
        {
            ImGui::TextColored(
                Theme::Colors::TextMuted,
                "Installed: %s",
                hive.installationDate.c_str()
            );
        }

        ImGui::PopFont();

        ImGui::Unindent(12.0f);

        ImGui::EndChild();

        ImGui::Spacing();

        ImGui::PopID();
    }
}

// --------------------------------------------------
// Add Apiary Dialog
// --------------------------------------------------

void ApiaryScreen::DrawAddApiaryPopup()
{
    if (ImGui::BeginPopupModal(
        "Add Apiary",
        nullptr,
        ImGuiWindowFlags_AlwaysAutoResize
    ))
    {
        ImGui::PushFont(Fonts::Heading);
        ImGui::Text("Create a New Apiary");
        ImGui::PopFont();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Name.
        ImGui::Text("Name *");

        ImGui::SetNextItemWidth(320);

        ImGui::InputText(
            "##ApiaryName",
            m_apiaryName,
            sizeof(m_apiaryName)
        );

        ImGui::Spacing();

        // Location.
        ImGui::Text("Location");

        ImGui::SetNextItemWidth(320);

        ImGui::InputText(
            "##ApiaryLocation",
            m_apiaryLocation,
            sizeof(m_apiaryLocation)
        );

        ImGui::Spacing();

        // Notes.
        ImGui::Text("Notes");

        ImGui::SetNextItemWidth(320);

        ImGui::InputTextMultiline(
            "##ApiaryNotes",
            m_apiaryNotes,
            sizeof(m_apiaryNotes),
            ImVec2(320, 80)
        );

        ImGui::Spacing();

        // Validation message.
        if (!m_errorMessage.empty())
        {
            ImGui::TextColored(
                Theme::Colors::Danger,
                "%s",
                m_errorMessage.c_str()
            );
        }

        ImGui::Spacing();

        // Save.
        if (ImGui::Button("Save Apiary", ImVec2(150, 38)))
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

        // Cancel.
        if (ImGui::Button("Cancel", ImVec2(100, 38)))
        {
            m_errorMessage.clear();
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

// --------------------------------------------------
// Add Hive Dialog
// --------------------------------------------------

void ApiaryScreen::DrawAddHivePopup()
{
    if (ImGui::BeginPopupModal(
        "Add Hive",
        nullptr,
        ImGuiWindowFlags_AlwaysAutoResize
    ))
    {
        ImGui::PushFont(Fonts::Heading);
        ImGui::Text("Add a Hive");
        ImGui::PopFont();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Hive name.
        ImGui::Text("Hive Name *");

        ImGui::SetNextItemWidth(320);

        ImGui::InputText(
            "##HiveName",
            m_hiveName,
            sizeof(m_hiveName)
        );

        ImGui::Spacing();

        // Hive type.
        ImGui::Text("Hive Type");

        ImGui::SetNextItemWidth(320);

        ImGui::InputText(
            "##HiveType",
            m_hiveType,
            sizeof(m_hiveType)
        );

        ImGui::Spacing();

        // Notes.
        ImGui::Text("Notes");

        ImGui::SetNextItemWidth(320);

        ImGui::InputTextMultiline(
            "##HiveNotes",
            m_hiveNotes,
            sizeof(m_hiveNotes),
            ImVec2(320, 80)
        );

        ImGui::Spacing();

        // Validation message.
        if (!m_errorMessage.empty())
        {
            ImGui::TextColored(
                Theme::Colors::Danger,
                "%s",
                m_errorMessage.c_str()
            );
        }

        ImGui::Spacing();

        // Save.
        if (ImGui::Button("Save Hive", ImVec2(150, 38)))
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

        // Cancel.
        if (ImGui::Button("Cancel", ImVec2(100, 38)))
        {
            m_errorMessage.clear();
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}
