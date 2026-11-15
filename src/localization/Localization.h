
#pragma once

#include <initializer_list>
#include <string>
#include <utility>

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
// A missing key renders as the key itself so gaps are
// visible during development instead of crashing.
// ==================================================

namespace Localization
{
    constexpr const char* DefaultLocale = "en";
    constexpr const char* DefaultDirectory = "resources/locales";

    // Loads the locale chain for the given tag
    // ("en-US", "en_GB.UTF-8", "fr"...). Returns false if
    // the default locale file could not be loaded.
    bool Initialize(
        const std::string& localeTag,
        const std::string& directory = DefaultDirectory
    );

    // Normalised tag of the active locale, e.g. "en-GB".
    const std::string& CurrentLocale();

    const std::string& GetLastError();

    // Best-effort OS locale ("en-US"). Falls back to
    // DefaultLocale when it cannot be determined.
    std::string DetectSystemLocale();

    // Translated text for a key. The returned pointer
    // stays valid until the next Initialize() call.
    const char* Tr(const char* key);

    // Translated text with named placeholders:
    //   "hive.count": "Hives ({count})"
    //   Format("hive.count", {{"count", "3"}})
    // Named (not positional) so translators can reorder.
    using Args = std::initializer_list<std::pair<const char*, std::string>>;

    std::string Format(const char* key, Args args);

    // ImGui label with a stable ID that does not change
    // when the visible text is translated:
    //   Label("common.save", "Save") -> "Speichern###Save"
    std::string Label(const char* key, const char* stableId);

    // Same, for already-formatted visible text.
    std::string WithId(const std::string& text, const char* stableId);
}

// Short alias used throughout the UI code.
using Localization::Tr;
