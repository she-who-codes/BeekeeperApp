
#pragma once

#include "database/DatabaseManager.h"
#include "models/Inspection.h"

#include <string>
#include <vector>

// ==================================================
// InspectionRepository
// --------------------------------------------------
// Inspections are recorded as a DRAFT that is saved as
// the keeper goes (nothing is lost if the app closes),
// then marked COMPLETED. Completed inspections can still
// be edited. Deleting an inspection removes its frame
// observations and findings (ON DELETE CASCADE).
// ==================================================

class InspectionRepository
{
public:
    explicit InspectionRepository(DatabaseManager& database);

    // Starts a draft for the hive. Sets inspection.id.
    bool CreateDraft(Inspection& inspection);

    // The hive's open draft, if any (false + id 0 if none).
    bool FindDraft(int hiveId, Inspection& draft);

    // Full inspection with frames and findings.
    bool GetById(int id, Inspection& inspection);

    // Newest first.
    bool ListByHive(int hiveId, std::vector<InspectionSummary>& summaries);

    // Latest COMPLETED inspection (false if none).
    bool LatestCompleted(int hiveId, InspectionSummary& summary);

    // Date, time, weather, colony fields and notes.
    bool UpdateHeader(const Inspection& inspection);

    bool SetFindings(int inspectionId, const std::set<std::string>& findings);

    // Insert or update the observation of one frame.
    bool SaveObservation(FrameObservation& observation);

    // Frame back to "not inspected".
    bool DeleteObservation(int inspectionId, int frameId);

    bool Complete(int inspectionId);
    bool Delete(int inspectionId);

    // Copies the frame observations of the hive's latest
    // completed inspection into 'inspectionId' for frames
    // that are still in the hive. Returns the count copied.
    bool CopyFromLatest(int inspectionId, int hiveId, int& copied);

    std::string GetLastError() const;

private:
    DatabaseManager& m_database;
    std::string m_lastError;

    bool Fail();
    bool Summaries(const char* where, int hiveId, std::vector<InspectionSummary>& out);
};
