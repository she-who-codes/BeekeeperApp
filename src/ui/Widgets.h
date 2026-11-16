
#pragma once

#include "imgui.h"
#include "config/LayoutUI.h"

#include <optional>
#include <string>

// ==================================================
// Widgets
// --------------------------------------------------
// Reusable controls built on the values in LayoutUI.h
// and the colours in Theme. Screens should use these
// instead of hard-coded sizes so the whole app stays
// DPI-aware and tolerant of longer translated text.
// ==================================================

namespace UI
{
    // ----------------------------------------------
    // Buttons (width always follows the label text)
    // ----------------------------------------------

    enum class ButtonKind
    {
        Primary,    // main action on a view (honey)
        Secondary,  // default
        Danger      // destructive
    };

    // Note: there is intentionally no transparent button
    // style; every button keeps a visible background so it
    // can't blend into the view.

    // Width the button will use for 'label' (text after
    // "##"/"###" is ignored, as in ImGui).
    float ButtonWidth(const char* label);

    // height: 0 = UI::Size::ButtonHeight, -1 = frame
    // height (aligns with inputs on the same row).
    bool Button(
        const char* label,
        ButtonKind kind = ButtonKind::Secondary,
        float height = 0.0f
    );

    // ----------------------------------------------
    // Flow layout
    // ----------------------------------------------

    // Call between two items: keeps the next item on
    // the same line if 'nextWidth' fits, otherwise
    // wraps it to a new line (toolbars with translated
    // labels of unknown length).
    void SameLineOrWrap(float nextWidth);

    // ----------------------------------------------
    // Text
    // ----------------------------------------------

    void TextStyled(const TextStyle& style, const ImVec4& color, const char* text);
    void TextStyledWrapped(const TextStyle& style, const ImVec4& color, const char* text);

    // Truncates to maxCharacters UTF-8 code points and
    // appends an ellipsis. Never splits a character.
    std::string Ellipsize(const std::string& utf8, size_t maxCharacters);

    // Small rounded status label ("Archived"...).
    void StatusChip(const char* text, const ImVec4& color);
    float StatusChipWidth(const char* text);

    // ----------------------------------------------
    // Forms
    // ----------------------------------------------

    void FormLabel(const char* text, bool required = false);

    // Short explanatory text under a field label.
    void FieldHelp(const char* text);

    // Validation message under a field (no-op if empty).
    void FieldError(const std::string& message);

    // Validated, locale-formatted date entry.
    //  text      : user text in the locale pattern
    //  pattern   : e.g. "MM/DD/YYYY" (see DateFormat.h)
    //  hint      : localized placeholder, e.g. "MM/DD/YYYY"
    //  todayLabel: localized button label with ###id
    // Only digits and the pattern's separators can be
    // typed. Returns true when the text changed.
    bool DateInput(
        const char* id,
        std::string& text,
        const std::string& pattern,
        const char* hint,
        const char* todayLabel
    );

    // ----------------------------------------------
    // Panes with horizontal + vertical scrolling
    // ----------------------------------------------

    // A child window that scrolls vertically AND shows
    // a horizontal scrollbar when it becomes narrower
    // than 'minContentWidth' (instead of squeezing or
    // clipping its content). Content width is
    // max(minContentWidth, visible width): pass the width
    // wide content needs (e.g. a frame grid) as the
    // minimum. Always call EndScrollPane() whatever the
    // return value, like BeginChild().
    bool BeginScrollPane(
        const char* id,
        const ImVec2& size,
        float minContentWidth,
        ImGuiChildFlags childFlags = ImGuiChildFlags_Borders,
        ImGuiWindowFlags windowFlags = 0
    );

    void EndScrollPane();

    // ----------------------------------------------
    // Choice & number inputs
    // ----------------------------------------------

    // Row of large, equally sized toggle buttons (easy to
    // hit with gloves). 'height' 0 = UI::Size::TouchTarget.
    // Returns true when the selection changed.
    bool SegmentedControl(
        const char* id,
        const char* const* labels,
        int count,
        int& selected,
        float height = 0.0f
    );

    // Numeric text field whose value may be empty
    // (std::nullopt). Accepts digits and one '.'.
    // Returns true when the value changed.
    bool OptionalNumberInput(
        const char* id,
        std::optional<double>& value,
        const char* hint = ""
    );

    // Integer field without +/- buttons, clamped to
    // [minimum, maximum]. Returns true when changed.
    bool IntInput(const char* id, int& value, int minimum, int maximum);

    // ----------------------------------------------
    // Containers
    // ----------------------------------------------

    // Bordered, auto-height panel. Always call EndCard().
    void BeginCard(const char* id);
    void EndCard();

    // "1 Region - 2 Seasons - ..." progress header.
    void StepIndicator(const char* const* labels, int count, int current);

    // ----------------------------------------------
    // Feedback
    // ----------------------------------------------

    enum class BannerKind
    {
        Error,
        Success
    };

    // Full-width message with a dismiss button.
    // Returns true when the user dismissed it.
    bool Banner(
        const char* id,
        const std::string& text,
        BannerKind kind,
        const char* dismissTooltip
    );

    // ----------------------------------------------
    // Dialogs
    // ----------------------------------------------

    // Call right before BeginPopupModal(): centres the
    // dialog, sets a width of 'widthEm' (clamped to the
    // screen) and auto-fits the height up to the screen
    // limit, after which the dialog scrolls.
    void SetNextDialogLayout(float widthEm);

    // Scrollable dialog body with a pinned footer, so the
    // Save / Cancel buttons stay visible on small screens
    // or at high DPI:
    //
    //   UI::BeginDialogBody("Body");
    //   ... fields ...
    //   UI::EndDialogBody();
    //   ... footer (errors, buttons) ...
    //   UI::EndDialogFooter();
    //
    // The body grows with its content up to what fits on
    // screen, then scrolls vertically.
    void BeginDialogBody(const char* id);
    void EndDialogBody();
    void EndDialogFooter();

    // Colour helpers
    ImVec4 Mix(const ImVec4& a, const ImVec4& b, float t);
    ImVec4 WithAlpha(const ImVec4& color, float alpha);
}
