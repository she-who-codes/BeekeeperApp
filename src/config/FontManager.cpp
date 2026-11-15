
#include "FontManager.h"

#include <iostream>

namespace Fonts
{
    ImFont* Small = nullptr;
    ImFont* Body = nullptr;
    ImFont* Heading = nullptr;
    ImFont* Bold = nullptr;

    bool Load(float dpiScale)
    {
        ImGuiIO& io = ImGui::GetIO();

        if (dpiScale <= 0.0f)
            dpiScale = 1.0f;

        const float smallSize = 13.0f * dpiScale;
        const float bodySize = 16.0f * dpiScale;
        const float headingSize = 22.0f * dpiScale;
        const float boldSize = 16.0f * dpiScale;

        ImFontConfig config;
        config.OversampleH = 2;
        config.OversampleV = 2;
        config.PixelSnapH = false;

   Small = io.Fonts->AddFontFromFileTTF(
    "resources/fonts/Inter_18pt-Regular.ttf",
    smallSize,
    &config
);

Body = io.Fonts->AddFontFromFileTTF(
    "resources/fonts/Inter_18pt-Regular.ttf",
    bodySize,
    &config
);

Heading = io.Fonts->AddFontFromFileTTF(
    "resources/fonts/Inter_18pt-SemiBold.ttf",
    headingSize,
    &config
);

Bold = io.Fonts->AddFontFromFileTTF(
    "resources/fonts/Inter_18pt-Bold.ttf",
    boldSize,
    &config
);

        // Fall back to ImGui's default font if
        // the bundled font files are unavailable.
        if (!Body)
        {
            std::cerr
                << "Warning: Could not load Inter fonts. "
                << "Using default ImGui font."
                << std::endl;

            Body = io.Fonts->AddFontDefault();
        }

        if (!Small)
            Small = Body;

        if (!Heading)
            Heading = Body;

        if (!Bold)
            Bold = Body;

        io.FontDefault = Body;

        return Body != nullptr;
    }
}
