
#include "Theme.h"
#include "LayoutUI.h"

namespace Theme
{
    namespace Colors
    {
        const ImVec4 Forest = ImVec4(0.149f, 0.239f, 0.196f, 1.0f);      // #263D32
        const ImVec4 ForestLight = ImVec4(0.220f, 0.333f, 0.267f, 1.0f); // #385544
        const ImVec4 Sage = ImVec4(0.400f, 0.525f, 0.416f, 1.0f);        // #66866A
        const ImVec4 SageLight = ImVec4(0.596f, 0.690f, 0.600f, 1.0f);   // #98B099
        const ImVec4 Honey = ImVec4(0.839f, 0.663f, 0.310f, 1.0f);       // #D6A94F
        const ImVec4 Cream = ImVec4(0.914f, 0.898f, 0.851f, 1.0f);       // #E9E5D9

        const ImVec4 Background = ImVec4(0.090f, 0.122f, 0.102f, 1.0f);
        const ImVec4 Surface = ImVec4(0.125f, 0.165f, 0.137f, 1.0f);
        const ImVec4 SurfaceRaised = ImVec4(0.169f, 0.220f, 0.184f, 1.0f);
        const ImVec4 Border = ImVec4(0.275f, 0.345f, 0.290f, 1.0f);

        const ImVec4 TextPrimary = ImVec4(0.914f, 0.898f, 0.851f, 1.0f);
        const ImVec4 TextSecondary = ImVec4(0.690f, 0.733f, 0.675f, 1.0f);
        const ImVec4 TextMuted = ImVec4(0.490f, 0.553f, 0.490f, 1.0f);

        const ImVec4 Success = ImVec4(0.427f, 0.690f, 0.494f, 1.0f);
        const ImVec4 Warning = ImVec4(0.839f, 0.663f, 0.310f, 1.0f);
        const ImVec4 Danger = ImVec4(0.800f, 0.345f, 0.310f, 1.0f);

        const ImVec4 Brood = ImVec4(0.722f, 0.525f, 0.361f, 1.0f);      // #B8865C capped-brood tan
        const ImVec4 Pollen = ImVec4(0.878f, 0.541f, 0.235f, 1.0f);     // #E08A3C pollen orange
    }

    void Apply()
    {
        ImGuiStyle &style = ImGui::GetStyle();

        // All dimensions come from LayoutUI.h (single source
        // of truth). They are design pixels at 100% scale;
        // main() applies ScaleAllSizes() for the display DPI.
        style.FontSizeBase = UI::Type::Body;

        style.WindowPadding = ImVec2(UI::Spacing::WindowPaddingX, UI::Spacing::WindowPaddingY);
        style.FramePadding = ImVec2(UI::Spacing::FramePaddingX, UI::Spacing::FramePaddingY);
        style.CellPadding = ImVec2(UI::Spacing::CellPaddingX, UI::Spacing::CellPaddingY);
        style.ItemSpacing = ImVec2(UI::Spacing::ItemSpacingX, UI::Spacing::ItemSpacingY);
        style.ItemInnerSpacing = ImVec2(UI::Spacing::ItemInnerSpacingX, UI::Spacing::ItemInnerSpacingY);
        style.IndentSpacing = UI::Spacing::Indent;
        style.ScrollbarSize = UI::Size::ScrollbarWidth;
        style.GrabMinSize = UI::Size::GrabMin;

        style.WindowRounding = UI::Radius::Window;
        style.ChildRounding = UI::Radius::Child;
        style.FrameRounding = UI::Radius::Frame;
        style.PopupRounding = UI::Radius::Popup;
        style.ScrollbarRounding = UI::Radius::Scrollbar;
        style.GrabRounding = UI::Radius::Grab;
        style.TabRounding = UI::Radius::Tab;

        style.WindowBorderSize = UI::Border::Window;
        style.ChildBorderSize = UI::Border::Child;
        style.FrameBorderSize = UI::Border::Frame;
        style.PopupBorderSize = UI::Border::Popup;

        ImVec4 *colors = style.Colors;

        colors[ImGuiCol_Text] = Colors::TextPrimary;
        colors[ImGuiCol_TextDisabled] = Colors::TextMuted;

        colors[ImGuiCol_WindowBg] = Colors::Background;
        colors[ImGuiCol_ChildBg] = Colors::Surface;
        colors[ImGuiCol_PopupBg] = Colors::SurfaceRaised;
        colors[ImGuiCol_Border] = Colors::Border;

        // Inputs use the darkest surface so they stay visible
        // on both panels (Surface) and dialogs (SurfaceRaised).
        colors[ImGuiCol_FrameBg] = Colors::Background;
        colors[ImGuiCol_FrameBgHovered] = Colors::ForestLight;
        colors[ImGuiCol_FrameBgActive] = Colors::Sage;

        colors[ImGuiCol_TitleBg] = Colors::Forest;
        colors[ImGuiCol_TitleBgActive] = Colors::Forest;
        colors[ImGuiCol_TitleBgCollapsed] = Colors::Forest;

        colors[ImGuiCol_MenuBarBg] = Colors::Forest;

        colors[ImGuiCol_ScrollbarBg] = Colors::Background;
        colors[ImGuiCol_ScrollbarGrab] = Colors::ForestLight;
        colors[ImGuiCol_ScrollbarGrabHovered] = Colors::Sage;
        colors[ImGuiCol_ScrollbarGrabActive] = Colors::SageLight;

        colors[ImGuiCol_CheckMark] = Colors::Honey;
        colors[ImGuiCol_SliderGrab] = Colors::Honey;
        colors[ImGuiCol_SliderGrabActive] = Colors::Cream;

        colors[ImGuiCol_Button] = Colors::ForestLight;
        colors[ImGuiCol_ButtonHovered] = Colors::Sage;
        colors[ImGuiCol_ButtonActive] = Colors::SageLight;

        colors[ImGuiCol_Header] = Colors::ForestLight;
        colors[ImGuiCol_HeaderHovered] = Colors::Sage;
        colors[ImGuiCol_HeaderActive] = Colors::SageLight;

        colors[ImGuiCol_Separator] = Colors::Border;
        colors[ImGuiCol_SeparatorHovered] = Colors::Sage;
        colors[ImGuiCol_SeparatorActive] = Colors::Honey;

        colors[ImGuiCol_ResizeGrip] = Colors::ForestLight;
        colors[ImGuiCol_ResizeGripHovered] = Colors::Sage;
        colors[ImGuiCol_ResizeGripActive] = Colors::Honey;

        colors[ImGuiCol_Tab] = Colors::Forest;
        colors[ImGuiCol_TabHovered] = Colors::Sage;
        colors[ImGuiCol_TabSelected] = Colors::ForestLight;
        colors[ImGuiCol_TabDimmed] = Colors::Background;
        colors[ImGuiCol_TabDimmedSelected] = Colors::Surface;

        colors[ImGuiCol_PlotLines] = Colors::SageLight;
        colors[ImGuiCol_PlotLinesHovered] = Colors::Honey;
        colors[ImGuiCol_PlotHistogram] = Colors::Honey;
        colors[ImGuiCol_PlotHistogramHovered] = Colors::Cream;

        colors[ImGuiCol_TableHeaderBg] = Colors::Forest;
        colors[ImGuiCol_TableBorderStrong] = Colors::Border;
        colors[ImGuiCol_TableBorderLight] = Colors::SurfaceRaised;
        colors[ImGuiCol_TableRowBg] = Colors::Surface;
        colors[ImGuiCol_TableRowBgAlt] = Colors::SurfaceRaised;

        colors[ImGuiCol_ModalWindowDimBg] = ImVec4(
            Colors::Background.x, Colors::Background.y, Colors::Background.z, 0.70f);

        colors[ImGuiCol_TextSelectedBg] = ImVec4(
            Colors::Sage.x, Colors::Sage.y, Colors::Sage.z, 0.45f);

        colors[ImGuiCol_NavCursor] = Colors::Honey;

        // Text links (breadcrumbs) use the accent colour.
        colors[ImGuiCol_TextLink] = Colors::Honey;
    }
}
