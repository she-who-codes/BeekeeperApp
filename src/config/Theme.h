
#pragma once

#include "imgui.h"
#include <cstdint>

namespace Theme
{
    namespace Colors
    {
        // Main palette
        extern const ImVec4 Forest;
        extern const ImVec4 ForestLight;
        extern const ImVec4 Sage;
        extern const ImVec4 SageLight;
        extern const ImVec4 Honey;
        extern const ImVec4 Cream;

        // Surfaces
        extern const ImVec4 Background;
        extern const ImVec4 Surface;
        extern const ImVec4 SurfaceRaised;
        extern const ImVec4 Border;

        // Text
        extern const ImVec4 TextPrimary;
        extern const ImVec4 TextSecondary;
        extern const ImVec4 TextMuted;

        // Status
        extern const ImVec4 Success;
        extern const ImVec4 Warning;
        extern const ImVec4 Danger;

        // Frame coverage (inspections). Honey uses Honey.
        extern const ImVec4 Brood;
        extern const ImVec4 Pollen;
    }

    void Apply();
}
