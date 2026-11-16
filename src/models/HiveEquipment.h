
#pragma once

#include <string>
#include <vector>

// ==================================================
// HiveEquipment
// --------------------------------------------------
// SINGLE SOURCE for box types, box roles and frame types:
// which boxes each hive type can use, default frame
// counts and default frame types. The database stores the
// keys; the UI shows Tr(labelKey).
// ==================================================

namespace HiveEquipment
{
    // ---------------- Box types ----------------

    struct BoxTypeInfo
    {
        const char* key;
        const char* labelKey;
        bool holdsFrames;   // false: queen excluder, feeder
        bool hasRole;       // brood / honey super can be chosen
    };

    const BoxTypeInfo* FindBoxType(const std::string& key);

    // Box types offered for a hive type, most common
    // first. Unknown / "other" hive types get all types.
    std::vector<const BoxTypeInfo*> AllowedBoxTypes(const std::string& hiveType);

    // Default number of frames (0 for boxes without frames).
    int DefaultFrameCount(const std::string& boxType, const std::string& hiveType);

    // Default type for newly created frames in this box.
    const char* DefaultFrameType(const std::string& boxType, const std::string& hiveType);

    // Default role when adding a box: honey super above a
    // queen excluder (or for a Flow super), otherwise brood.
    // Empty for boxes without a role.
    const char* DefaultRole(const std::string& boxType, bool stackHasQueenExcluder);

    constexpr int MaxFramesPerBox = 60;

    // ---------------- Roles ----------------

    struct RoleInfo
    {
        const char* key;
        const char* labelKey;
    };

    const std::vector<RoleInfo>& Roles();
    const RoleInfo* FindRole(const std::string& key);

    // ---------------- Frame types ----------------

    struct FrameTypeInfo
    {
        const char* key;
        const char* labelKey;
        const char* shortKey;   // 1-3 letter tile label (localized)
        bool hasComb;           // false: feeder, follower board
    };

    const std::vector<FrameTypeInfo>& FrameTypes();
    const FrameTypeInfo* FindFrameType(const std::string& key);

    // Frames that carry comb (have age / coverage data).
    bool HasComb(const std::string& frameType);
}
