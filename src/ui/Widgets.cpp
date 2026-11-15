
#include "Widgets.h"

#include "config/Theme.h"
#include "localization/DateFormat.h"

#include "imgui_stdlib.h"

#include <algorithm>

namespace
{
    // Scroll panes remember their last width so that
    // user-resizable panes (ImGuiChildFlags_ResizeX)
    // can compute horizontal overflow correctly.
    ImVector<ImGuiID> g_scrollPaneStack;

    int DateCharFilter(ImGuiInputTextCallbackData* data)
    {
        const std::string* pattern = static_cast<const std::string*>(data->UserData);

        return DateFormat::IsAllowedInput(data->EventChar, *pattern) ? 0 : 1;
    }
}

namespace UI
{
    // ----------------------------------------------
    // Colour helpers
    // ----------------------------------------------

    ImVec4 Mix(const ImVec4& a, const ImVec4& b, float t)
    {
        return ImVec4(
            a.x + (b.x - a.x) * t,
            a.y + (b.y - a.y) * t,
            a.z + (b.z - a.z) * t,
            a.w + (b.w - a.w) * t
        );
    }

    ImVec4 WithAlpha(const ImVec4& color, float alpha)
    {
        return ImVec4(color.x, color.y, color.z, alpha);
    }

    // ----------------------------------------------
    // Buttons
    // ----------------------------------------------

    float ButtonWidth(const char* label)
    {
        const ImGuiStyle& style = ImGui::GetStyle();
        const float textWidth = ImGui::CalcTextSize(label, nullptr, true).x;

        return std::max(
            textWidth + style.FramePadding.x * 2.0f,
            Em(Controls::ButtonMinWidthEm)
        );
    }

    bool Button(const char* label, ButtonKind kind, float height)
    {
        using namespace Theme::Colors;

        if (height == 0.0f)
            height = std::max(Px(Size::ButtonHeight), ImGui::GetFrameHeight());
        else if (height < 0.0f)
            height = ImGui::GetFrameHeight();

        int pushed = 0;

        auto push = [&pushed](ImGuiCol slot, const ImVec4& color)
        {
            ImGui::PushStyleColor(slot, color);
            ++pushed;
        };

        switch (kind)
        {
            case ButtonKind::Primary:
                push(ImGuiCol_Button, Honey);
                push(ImGuiCol_ButtonHovered, Mix(Honey, Cream, 0.25f));
                push(ImGuiCol_ButtonActive, Mix(Honey, Cream, 0.45f));
                push(ImGuiCol_Text, Forest);
                break;

            case ButtonKind::Danger:
                push(ImGuiCol_Button, Mix(Surface, Danger, 0.55f));
                push(ImGuiCol_ButtonHovered, Danger);
                push(ImGuiCol_ButtonActive, Mix(Danger, Cream, 0.25f));
                break;

            case ButtonKind::Subtle:
                push(ImGuiCol_Button, WithAlpha(SurfaceRaised, 0.0f));
                push(ImGuiCol_ButtonHovered, SurfaceRaised);
                push(ImGuiCol_ButtonActive, ForestLight);
                break;

            case ButtonKind::Secondary:
                break;
        }

        const bool pressed = ImGui::Button(label, ImVec2(ButtonWidth(label), height));

        ImGui::PopStyleColor(pushed);
        return pressed;
    }

    // ----------------------------------------------
    // Flow layout
    // ----------------------------------------------

    void SameLineOrWrap(float nextWidth)
    {
        ImGui::SameLine();

        if (ImGui::GetContentRegionAvail().x < nextWidth)
            ImGui::NewLine();
    }

    // ----------------------------------------------
    // Text
    // ----------------------------------------------

    void TextStyled(const TextStyle& style, const ImVec4& color, const char* text)
    {
        ScopedFont font(style);
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::TextUnformatted(text);
        ImGui::PopStyleColor();
    }

    void TextStyledWrapped(const TextStyle& style, const ImVec4& color, const char* text)
    {
        ScopedFont font(style);
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextUnformatted(text);
        ImGui::PopTextWrapPos();
        ImGui::PopStyleColor();
    }

    std::string Ellipsize(const std::string& utf8, size_t maxCharacters)
    {
        size_t characters = 0;

        for (size_t i = 0; i < utf8.size(); ++i)
        {
            // Count lead bytes only (skip 10xxxxxx).
            if ((static_cast<unsigned char>(utf8[i]) & 0xC0) != 0x80)
            {
                if (characters == maxCharacters)
                    return utf8.substr(0, i) + "\xE2\x80\xA6"; // U+2026 …

                ++characters;
            }
        }

        return utf8;
    }

    float StatusChipWidth(const char* text)
    {
        ScopedFont font(Text::Caption);
        return ImGui::CalcTextSize(text).x + Em(1.2f);
    }

    void StatusChip(const char* text, const ImVec4& color)
    {
        ScopedFont font(Text::Caption);

        const ImVec2 textSize = ImGui::CalcTextSize(text);
        const ImVec2 padding(Em(0.6f), Em(0.18f));
        const ImVec2 size(textSize.x + padding.x * 2.0f, textSize.y + padding.y * 2.0f);
        const ImVec2 position = ImGui::GetCursorScreenPos();

        ImDrawList* drawList = ImGui::GetWindowDrawList();

        drawList->AddRectFilled(
            position,
            ImVec2(position.x + size.x, position.y + size.y),
            ImGui::GetColorU32(WithAlpha(color, 0.18f)),
            Px(Radius::Chip)
        );

        drawList->AddText(
            ImVec2(position.x + padding.x, position.y + padding.y),
            ImGui::GetColorU32(color),
            text
        );

        ImGui::Dummy(size);
    }

    // ----------------------------------------------
    // Forms
    // ----------------------------------------------

    void FormLabel(const char* text, bool required)
    {
        ScopedFont font(Text::Label);
        ImGui::TextUnformatted(text);

        if (required)
        {
            ImGui::SameLine(0.0f, Px(4.0f));
            ImGui::TextColored(Theme::Colors::Honey, "*");
        }
    }

    void FieldHelp(const char* text)
    {
        TextStyledWrapped(Text::Caption, Theme::Colors::TextMuted, text);
    }

    void FieldError(const std::string& message)
    {
        if (message.empty())
            return;

        TextStyledWrapped(Text::Caption, Theme::Colors::Danger, message.c_str());
    }

    bool DateInput(
        const char* id,
        std::string& text,
        const std::string& pattern,
        const char* hint,
        const char* todayLabel)
    {
        const ImGuiStyle& style = ImGui::GetStyle();

        const float todayWidth = ButtonWidth(todayLabel);
        const float inputWidth = std::max(
            Em(8.0f),
            ImGui::GetContentRegionAvail().x - todayWidth - style.ItemSpacing.x
        );

        ImGui::SetNextItemWidth(inputWidth);

        bool changed = ImGui::InputTextWithHint(
            id,
            hint,
            &text,
            ImGuiInputTextFlags_CallbackCharFilter,
            DateCharFilter,
            const_cast<std::string*>(&pattern)
        );

        SameLineOrWrap(todayWidth);

        // Scope the button ID to this field so several date
        // inputs can share the same "Today" label.
        ImGui::PushID(id);

        if (Button(todayLabel, ButtonKind::Secondary, -1.0f))
        {
            text = DateFormat::Format(DateFormat::Today(), pattern);
            changed = true;
        }

        ImGui::PopID();

        return changed;
    }

    // ----------------------------------------------
    // Scroll panes
    // ----------------------------------------------

    bool BeginScrollPane(
        const char* id,
        const ImVec2& size,
        float minContentWidth,
        ImGuiChildFlags childFlags,
        ImGuiWindowFlags windowFlags)
    {
        const ImGuiStyle& style = ImGui::GetStyle();
        const ImGuiID key = ImGui::GetID(id);

        // Width the pane will occupy this frame.
        const float available = ImGui::GetContentRegionAvail().x;

        float width = size.x > 0.0f
            ? size.x
            : available + size.x;   // 0 = fill, <0 = fill minus

        if (childFlags & ImGuiChildFlags_ResizeX)
        {
            const float remembered = ImGui::GetStateStorage()->GetFloat(key, -1.0f);

            if (remembered > 0.0f)
                width = remembered;
        }

        const bool padded =
            (childFlags & (ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding)) != 0;

        const float inner = width
            - (padded ? style.WindowPadding.x * 2.0f : 0.0f)
            - style.ScrollbarSize;

        // Only force a content width when the pane is too
        // narrow; otherwise content fills the pane.
        if (minContentWidth > 0.0f && inner < minContentWidth)
            ImGui::SetNextWindowContentSize(ImVec2(minContentWidth, 0.0f));

        g_scrollPaneStack.push_back(key);

        return ImGui::BeginChild(
            id,
            size,
            childFlags,
            windowFlags | ImGuiWindowFlags_HorizontalScrollbar
        );
    }

    void EndScrollPane()
    {
        ImGui::EndChild();

        if (!g_scrollPaneStack.empty())
        {
            ImGui::GetStateStorage()->SetFloat(
                g_scrollPaneStack.back(),
                ImGui::GetItemRectSize().x
            );

            g_scrollPaneStack.pop_back();
        }
    }

    // ----------------------------------------------
    // Feedback
    // ----------------------------------------------

    bool Banner(
        const char* id,
        const std::string& text,
        BannerKind kind,
        const char* dismissTooltip)
    {
        using namespace Theme::Colors;

        const ImVec4 accent = kind == BannerKind::Error ? Danger : Success;

        ImGui::PushStyleColor(ImGuiCol_ChildBg, Mix(Surface, accent, 0.16f));
        ImGui::PushStyleColor(ImGuiCol_Border, WithAlpha(accent, 0.6f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, Px(Spacing::Panel * 0.75f, Spacing::Panel * 0.5f));

        bool dismissed = false;

        if (ImGui::BeginChild(
            id,
            ImVec2(0.0f, 0.0f),
            ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY))
        {
            const float closeWidth = ImGui::GetFrameHeight();

            // Window-local x of the right content edge.
            const float rightEdge =
                ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x;

            ImGui::PushTextWrapPos(
                rightEdge - closeWidth - ImGui::GetStyle().ItemSpacing.x
            );
            ImGui::AlignTextToFramePadding();
            ImGui::TextColored(accent, "%s", text.c_str());
            ImGui::PopTextWrapPos();

            ImGui::SameLine(rightEdge - closeWidth);

            ImGui::PushStyleColor(ImGuiCol_Button, WithAlpha(accent, 0.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, WithAlpha(accent, 0.25f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, WithAlpha(accent, 0.4f));

            // U+00D7 multiplication sign as the close glyph.
            if (ImGui::Button("\xC3\x97###Dismiss", ImVec2(closeWidth, closeWidth)))
                dismissed = true;

            ImGui::PopStyleColor(3);

            if (dismissTooltip && ImGui::BeginItemTooltip())
            {
                ImGui::TextUnformatted(dismissTooltip);
                ImGui::EndTooltip();
            }
        }

        ImGui::EndChild();

        ImGui::PopStyleVar();
        ImGui::PopStyleColor(2);

        return dismissed;
    }

    // ----------------------------------------------
    // Dialogs
    // ----------------------------------------------

    void SetNextDialogLayout(float widthEm)
    {
        const ImGuiIO& io = ImGui::GetIO();

        const float maxWidth = io.DisplaySize.x * Dialog::MaxScreenFraction;
        const float maxHeight = io.DisplaySize.y * Dialog::MaxScreenFraction;

        const float width = std::min(Em(widthEm), maxWidth);
        const float minWidth = std::min(Em(Dialog::MinWidthEm), maxWidth);

        ImGui::SetNextWindowPos(
            ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
            ImGuiCond_Always,
            ImVec2(0.5f, 0.5f)
        );

        // Height 0 = auto-fit every frame (capped below).
        ImGui::SetNextWindowSize(ImVec2(width, 0.0f));

        ImGui::SetNextWindowSizeConstraints(
            ImVec2(minWidth, 0.0f),
            ImVec2(maxWidth, maxHeight)
        );
    }

    // Footer height is measured each frame and stored in
    // the dialog window's state storage.
    void BeginDialogBody(const char* id)
    {
        const ImGuiStyle& style = ImGui::GetStyle();
        const ImGuiIO& io = ImGui::GetIO();

        ImGuiStorage* storage = ImGui::GetStateStorage();

        const float footerHeight = storage->GetFloat(
            ImGui::GetID("##DialogFooterHeight"),
            Px(Size::ButtonHeight) + style.ItemSpacing.y * 4.0f
        );

        // Screen budget minus title bar, window padding,
        // footer and the gap between body and footer.
        const float maxBodyHeight = std::max(
            Em(4.0f),
            io.DisplaySize.y * Dialog::MaxScreenFraction
                - ImGui::GetFrameHeight()
                - style.WindowPadding.y * 2.0f
                - footerHeight
                - style.ItemSpacing.y
        );

        ImGui::SetNextWindowSizeConstraints(
            ImVec2(0.0f, 0.0f),
            ImVec2(FLT_MAX, maxBodyHeight)
        );

        // Transparent: the body blends into the dialog.
        ImGui::PushStyleColor(ImGuiCol_ChildBg, WithAlpha(Theme::Colors::SurfaceRaised, 0.0f));

        ImGui::BeginChild(
            id,
            ImVec2(0.0f, 0.0f),
            ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_NavFlattened
        );

        ImGui::PopStyleColor();
    }

    void EndDialogBody()
    {
        ImGui::EndChild();

        ImGui::GetStateStorage()->SetFloat(
            ImGui::GetID("##DialogFooterStart"),
            ImGui::GetCursorPosY()
        );
    }

    void EndDialogFooter()
    {
        ImGuiStorage* storage = ImGui::GetStateStorage();

        const float start = storage->GetFloat(ImGui::GetID("##DialogFooterStart"), 0.0f);

        storage->SetFloat(
            ImGui::GetID("##DialogFooterHeight"),
            std::max(0.0f, ImGui::GetCursorPosY() - start)
        );
    }
}
