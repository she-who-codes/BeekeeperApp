
#include "FontManager.h"
#include "LayoutUI.h"
#include "Paths.h"

#include <filesystem>
#include <iostream>
#include <string>

namespace
{
    ImFont* g_regular  = nullptr;
    ImFont* g_medium   = nullptr;
    ImFont* g_semiBold = nullptr;
    ImFont* g_bold     = nullptr;

    ImFont* LoadFace(const char* fileName)
    {
        // Found relative to the resources folder wherever
        // BeeKeeper was started from (see Paths.h).
        const std::string path = Paths::Resource(std::string("fonts/") + fileName);

        // ImGui asserts (aborts in Debug builds) on a missing
        // font file, so check first and let Load() fall back
        // to the default font instead.
        std::error_code error;

        if (!std::filesystem::is_regular_file(std::filesystem::u8path(path), error))
        {
            std::cerr << "Warning: font file not found: " << path << std::endl;
            return nullptr;
        }

        ImFontConfig config;
        config.OversampleH = 2;
        config.OversampleV = 2;
        config.PixelSnapH = false;

        return ImGui::GetIO().Fonts->AddFontFromFileTTF(
            path.c_str(),
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
        g_regular  = LoadFace("Inter_18pt-Regular.ttf");
        g_medium   = LoadFace("Inter_18pt-Medium.ttf");
        g_semiBold = LoadFace("Inter_18pt-SemiBold.ttf");
        g_bold     = LoadFace("Inter_18pt-Bold.ttf");

        const bool loaded = g_regular != nullptr;

        // Fall back to ImGui's default font if the
        // bundled font files are unavailable.
        if (!loaded)
        {
            std::cerr
                << "Warning: Could not load Inter fonts. "
                << "Using default ImGui font."
                << std::endl;

            // The scalable built-in font (AddFontDefault()'s
            // automatic choice asserts when the theme changes
            // the font size afterwards).
            g_regular = io.Fonts->AddFontDefaultVector();
        }

        io.FontDefault = g_regular;

        return loaded;
    }
}
