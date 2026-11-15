
#pragma once

#include <cstring>
#include <string>

// ==================================================
// Hive types
// --------------------------------------------------
// The database stores the stable KEY (never the
// translated text). The UI shows Tr(labelKey).
// "other" stores the user's description in
// hives.hive_type_other.
// ==================================================

namespace HiveTypes
{
    struct Entry
    {
        const char* key;        // stored in hives.hive_type
        const char* labelKey;   // localization key
    };

    constexpr const char* Other = "other";

    inline constexpr Entry All[] =
    {
        { "langstroth_8",    "hive_type.langstroth_8"    },
        { "langstroth_10",   "hive_type.langstroth_10"   },
        { "long_langstroth", "hive_type.long_langstroth" },
        { "top_bar",         "hive_type.top_bar"         },
        { "warre",           "hive_type.warre"           },
        { "layens",          "hive_type.layens"          },
        { "flow",            "hive_type.flow"            },
        { "other",           "hive_type.other"           },
    };

    constexpr int Count = static_cast<int>(sizeof(All) / sizeof(All[0]));

    // Index into All[], or -1 if unknown / empty.
    inline int IndexOf(const std::string& key)
    {
        for (int i = 0; i < Count; ++i)
        {
            if (key == All[i].key)
                return i;
        }

        return -1;
    }

    inline bool IsOther(const std::string& key)
    {
        return key == Other;
    }
}
