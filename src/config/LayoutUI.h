
#pragma once

// ==================================================
// LayoutUI.h
// --------------------------------------------------
// SINGLE SOURCE OF TRUTH for every size, spacing,
// radius, type size and breakpoint used by the UI.
//
//  * Colours live in Theme (config/Theme.h).
//  * Font faces live in FontManager (config/FontManager.h).
//  * Everything measurable lives here.
//
// All values are "design pixels" at 100% display
// scale. Convert them at the point of use with:
//
//   UI::Px(v)  -> fixed size, scaled by display DPI
//   UI::Em(n)  -> n x root font size. Prefer Em for
//                 anything that holds text so layouts
//                 grow with font size and with longer
//                 translated strings.
// ==================================================

#include "imgui.h"
#include "config/FontManager.h"

#include <algorithm>

namespace UI
{
    // ----------------------------------------------
    // Display scale
    // ----------------------------------------------

    namespace Detail
    {
        inline float g_dpiScale = 1.0f;
    }

    // Call once at start-up after the DPI is known.
    inline void SetDpiScale(float scale)
    {
        Detail::g_dpiScale = scale > 0.0f ? scale : 1.0f;
    }

    inline float DpiScale()
    {
        return Detail::g_dpiScale;
    }

    inline float Px(float designPixels)
    {
        return designPixels * Detail::g_dpiScale;
    }

    inline ImVec2 Px(float x, float y)
    {
        return ImVec2(Px(x), Px(y));
    }

    // ----------------------------------------------
    // Typography (sizes in design pixels)
    // ----------------------------------------------

    namespace Type
    {
        constexpr float Caption  = 13.0f;  // secondary / meta text
        constexpr float Body     = 16.0f;  // default text, inputs
        constexpr float Label    = 16.0f;  // form labels, section labels
        constexpr float Title    = 18.0f;  // card titles
        constexpr float Heading  = 22.0f;  // pane headings
        constexpr float Display  = 28.0f;  // page titles
    }

    struct TextStyle
    {
        Fonts::Face face;
        float size;
    };

    namespace Text
    {
        constexpr TextStyle Caption { Fonts::Face::Regular,  Type::Caption };
        constexpr TextStyle Body    { Fonts::Face::Regular,  Type::Body    };
        constexpr TextStyle Label   { Fonts::Face::Medium,   Type::Label   };
        constexpr TextStyle Strong  { Fonts::Face::Bold,     Type::Body    };
        constexpr TextStyle Title   { Fonts::Face::SemiBold, Type::Title   };
        constexpr TextStyle Heading { Fonts::Face::SemiBold, Type::Heading };
        constexpr TextStyle Display { Fonts::Face::Bold,     Type::Display };
    }

    // Root font size in screen pixels (includes DPI and
    // any global font scaling). 1 Em == one Body line.
    inline float RootFontSize()
    {
        const ImGuiStyle& style = ImGui::GetStyle();
        const float base = style.FontSizeBase > 0.0f
            ? style.FontSizeBase
            : Type::Body;

        return base * style.FontScaleMain * style.FontScaleDpi;
    }

    inline float Em(float multiple)
    {
        return multiple * RootFontSize();
    }

    // Screen-pixel height of a line in the given style.
    inline float LineHeight(const TextStyle& text)
    {
        const ImGuiStyle& style = ImGui::GetStyle();
        return text.size * style.FontScaleMain * style.FontScaleDpi;
    }

    // RAII font push:  { UI::ScopedFont f(UI::Text::Heading); ... }
    class ScopedFont
    {
    public:
        explicit ScopedFont(const TextStyle& text)
        {
            // PushFont() takes the size BEFORE global scale
            // factors; ImGui applies FontScaleDpi itself.
            ImGui::PushFont(Fonts::Get(text.face), text.size);
        }

        ~ScopedFont()
        {
            ImGui::PopFont();
        }

        ScopedFont(const ScopedFont&) = delete;
        ScopedFont& operator=(const ScopedFont&) = delete;
    };

    // ----------------------------------------------
    // Control sizes (design pixels)
    // ----------------------------------------------

    namespace Size
    {
        constexpr float ButtonHeight   = 38.0f;
        constexpr float InputHeight    = 36.0f;
        constexpr float ScrollbarWidth = 12.0f;
        constexpr float GrabMin        = 10.0f;

        // Minimum tap target for field use (gloves / bee
        // suit). Used by field-logging controls such as
        // the frame coverage picker.
        constexpr float TouchTarget    = 56.0f;
    }

    // ----------------------------------------------
    // Spacing (design pixels)
    // ----------------------------------------------

    namespace Spacing
    {
        // ImGui style
        constexpr float WindowPaddingX    = 20.0f;
        constexpr float WindowPaddingY    = 20.0f;
        constexpr float FramePaddingX     = 12.0f;
        // Derived so framed widgets (inputs, combos) are
        // exactly Size::InputHeight tall at Body size.
        constexpr float FramePaddingY     = (Size::InputHeight - Type::Body) * 0.5f;
        constexpr float CellPaddingX      = 10.0f;
        constexpr float CellPaddingY      = 8.0f;
        constexpr float ItemSpacingX      = 10.0f;
        constexpr float ItemSpacingY      = 10.0f;
        constexpr float ItemInnerSpacingX = 8.0f;
        constexpr float ItemInnerSpacingY = 6.0f;
        constexpr float Indent            = 22.0f;

        // Page layout
        constexpr float PageMin           = 16.0f;  // page side padding, small windows
        constexpr float PageMax           = 40.0f;  // page side padding, large windows
        constexpr float PageRatio         = 0.02f;  // side padding as fraction of width
        constexpr float PageTop           = 20.0f;
        constexpr float Panel             = 16.0f;  // inner padding of panes / cards
        constexpr float Section           = 24.0f;  // gap between page sections
        constexpr float Gutter            = 12.0f;  // gap between panes / grid cards
    }

    // ----------------------------------------------
    // Corner radius (design pixels)
    // ----------------------------------------------

    namespace Radius
    {
        constexpr float Window    = 10.0f;
        constexpr float Child     = 8.0f;
        constexpr float Frame     = 6.0f;
        constexpr float Popup     = 8.0f;
        constexpr float Scrollbar = 8.0f;
        constexpr float Grab      = 6.0f;
        constexpr float Tab       = 6.0f;
        constexpr float Chip      = 10.0f;
    }

    // ----------------------------------------------
    // Border widths (design pixels)
    // ----------------------------------------------

    namespace Border
    {
        constexpr float Window = 0.0f;
        constexpr float Child  = 1.0f;
        constexpr float Frame  = 0.0f;
        constexpr float Popup  = 1.0f;
    }

    // ----------------------------------------------
    // Responsive breakpoints (design pixels, compare
    // against the DPI-scaled window width via Px())
    // ----------------------------------------------

    namespace Breakpoint
    {
        constexpr float Compact = 900.0f;
        constexpr float Wide    = 1400.0f;
    }

    // ----------------------------------------------
    // Panes (Em units so they follow text size)
    // ----------------------------------------------

    namespace Pane
    {
        constexpr float SidebarDefaultEm  = 17.0f;
        constexpr float SidebarMinEm      = 12.0f;
        constexpr float SidebarMaxEm      = 28.0f;
        constexpr float SidebarMaxRatio   = 0.45f;  // of available width
        constexpr float SidebarMaxRatioCompact = 0.30f;  // below Breakpoint::Compact

        // Below this width a pane scrolls horizontally
        // instead of squeezing its content.
        constexpr float SidebarContentMinEm = 8.0f;   // < SidebarMinEm minus padding + scrollbar
        constexpr float ContentMinEm        = 22.0f;
    }

    // ----------------------------------------------
    // Cards & lists (Em units)
    // ----------------------------------------------

    namespace Card
    {
        constexpr float HiveMinWidthEm   = 17.0f;
        constexpr int   HiveMaxColumns   = 4;
        constexpr int   NotesPreviewChars = 140;
    }

    namespace List
    {
        constexpr float RowPaddingYEm = 0.45f;
    }

    // ----------------------------------------------
    // Dialogs & forms (Em units)
    // ----------------------------------------------

    namespace Dialog
    {
        constexpr float WidthEm            = 30.0f;
        constexpr float MinWidthEm         = 20.0f;
        constexpr float MaxScreenFraction  = 0.92f;
        constexpr float NotesHeightEm      = 5.5f;
        constexpr float ConfirmWidthEm     = 26.0f;
    }

    namespace Controls
    {
        constexpr float ButtonMinWidthEm = 6.0f;
    }
}
