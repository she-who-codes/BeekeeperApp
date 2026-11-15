
#include "FontManager.h"
#include "LayoutUI.h"

#include <iostream>

namespace
{
    ImFont* g_regular  = nullptr;
    ImFont* g_medium   = nullptr;
    ImFont* g_semiBold = nullptr;
    ImFont* g_bold     = nullptr;

    ImFont* LoadFace(const char* path)
    {
        ImFontConfig config;
        config.OversampleH = 2;
        config.OversampleV = 2;
        config.PixelSnapH = false;

        return ImGui::GetIO().Fonts->AddFontFromFileTTF(
            path,
            UI::Type::Body,
            &config
        );
    }
}

namespace Fonts
{
    ImFont* Get(Face face)
    {
        ImFont* font = nullptr;

        switch (face)
        {
            case Face::Regular:  font = g_regular;  break;
            case Face::Medium:   font = g_medium;   break;
            case Face::SemiBold: font = g_semiBold; break;
            case Face::Bold:     font = g_bold;     break;
        }

        return font ? font : g_regular;
    }

    bool Load()
    {
        ImGuiIO& io = ImGui::GetIO();

        // Regular is loaded first so it becomes Fonts[0].
        g_regular  = LoadFace("resources/fonts/Inter_18pt-Regular.ttf");
        g_medium   = LoadFace("resources/fonts/Inter_18pt-Medium.ttf");
        g_semiBold = LoadFace("resources/fonts/Inter_18pt-SemiBold.ttf");
        g_bold     = LoadFace("resources/fonts/Inter_18pt-Bold.ttf");

        const bool loaded = g_regular != nullptr;

        // Fall back to ImGui's default font if the
        // bundled font files are unavailable.
        if (!loaded)
        {
            std::cerr
                << "Warning: Could not load Inter fonts. "
                << "Using default ImGui font."
                << std::endl;

            g_regular = io.Fonts->AddFontDefault();
        }

        io.FontDefault = g_regular;

        return loaded;
    }
}
