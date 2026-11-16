
#include "Localization.h"

#include "config/Paths.h"
#include "localization/EmbeddedLocales.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#ifdef _WIN32
// GetUserDefaultLocaleName() requires Windows Vista+
// (older MinGW toolchains default to an XP target).
#if !defined(_WIN32_WINNT) || _WIN32_WINNT < 0x0600
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace
{
    std::unordered_map<std::string, std::string> g_strings;
    std::string g_currentLocale = Localization::DefaultLocale;
    std::string g_lastError;
    std::string g_datePatternOverride;

    std::vector<std::string> g_missingKeys;
    std::unordered_set<std::string> g_missingSet;

    void ReportMissing(const char* key)
    {
        if (g_missingSet.insert(key).second)
        {
            g_missingKeys.push_back(key);
            std::cerr << "Missing translation: " << key
                      << " (locale " << g_currentLocale << ")" << std::endl;
        }
    }

    // "en_GB.UTF-8" -> "en-GB", "EN-us" -> "en-US"
    std::string NormalizeTag(const std::string& raw)
    {
        std::string tag = raw.substr(0, raw.find_first_of(".@"));

        for (char& c : tag)
        {
            if (c == '_')
                c = '-';
        }

        const size_t dash = tag.find('-');

        for (size_t i = 0; i < tag.size(); ++i)
        {
            const unsigned char c = static_cast<unsigned char>(tag[i]);

            tag[i] = (dash != std::string::npos && i > dash)
                ? static_cast<char>(std::toupper(c))
                : static_cast<char>(std::tolower(c));
        }

        if (tag.empty() || tag == "c" || tag == "posix")
            return Localization::DefaultLocale;

        return tag;
    }

    void Flatten(
        const nlohmann::json& node,
        const std::string& prefix,
        std::unordered_map<std::string, std::string>& out)
    {
        if (node.is_object())
        {
            for (auto it = node.begin(); it != node.end(); ++it)
            {
                Flatten(
                    it.value(),
                    prefix.empty() ? it.key() : prefix + "." + it.key(),
                    out
                );
            }
        }
        else if (node.is_string())
        {
            out[prefix] = node.get<std::string>();
        }
    }

    // Overlays one parsed locale onto 'out'.
    bool LoadJson(
        const nlohmann::json& root,
        const std::string& source,
        std::unordered_map<std::string, std::string>& out,
        std::string& error)
    {
        if (root.is_discarded() || !root.is_object())
        {
            error = "Locale is not valid JSON: " + source;
            return false;
        }

        Flatten(root, "", out);
        return true;
    }

    const EmbeddedLocales::Entry* FindEmbedded(const std::string& tag)
    {
        int count = 0;
        const EmbeddedLocales::Entry* entries = EmbeddedLocales::All(count);

        for (int i = 0; i < count; ++i)
        {
            if (tag == entries[i].tag)
                return &entries[i];
        }

        return nullptr;
    }

    // Overlays one locale (built-in if available, otherwise
    // <directory>/<tag>.json) onto 'out'. Returns false if
    // neither exists or it is invalid.
    bool LoadLocale(
        const std::string& tag,
        const std::string& directory,
        std::unordered_map<std::string, std::string>& out,
        std::string& error)
    {
        if (const EmbeddedLocales::Entry* entry = FindEmbedded(tag))
        {
            const nlohmann::json root = nlohmann::json::parse(
                entry->data, entry->data + entry->size,
                nullptr, false, true);

            return LoadJson(root, "built-in " + tag, out, error);
        }

        const std::string path = directory + "/" + tag + ".json";
        std::ifstream file(path, std::ios::binary);

        if (!file)
        {
            error = "Locale not found: " + tag;
            return false;
        }

        const nlohmann::json root = nlohmann::json::parse(
            file,
            nullptr,
            false,  // no exceptions
            true    // ignore comments
        );

        return LoadJson(root, path, out, error);
    }
}

namespace Localization
{
    std::string DefaultDirectory()
    {
        return Paths::Resource("locales");
    }

    bool Initialize(const std::string& localeTag, const std::string& requestedDirectory)
    {
        const std::string directory = requestedDirectory.empty() ? DefaultDirectory() : requestedDirectory;
        const std::string tag = NormalizeTag(localeTag);

        // Chain: default -> language -> language-REGION
        std::vector<std::string> chain { DefaultLocale };

        const std::string language = tag.substr(0, tag.find('-'));

        if (language != DefaultLocale)
            chain.push_back(language);

        if (tag != language)
            chain.push_back(tag);

        std::unordered_map<std::string, std::string> strings;
        std::string error;

        if (!LoadLocale(DefaultLocale, directory, strings, error))
        {
            g_lastError = error;
            return false;
        }

        // Optional overlays: a missing regional file is
        // normal (e.g. no fr-CA.json -> use fr.json).
        for (size_t i = 1; i < chain.size(); ++i)
        {
            std::string ignored;
            LoadLocale(chain[i], directory, strings, ignored);
        }

        g_strings = std::move(strings);
        g_currentLocale = tag;
        g_lastError.clear();
        g_missingKeys.clear();
        g_missingSet.clear();
        return true;
    }

    const std::string& CurrentLocale()
    {
        return g_currentLocale;
    }

    const std::string& GetLastError()
    {
        return g_lastError;
    }

    std::string DetectSystemLocale()
    {
#ifdef _WIN32
        wchar_t buffer[LOCALE_NAME_MAX_LENGTH] = {};

        if (GetUserDefaultLocaleName(buffer, LOCALE_NAME_MAX_LENGTH) > 0)
        {
            std::string tag;

            for (const wchar_t* c = buffer; *c; ++c)
                tag.push_back(static_cast<char>(*c & 0x7F));

            return NormalizeTag(tag);
        }
#else
        for (const char* variable : { "LC_ALL", "LC_MESSAGES", "LANG" })
        {
            const char* value = std::getenv(variable);

            if (value && *value)
                return NormalizeTag(value);
        }
#endif
        return DefaultLocale;
    }

    const char* Tr(const char* key)
    {
        const auto it = g_strings.find(key);

        if (it != g_strings.end())
            return it->second.c_str();

        ReportMissing(key);
        return key;
    }

    const std::vector<std::string>& MissingKeys()
    {
        return g_missingKeys;
    }

    std::string Format(const char* key, Args args)
    {
        std::string text = Tr(key);

        for (const auto& [name, value] : args)
        {
            const std::string token = std::string("{") + name + "}";

            size_t position = 0;

            while ((position = text.find(token, position)) != std::string::npos)
            {
                text.replace(position, token.size(), value);
                position += value.size();
            }
        }

        return text;
    }

    std::string FormatPlural(const char* key, long count, Args args)
    {
        const std::string base = key;
        const std::string form = base + (count == 1 ? ".one" : ".other");

        std::string text = g_strings.count(form) ? g_strings[form] : Tr((base + ".other").c_str());

        const std::string countText = std::to_string(count);
        const std::string countToken = "{count}";

        for (size_t position = 0; (position = text.find(countToken, position)) != std::string::npos;)
        {
            text.replace(position, countToken.size(), countText);
            position += countText.size();
        }

        for (const auto& [name, value] : args)
        {
            const std::string token = std::string("{") + name + "}";

            for (size_t position = 0; (position = text.find(token, position)) != std::string::npos;)
            {
                text.replace(position, token.size(), value);
                position += value.size();
            }
        }

        return text;
    }

    std::string Label(const char* key, const char* stableId)
    {
        return WithId(Tr(key), stableId);
    }

    std::string WithId(const std::string& text, const char* stableId)
    {
        return text + "###" + stableId;
    }

    void SetDatePatternOverride(const std::string& pattern)
    {
        g_datePatternOverride = pattern == "auto" ? std::string() : pattern;
    }

    std::string DatePattern()
    {
        return g_datePatternOverride.empty()
            ? std::string(Tr("format.date_pattern"))
            : g_datePatternOverride;
    }

    std::string DateHintFor(const std::string& pattern)
    {
        std::string hint;

        for (size_t p = 0; p < pattern.size();)
        {
            if (pattern.compare(p, 4, "YYYY") == 0)
            {
                hint += Tr("format.token_year");
                p += 4;
            }
            else if (pattern.compare(p, 2, "MM") == 0)
            {
                hint += Tr("format.token_month");
                p += 2;
            }
            else if (pattern.compare(p, 2, "DD") == 0)
            {
                hint += Tr("format.token_day");
                p += 2;
            }
            else
            {
                hint += pattern[p];
                ++p;
            }
        }

        return hint;
    }

    std::vector<LanguageInfo> AvailableLanguages(const std::string& requestedDirectory)
    {
        const std::string directory = requestedDirectory.empty() ? DefaultDirectory() : requestedDirectory;

        // Base language tags: built-in first, then files on
        // disk that are not built in. Regional overlays
        // (en-GB) are not languages.
        std::vector<std::string> tags;

        int count = 0;
        const EmbeddedLocales::Entry* entries = EmbeddedLocales::All(count);

        for (int i = 0; i < count; ++i)
        {
            if (std::string(entries[i].tag).find('-') == std::string::npos)
                tags.push_back(entries[i].tag);
        }

        std::error_code error;

        for (const auto& entry : std::filesystem::directory_iterator(directory, error))
        {
            if (!entry.is_regular_file() || entry.path().extension() != ".json")
                continue;

            const std::string tag = entry.path().stem().string();

            if (tag.find('-') == std::string::npos
                && std::find(tags.begin(), tags.end(), tag) == tags.end())
                tags.push_back(tag);
        }

        std::vector<LanguageInfo> languages;

        for (const std::string& tag : tags)
        {
            std::unordered_map<std::string, std::string> strings;
            std::string ignored;

            LanguageInfo info;
            info.tag = tag;
            info.name = tag;

            if (LoadLocale(tag, directory, strings, ignored))
            {
                const auto it = strings.find("meta.language_name");

                if (it != strings.end())
                    info.name = it->second;
            }

            languages.push_back(info);
        }

        return languages;
    }
}
