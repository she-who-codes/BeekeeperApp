
#include "DateFormat.h"

#include <cctype>
#include <cstdio>
#include <ctime>

namespace
{
    bool IsLeapYear(int year)
    {
        return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
    }

    int DaysInMonth(int year, int month)
    {
        static const int days[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

        if (month == 2 && IsLeapYear(year))
            return 29;

        return days[month - 1];
    }

    bool StartsWith(const std::string& text, size_t position, const char* token)
    {
        return text.compare(position, 4, token, 4) == 0;
    }

    std::string Trim(const std::string& text)
    {
        const size_t first = text.find_first_not_of(" \t\r\n");

        if (first == std::string::npos)
            return "";

        const size_t last = text.find_last_not_of(" \t\r\n");
        return text.substr(first, last - first + 1);
    }

    // Reads between minDigits and maxDigits digits.
    bool ReadNumber(
        const std::string& text,
        size_t& position,
        int minDigits,
        int maxDigits,
        int& value)
    {
        int digits = 0;
        value = 0;

        while (position < text.size()
            && digits < maxDigits
            && std::isdigit(static_cast<unsigned char>(text[position])))
        {
            value = value * 10 + (text[position] - '0');
            ++position;
            ++digits;
        }

        return digits >= minDigits;
    }
}

namespace DateFormat
{
    bool IsValid(const Date& date)
    {
        return date.year >= MinimumYear
            && date.year <= MaximumYear
            && date.month >= 1 && date.month <= 12
            && date.day >= 1
            && date.day <= DaysInMonth(date.year, date.month);
    }

    int Compare(const Date& a, const Date& b)
    {
        if (a.year != b.year)   return a.year < b.year ? -1 : 1;
        if (a.month != b.month) return a.month < b.month ? -1 : 1;
        if (a.day != b.day)     return a.day < b.day ? -1 : 1;
        return 0;
    }

    bool ParseIso(const std::string& text, Date& out)
    {
        return Parse(text, "YYYY-MM-DD", out) == ParseResult::Ok
            && text.size() == 10;
    }

    std::string ToIso(const Date& date)
    {
        return Format(date, "YYYY-MM-DD");
    }

    ParseResult Parse(const std::string& rawText, const std::string& pattern, Date& out)
    {
        const std::string text = Trim(rawText);

        if (text.empty())
            return ParseResult::Empty;

        Date date;
        size_t t = 0;
        size_t p = 0;

        while (p < pattern.size())
        {
            if (StartsWith(pattern, p, "YYYY"))
            {
                if (!ReadNumber(text, t, 4, 4, date.year))
                    return ParseResult::BadFormat;
                p += 4;
            }
            else if (pattern.compare(p, 2, "MM") == 0)
            {
                if (!ReadNumber(text, t, 1, 2, date.month))
                    return ParseResult::BadFormat;
                p += 2;
            }
            else if (pattern.compare(p, 2, "DD") == 0)
            {
                if (!ReadNumber(text, t, 1, 2, date.day))
                    return ParseResult::BadFormat;
                p += 2;
            }
            else
            {
                if (t >= text.size() || text[t] != pattern[p])
                    return ParseResult::BadFormat;
                ++t;
                ++p;
            }
        }

        if (t != text.size())
            return ParseResult::BadFormat;

        if (!IsValid(date))
            return ParseResult::InvalidDate;

        out = date;
        return ParseResult::Ok;
    }

    std::string Format(const Date& date, const std::string& pattern)
    {
        std::string result;
        char buffer[8];

        for (size_t p = 0; p < pattern.size();)
        {
            if (StartsWith(pattern, p, "YYYY"))
            {
                std::snprintf(buffer, sizeof(buffer), "%04d", date.year);
                result += buffer;
                p += 4;
            }
            else if (pattern.compare(p, 2, "MM") == 0)
            {
                std::snprintf(buffer, sizeof(buffer), "%02d", date.month);
                result += buffer;
                p += 2;
            }
            else if (pattern.compare(p, 2, "DD") == 0)
            {
                std::snprintf(buffer, sizeof(buffer), "%02d", date.day);
                result += buffer;
                p += 2;
            }
            else
            {
                result += pattern[p];
                ++p;
            }
        }

        return result;
    }

    std::string IsoToDisplay(const std::string& iso, const std::string& pattern)
    {
        Date date;

        return ParseIso(iso, date)
            ? Format(date, pattern)
            : "";
    }

    Date Today()
    {
        const std::time_t now = std::time(nullptr);
        std::tm local {};

#if defined(_MSC_VER)
        localtime_s(&local, &now);
#elif defined(_WIN32)
        // MinGW: localtime_r/localtime_s availability varies.
        // The UI is single-threaded, so std::localtime is safe.
        local = *std::localtime(&now);
#else
        localtime_r(&now, &local);
#endif

        Date date;
        date.year = local.tm_year + 1900;
        date.month = local.tm_mon + 1;
        date.day = local.tm_mday;
        return date;
    }

    bool IsAllowedInput(unsigned int character, const std::string& pattern)
    {
        if (character >= '0' && character <= '9')
            return true;

        for (char c : pattern)
        {
            if (c != 'Y' && c != 'M' && c != 'D'
                && static_cast<unsigned char>(c) == character)
                return true;
        }

        return false;
    }
}
