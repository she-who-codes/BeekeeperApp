
#pragma once

namespace AppConfig
{
    namespace Window
    {
        constexpr int DefaultWidth = 1440;
        constexpr int DefaultHeight = 900;
        constexpr int MinimumWidth = 1024;
        constexpr int MinimumHeight = 700;
    }

    // Layout values moved to config/LayoutUI.h
    // (single source of truth for UI dimensions).

    namespace FrameEditor
    {
        constexpr int GridColumns = 20;
        constexpr int GridRows = 40;
        constexpr float CellGap = 1.0f;
    }
}
