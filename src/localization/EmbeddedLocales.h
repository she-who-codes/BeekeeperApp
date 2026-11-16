
#pragma once

#include <cstddef>

// ==================================================
// EmbeddedLocales
// --------------------------------------------------
// The resources/locales/*.json files compiled into the
// executable at build time (see cmake/EmbedLocales.cmake).
// These are authoritative for the built-in languages;
// files on disk can only add languages that are not
// built in.
// ==================================================

namespace EmbeddedLocales
{
    struct Entry
    {
        const char* tag;            // "en", "en-GB"...
        const unsigned char* data;  // UTF-8 JSON
        std::size_t size;
    };

    const Entry* All(int& count);
}
