
#include "Widgets.h"

#include "config/Theme.h"
#include "localization/DateFormat.h"

#include "imgui_internal.h"    // ImGuiTable (TableFitWidth)
#include "imgui_stdlib.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <unordered_map>

namespace
{
    // Scroll panes remember their last width so that
    // user-resizable panes (ImGuiChildFlags_ResizeX)
    // can compute horizontal overflow correctly.
    ImVector<ImGuiID> g_scrollPaneStack;

    // Text being edited in OptionalNumberInput fields,
    // keyed by ImGui ID (kept only while active).
    std::unordered_map<ImGuiID, std::string> g_numberText;

    int NumberCharFilter(ImGuiInputTextCallbackData* data)
    {
        const ImWchar c = data->EventChar;
        return ((c >= '0' && c <= '9') || c == '.') ? 0 : 1;
    }

    int SignedNumberCharFilter(ImGuiInputTextCallbackData* data)
    {
        const ImWchar c = data->EventChar;
        return ((c >= '0' && c <= '9') || c == '.' || c == '-') ? 0 : 1;
    }

    std::string FormatNumber(const std::optional<double>& value)
    {
        if (!value)
            return "";

        char buffer[32];
        std::snprintf(buffer, sizeof(buffer), "%g", *value);
        return buffer;
    }

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
                push(ImGuiCol_Border, Mix(Honey, Forest, 0.35f));
                break;

            case ButtonKind::Danger:
                push(ImGuiCol_Button, Mix(Surface, Danger, 0.55f));
                push(ImGuiCol_ButtonHovered, Danger);
                push(ImGuiCol_ButtonActive, Mix(Danger, Cream, 0.25f));
                push(ImGuiCol_Border, Danger);
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

        // Visible content width with room reserved for a
        // vertical scrollbar.
        const float inner = width
            - (padded ? style.WindowPadding.x * 2.0f : 0.0f)
            - style.ScrollbarSize;

        // Always set the content width explicitly: max of the
        // minimum and the visible width. Letting ImGui measure
        // it would let wrapped text keep a horizontal
        // scrollbar alive after a vertical one appears (text
        // wraps to the measured width, which then stays wide).
        ImGui::SetNextWindowContentSize(ImVec2(std::max(minContentWidth, inner), 0.0f));

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
    // Choice & number inputs
    // ----------------------------------------------

    bool SegmentedControl(
        const char* id,
        const char* const* labels,
        int count,
        int& selected,
        float height,
        bool allowNone,
        const bool* enabled)
    {
        using namespace Theme::Colors;

        if (count <= 0)
            return false;

        if (height <= 0.0f)
            height = Px(Size::TouchTarget);

        const ImGuiStyle& style = ImGui::GetStyle();

        // Equal widths: the widest label decides.
        float width = Em(Controls::ButtonMinWidthEm);

        for (int i = 0; i < count; ++i)
            width = std::max(width, ImGui::CalcTextSize(labels[i], nullptr, true).x + style.FramePadding.x * 2.0f);

        bool changed = false;

        ImGui::PushID(id);

        for (int i = 0; i < count; ++i)
        {
            if (i > 0)
                SameLineOrWrap(width);

            const bool active = i == selected;

            // Unselected segments use the normal button colour so
            // they are visible on panels and in dialogs.
            ImGui::PushStyleColor(ImGuiCol_Button, active ? Honey : ForestLight);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, active ? Mix(Honey, Cream, 0.25f) : Sage);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, active ? Mix(Honey, Cream, 0.45f) : SageLight);
            ImGui::PushStyleColor(ImGuiCol_Text, active ? Forest : TextPrimary);

            ImGui::PushID(i);

            const bool disabled = enabled && !enabled[i];
            ImGui::BeginDisabled(disabled);

            if (ImGui::Button(labels[i], ImVec2(width, height)))
            {
                if (!active)
                {
                    selected = i;
                    changed = true;
                }
                else if (allowNone)
                {
                    selected = -1;
                    changed = true;
                }
            }

            ImGui::EndDisabled();
            ImGui::PopID();
            ImGui::PopStyleColor(4);
        }

        ImGui::PopID();
        return changed;
    }

    bool OptionalNumberInput(const char* id, std::optional<double>& value, const char* hint, bool allowNegative)
    {
        const ImGuiID key = ImGui::GetID(id);

        auto it = g_numberText.find(key);

        std::string text = it != g_numberText.end() ? it->second : FormatNumber(value);

        bool changed = false;

        if (ImGui::InputTextWithHint(id, hint, &text, ImGuiInputTextFlags_CallbackCharFilter,
            allowNegative ? SignedNumberCharFilter : NumberCharFilter))
        {
            std::optional<double> parsed;

            if (!text.empty() && text != "." && text != "-" && text != "-.")
            {
                char* end = nullptr;
                const double number = std::strtod(text.c_str(), &end);

                if (end && *end == '\0')
                    parsed = number;
                else
                    parsed = value;   // e.g. "1.2." - keep previous value
            }

            if (parsed != value)
            {
                value = parsed;
                changed = true;
            }
        }

        // Keep the raw text only while editing so the user
        // can type "1." etc.; otherwise show the value.
        if (ImGui::IsItemActive())
            g_numberText[key] = text;
        else
            g_numberText.erase(key);

        return changed;
    }

    bool ToggleButton(const char* label, bool& value, const ImVec4& onColor, float height)
    {
        using namespace Theme::Colors;

        if (height <= 0.0f)
            height = Px(Size::TouchTarget);

        const float width = std::max(
            ImGui::CalcTextSize(label, nullptr, true).x + ImGui::GetStyle().FramePadding.x * 2.0f,
            Em(Controls::ButtonMinWidthEm));

        ImGui::PushStyleColor(ImGuiCol_Button, value ? onColor : ForestLight);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, value ? Mix(onColor, Cream, 0.2f) : Sage);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, value ? Mix(onColor, Cream, 0.35f) : SageLight);
        ImGui::PushStyleColor(ImGuiCol_Text, value ? Forest : TextPrimary);

        const bool pressed = ImGui::Button(label, ImVec2(width, height));

        ImGui::PopStyleColor(4);

        if (pressed)
            value = !value;

        return pressed;
    }

    bool TimeInput(const char* id, std::string& text, const char* hint, const char* nowLabel, const std::string& nowText)
    {
        const ImGuiStyle& style = ImGui::GetStyle();
        const float nowWidth = ButtonWidth(nowLabel);

        ImGui::SetNextItemWidth(std::max(Em(7.0f),
            std::min(Em(10.0f), ImGui::GetContentRegionAvail().x - nowWidth - style.ItemSpacing.x)));

        bool changed = ImGui::InputTextWithHint(id, hint, &text);

        ImGui::SameLine();
        ImGui::PushID(id);

        if (Button(nowLabel, ButtonKind::Secondary, -1.0f))
        {
            text = nowText;
            changed = true;
        }

        ImGui::PopID();
        return changed;
    }

    bool IntInput(const char* id, int& value, int minimum, int maximum)
    {
        int edited = value;

        // Apply while typing. Since Dear ImGui 1.92.9 number
        // fields only apply on Enter / Tab / focus loss, so a
        // value typed just before clicking Save would be lost
        // together with that click, and live hints (e.g.
        // "frames 9-10 will be retired") would lag behind.
        ImGui::PushItemFlag(ImGuiItemFlags_LiveEditOnInputScalar, true);
        ImGui::InputInt(id, &edited, 0, 0);
        ImGui::PopItemFlag();

        edited = std::clamp(edited, minimum, maximum);

        if (edited != value)
        {
            value = edited;
            return true;
        }

        return false;
    }

    // ----------------------------------------------
    // Tables
    // ----------------------------------------------

    float TableRowHeight()
    {
        return std::max(Px(Size::ButtonHeight), ImGui::GetFrameHeight());
    }

    bool RowSelectable(const char* label, float rowHeight)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.0f, 0.5f));

        const bool pressed = ImGui::Selectable(label, false,
            ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap, ImVec2(0.0f, rowHeight));

        ImGui::PopStyleVar();
        return pressed;
    }

    float TableFitWidth()
    {
        const ImGuiTable* table = ImGui::GetCurrentTable();
        return table ? table->ColumnsAutoFitWidth : 0.0f;
    }

    void AlignCellText(float rowHeight)
    {
        const float offset = (rowHeight - ImGui::GetTextLineHeight()) * 0.5f;

        if (offset > 0.0f)
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + offset);
    }

    // ----------------------------------------------
    // Containers
    // ----------------------------------------------

    void BeginCard(const char* id)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, Px(Spacing::Panel, Spacing::Panel));

        ImGui::BeginChild(
            id,
            ImVec2(0.0f, 0.0f),
            ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY
        );
    }

    void EndCard()
    {
        ImGui::EndChild();
        ImGui::PopStyleVar();
    }

    void StepIndicator(const char* const* labels, int count, int current)
    {
        using namespace Theme::Colors;

        const float dot = Em(1.6f);
        ImDrawList* drawList = ImGui::GetWindowDrawList();

        for (int i = 0; i < count; ++i)
        {
            char number[16];
            std::snprintf(number, sizeof(number), "%d", i + 1);

            const std::string label = labels[i];
            const float labelWidth = ImGui::CalcTextSize(label.c_str()).x;
            const float itemWidth = dot + ImGui::GetStyle().ItemInnerSpacing.x + labelWidth;

            if (i > 0)
                SameLineOrWrap(itemWidth + Em(1.5f));

            if (i > 0)
            {
                // Connector line.
                const ImVec2 p = ImGui::GetCursorScreenPos();
                drawList->AddLine(
                    ImVec2(p.x, p.y + dot * 0.5f),
                    ImVec2(p.x + Em(1.0f), p.y + dot * 0.5f),
                    ImGui::GetColorU32(Theme::Colors::Border),
                    Px(2.0f)
                );
                ImGui::Dummy(ImVec2(Em(1.0f), dot));
                ImGui::SameLine();
            }

            const bool done = i < current;
            const bool active = i == current;

            const ImVec4 fill = active ? Honey : (done ? Sage : SurfaceRaised);
            const ImVec4 text = active ? Forest : (done ? Forest : TextSecondary);

            const ImVec2 p = ImGui::GetCursorScreenPos();
            const ImVec2 center(p.x + dot * 0.5f, p.y + dot * 0.5f);

            drawList->AddCircleFilled(center, dot * 0.5f, ImGui::GetColorU32(fill));

            const ImVec2 numberSize = ImGui::CalcTextSize(number);
            drawList->AddText(
                ImVec2(center.x - numberSize.x * 0.5f, center.y - numberSize.y * 0.5f),
                ImGui::GetColorU32(text),
                number
            );

            ImGui::Dummy(ImVec2(dot, dot));
            ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);

            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (dot - ImGui::GetTextLineHeight()) * 0.5f);
            ImGui::TextColored(active ? TextPrimary : TextSecondary, "%s", label.c_str());
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

            ImGui::PushStyleColor(ImGuiCol_Button, WithAlpha(accent, 0.25f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, WithAlpha(accent, 0.4f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, WithAlpha(accent, 0.55f));

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
