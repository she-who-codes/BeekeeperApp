
#pragma once

#include "models/Mites.h"
#include "models/Settings.h"
#include "repositories/MiteRepository.h"

#include <string>

// ==================================================
// Mite check and treatment dialogs (add / edit / delete).
// Draw() every frame at the same ImGui ID-stack level.
// ==================================================

class MiteCheckDialog
{
public:
    enum class Result { None, Saved, Deleted };

    MiteCheckDialog(MiteRepository& repository, const SettingsData& settings);

    // inspectionId / date: when recorded from an inspection.
    void OpenForAdd(int hiveId, int inspectionId = 0, const std::string& dateIso = "");
    void OpenForEdit(const MiteCheck& check);

    Result Draw();

private:
    MiteRepository& m_repository;
    const SettingsData& m_settings;

    bool m_requestOpen = false;
    MiteCheck m_check;

    std::string m_dateText;
    std::string m_dateError;
    std::string m_error;
    bool m_confirmingDelete = false;

    const MiteCheckMethod* Method() const;
    void UseMethod(const MiteCheckMethod& method);
};

class TreatmentDialog
{
public:
    enum class Result { None, Saved, Deleted };

    explicit TreatmentDialog(MiteRepository& repository);

    void OpenForAdd(int hiveId);
    void OpenForEdit(const Treatment& treatment);

    Result Draw();

private:
    MiteRepository& m_repository;

    bool m_requestOpen = false;
    Treatment m_treatment;

    std::string m_dateText;
    std::string m_dateError;
    std::string m_productError;
    std::string m_error;
    bool m_confirmingDelete = false;
};

namespace MiteText
{
    // Method display name ("Alcohol wash" / custom name).
    std::string MethodName(const SettingsData& settings, int methodId);

    // "3.2%" / "10.5 mites/day" / "" for the method.
    std::string Result(const SettingsData& settings, const MiteCheck& check);

    MiteAlert Alert(const SettingsData& settings, const MiteCheck& check);
}
