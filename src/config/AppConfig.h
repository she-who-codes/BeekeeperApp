
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

    namespace Layout
    {
        constexpr float MainPadding = 20.0f;
        constexpr float PanelPadding = 16.0f;
        constexpr float ItemSpacing = 10.0f;
        constexpr float SectionSpacing = 24.0f;
        constexpr float ButtonHeight = 38.0f;
        constexpr float InputHeight = 36.0f;
        constexpr float CornerRadius = 8.0f;
    }

    namespace FrameEditor
    {
        constexpr int GridColumns = 20;
        constexpr int GridRows = 40;
        constexpr float CellGap = 1.0f;
    }
}
