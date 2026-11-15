
#include "Localization.h"

#include <nlohmann/json.hpp>

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <unordered_map>
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

    // Overlays one locale file onto 'out'.
    // Returns false if the file is missing or invalid.
    bool LoadFile(
        const std::string& path,
        std::unordered_map<std::string, std::string>& out,
        std::string& error)
    {
        std::ifstream file(path, std::ios::binary);

        if (!file)
        {
            error = "Locale file not found: " + path;
            return false;
        }

        const nlohmann::json root = nlohmann::json::parse(
            file,
            nullptr,
            false,  // no exceptions
            true    // ignore comments
        );

        if (root.is_discarded() || !root.is_object())
        {
            error = "Locale file is not valid JSON: " + path;
            return false;
        }

        Flatten(root, "", out);
        return true;
    }
}

namespace Localization
{
    bool Initialize(const std::string& localeTag, const std::string& directory)
    {
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

        if (!LoadFile(directory + "/" + DefaultLocale + ".json", strings, error))
        {
            g_lastError = error;
            return false;
        }

        // Optional overlays: a missing regional file is
        // normal (e.g. no fr-CA.json -> use fr.json).
        for (size_t i = 1; i < chain.size(); ++i)
        {
            std::string ignored;
            LoadFile(directory + "/" + chain[i] + ".json", strings, ignored);
        }

        g_strings = std::move(strings);
        g_currentLocale = tag;
        g_lastError.clear();
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

        return it != g_strings.end()
            ? it->second.c_str()
            : key;
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

    std::string Label(const char* key, const char* stableId)
    {
        return WithId(Tr(key), stableId);
    }

    std::string WithId(const std::string& text, const char* stableId)
    {
        return text + "###" + stableId;
    }
}
