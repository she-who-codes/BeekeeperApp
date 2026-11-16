
#include "HiveEquipment.h"

namespace
{
    using namespace HiveEquipment;

    const BoxTypeInfo g_boxTypes[] =
    {
        // key                label key                        frames  role
        { "deep",             "box_type.deep",                 true,   true  },
        { "medium",           "box_type.medium",               true,   true  },
        { "shallow",          "box_type.shallow",              true,   true  },
        { "warre_box",        "box_type.warre_box",            true,   true  },
        { "flow_super",       "box_type.flow_super",           true,   false },
        { "horizontal_body",  "box_type.horizontal_body",      true,   false },
        { "queen_excluder",   "box_type.queen_excluder",       false,  false },
        { "feeder",           "box_type.feeder",               false,  false },
    };

    const std::vector<RoleInfo> g_roles =
    {
        { "brood",       "box_role.brood"       },
        { "honey_super", "box_role.honey_super" },
    };

    const std::vector<FrameTypeInfo> g_frameTypes =
    {
        { "wax_foundation",     "frame_type.wax_foundation",     "frame_type_short.wax_foundation",     true  },
        { "plastic_foundation", "frame_type.plastic_foundation", "frame_type_short.plastic_foundation", true  },
        { "foundationless",     "frame_type.foundationless",     "frame_type_short.foundationless",     true  },
        { "flow_frame",         "frame_type.flow_frame",         "frame_type_short.flow_frame",         true  },
        { "feeder",             "frame_type.feeder",             "frame_type_short.feeder",             false },
        { "follower_board",     "frame_type.follower_board",     "frame_type_short.follower_board",     false },
    };
}

namespace HiveEquipment
{
    const BoxTypeInfo* FindBoxType(const std::string& key)
    {
        for (const BoxTypeInfo& info : g_boxTypes)
        {
            if (key == info.key)
                return &info;
        }

        return nullptr;
    }

    std::vector<const BoxTypeInfo*> AllowedBoxTypes(const std::string& hiveType)
    {
        std::vector<const char*> keys;

        if (hiveType == "langstroth_8" || hiveType == "langstroth_10")
            keys = { "deep", "medium", "shallow", "queen_excluder", "feeder" };
        else if (hiveType == "flow")
            keys = { "deep", "medium", "flow_super", "shallow", "queen_excluder", "feeder" };
        else if (hiveType == "warre")
            keys = { "warre_box", "feeder" };
        else if (hiveType == "top_bar" || hiveType == "layens")
            keys = { "horizontal_body" };
        else if (hiveType == "long_langstroth")
            keys = { "horizontal_body", "medium", "shallow", "queen_excluder", "feeder" };

        std::vector<const BoxTypeInfo*> result;

        if (keys.empty())
        {
            // "other" / unspecified hive type: everything.
            for (const BoxTypeInfo& info : g_boxTypes)
                result.push_back(&info);
        }
        else
        {
            for (const char* key : keys)
                result.push_back(FindBoxType(key));
        }

        return result;
    }

    int DefaultFrameCount(const std::string& boxType, const std::string& hiveType)
    {
        const BoxTypeInfo* info = FindBoxType(boxType);

        if (!info || !info->holdsFrames)
            return 0;

        if (boxType == "warre_box")
            return 8;

        if (boxType == "flow_super")
            return 6;

        if (boxType == "horizontal_body")
        {
            if (hiveType == "long_langstroth") return 30;
            if (hiveType == "layens")          return 20;
            return 24;   // top bar and others
        }

        // Langstroth-style deep / medium / shallow.
        if (hiveType == "langstroth_8" || hiveType == "flow")
            return 8;

        return 10;
    }

    const char* DefaultFrameType(const std::string& boxType, const std::string& hiveType)
    {
        if (boxType == "flow_super")
            return "flow_frame";

        if (boxType == "warre_box" || (boxType == "horizontal_body" && hiveType == "top_bar"))
            return "foundationless";

        return "wax_foundation";
    }

    const char* DefaultRole(const std::string& boxType, bool stackHasQueenExcluder)
    {
        const BoxTypeInfo* info = FindBoxType(boxType);

        if (boxType == "flow_super")
            return "honey_super";

        if (!info || !info->hasRole)
            return "";

        return stackHasQueenExcluder ? "honey_super" : "brood";
    }

    const std::vector<RoleInfo>& Roles()
    {
        return g_roles;
    }

    const RoleInfo* FindRole(const std::string& key)
    {
        for (const RoleInfo& info : g_roles)
        {
            if (key == info.key)
                return &info;
        }

        return nullptr;
    }

    const std::vector<FrameTypeInfo>& FrameTypes()
    {
        return g_frameTypes;
    }

    const FrameTypeInfo* FindFrameType(const std::string& key)
    {
        for (const FrameTypeInfo& info : g_frameTypes)
        {
            if (key == info.key)
                return &info;
        }

        return nullptr;
    }

    bool HasComb(const std::string& frameType)
    {
        const FrameTypeInfo* info = FindFrameType(frameType);
        return info ? info->hasComb : true;
    }
}
