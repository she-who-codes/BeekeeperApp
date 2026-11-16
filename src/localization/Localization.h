
#pragma once

#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

// ==================================================
// Localization
// --------------------------------------------------
// Strings live in resources/locales/<tag>.json as
// nested objects and are addressed by dotted keys:
//
//   Tr("hive.form.title_add")
//
// Locale resolution (later files override earlier):
//   en.json  ->  <language>.json  ->  <language-REGION>.json
// e.g. "en-GB" loads en.json, then en-GB.json. Region
// files only need the keys that differ (date format...).
//
// The files in resources/locales are also compiled into
// the executable (cmake/EmbedLocales.cmake). Built-in
// languages always come from the executable, so a
// missing or out-of-date copy of "resources" can never
// show raw keys. Files on disk can add languages that are
// not built in.
//
// A missing key renders as the key itself so gaps are
// visible during development instead of crashing.
// ==================================================

namespace Localization
{
    constexpr const char* DefaultLocale = "en";

    // Folder for additional (not built-in) locale files:
    // <resources>/locales (see config/Paths.h).
    std::string DefaultDirectory();

    // Loads the locale chain for the given tag
    // ("en-US", "en_GB.UTF-8", "fr"...). 'directory' empty
    // = DefaultDirectory(). Returns false only if the
    // default locale could not be loaded.
    bool Initialize(
        const std::string& localeTag,
        const std::string& directory = ""
    );

    // Normalised tag of the active locale, e.g. "en-GB".
    const std::string& CurrentLocale();

    const std::string& GetLastError();

    // Best-effort OS locale ("en-US"). Falls back to
    // DefaultLocale when it cannot be determined.
    std::string DetectSystemLocale();

    // Translated text for a key. The returned pointer
    // stays valid until the next Initialize() call.
    // A missing key returns the key itself, is recorded
    // in MissingKeys() and logged once to stderr.
    const char* Tr(const char* key);

    // Keys requested but not found since Initialize().
    const std::vector<std::string>& MissingKeys();

    // Translated text with named placeholders:
    //   "hive.count": "Hives ({count})"
    //   Format("hive.count", {{"count", "3"}})
    // Named (not positional) so translators can reorder.
    using Args = std::initializer_list<std::pair<const char*, std::string>>;

    std::string Format(const char* key, Args args);

    // Count-dependent text: uses "<key>.one" when count is
    // 1, otherwise "<key>.other" (English plural rules;
    // languages with more plural forms can extend this).
    // "{count}" is always available as a placeholder.
    std::string FormatPlural(const char* key, long count, Args args = {});

    // ImGui label with a stable ID that does not change
    // when the visible text is translated:
    //   Label("common.save", "Save") -> "Speichern###Save"
    std::string Label(const char* key, const char* stableId);

    // Same, for already-formatted visible text.
    std::string WithId(const std::string& text, const char* stableId);

    // ---------------- Dates ----------------

    // Keeper's date format from Settings. Empty or "auto"
    // = use the locale's "format.date_pattern".
    void SetDatePatternOverride(const std::string& pattern);

    // Active date pattern, e.g. "MM/DD/YYYY".
    std::string DatePattern();

    // Placeholder for a pattern using the locale's token
    // letters ("format.token_year" ...), e.g. a French
    // locale can show "JJ/MM/AAAA" for "DD/MM/YYYY".
    std::string DateHintFor(const std::string& pattern);

    // ---------------- Languages ----------------

    struct LanguageInfo
    {
        std::string tag;    // "en"
        std::string name;   // "English" (meta.language_name)
    };

    // Languages that have a base file in 'directory'
    // (regional files such as en-GB.json are overlays
    // and are not listed).
    // Built-in languages plus base files found in
    // 'directory' (empty = DefaultDirectory()).
    std::vector<LanguageInfo> AvailableLanguages(
        const std::string& directory = ""
    );
}

// Short alias used throughout the UI code.
using Localization::Tr;
