
#pragma once

#include "imgui.h"

// ==================================================
// FontManager
// --------------------------------------------------
// Loads one ImFont per Inter weight. Since Dear ImGui
// 1.92 fonts are resized dynamically, so sizes are NOT
// baked here: they come from UI::Type in LayoutUI.h and
// DPI scaling is applied globally via style.FontScaleDpi.
// ==================================================

namespace Fonts
{
    enum class Face
    {
        Regular,
        Medium,
        SemiBold,
        Bold
    };

    // Returns the requested face (falls back to Regular,
    // then to ImGui's default font if files are missing).
    ImFont* Get(Face face);

    // Loads all faces from resources/fonts. Returns false
    // if the Inter fonts could not be loaded.
    bool Load();
}
