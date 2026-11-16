
#include "TimeFormat.h"

#include <cctype>
#include <cstdio>
#include <ctime>

namespace
{
    std::string Lower(std::string text)
    {
        for (char& c : text)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return text;
    }

    std::string Trim(const std::string& text)
    {
        const size_t first = text.find_first_not_of(" \t");

        if (first == std::string::npos)
            return "";

        return text.substr(first, text.find_last_not_of(" \t") - first + 1);
    }

    bool EndsWith(const std::string& text, const std::string& suffix)
    {
        return !suffix.empty() && text.size() >= suffix.size()
            && text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
    }
}

namespace TimeFormat
{
    ParseResult Parse(const std::string& rawText, const std::string& am, const std::string& pm, Time& out)
    {
        std::string text = Lower(Trim(rawText));

        if (text.empty())
            return ParseResult::Empty;

        // Optional AM / PM marker (locale or English).
        int meridiem = 0;   // 0 none, 1 am, 2 pm

        for (const auto& [marker, value] : { std::make_pair(Lower(am), 1), std::make_pair(Lower(pm), 2),
                                             std::make_pair(std::string("am"), 1), std::make_pair(std::string("pm"), 2) })
        {
            if (meridiem == 0 && EndsWith(text, marker))
            {
                meridiem = value;
                text = Trim(text.substr(0, text.size() - marker.size()));
            }
        }

        int hour = -1;
        int minute = -1;
        char extra = 0;

        if (std::sscanf(text.c_str(), "%d:%d%c", &hour, &minute, &extra) != 2)
            return ParseResult::Invalid;

        if (minute < 0 || minute > 59 || text.find(':') + 3 != text.size())
            return ParseResult::Invalid;

        if (meridiem != 0)
        {
            if (hour < 1 || hour > 12)
                return ParseResult::Invalid;

            hour = (hour % 12) + (meridiem == 2 ? 12 : 0);
        }
        else if (hour < 0 || hour > 23)
        {
            return ParseResult::Invalid;
        }

        out.hour = hour;
        out.minute = minute;
        return ParseResult::Ok;
    }

    std::string Format(const Time& time, const std::string& pattern, const std::string& am, const std::string& pm)
    {
        char buffer[32];

        if (pattern.find('a') != std::string::npos)
        {
            const int hour12 = time.hour % 12 == 0 ? 12 : time.hour % 12;
            std::snprintf(buffer, sizeof(buffer), "%d:%02d %s", hour12, time.minute,
                (time.hour < 12 ? am : pm).c_str());
        }
        else
        {
            std::snprintf(buffer, sizeof(buffer), "%02d:%02d", time.hour, time.minute);
        }

        return buffer;
    }

    std::string ToStorage(const Time& time)
    {
        char buffer[8];
        std::snprintf(buffer, sizeof(buffer), "%02d:%02d", time.hour, time.minute);
        return buffer;
    }

    bool FromStorage(const std::string& text, Time& out)
    {
        return text.size() == 5 && Parse(text, "", "", out) == ParseResult::Ok;
    }

    Time Now()
    {
        const std::time_t now = std::time(nullptr);
        std::tm local {};

#if defined(_MSC_VER)
        localtime_s(&local, &now);
#elif defined(_WIN32)
        local = *std::localtime(&now);
#else
        localtime_r(&now, &local);
#endif

        return Time { local.tm_hour, local.tm_min };
    }
}
