
#include "Display.h"

#include "localization/DateFormat.h"
#include "localization/TimeFormat.h"
#include "localization/Localization.h"
#include "models/Box.h"
#include "models/HiveEquipment.h"
#include "models/HiveType.h"

#include <cmath>
#include <cstdio>

namespace Display
{
    std::string HiveType(const Hive& hive)
    {
        const int index = HiveTypes::IndexOf(hive.hiveType);

        if (index < 0)
        {
            // Unknown legacy value: show it verbatim.
            return hive.hiveType.empty()
                ? Tr("hive.type_not_set")
                : hive.hiveType;
        }

        if (HiveTypes::IsOther(hive.hiveType) && !hive.hiveTypeOther.empty())
            return hive.hiveTypeOther;

        return Tr(HiveTypes::All[index].labelKey);
    }

    std::string BoxType(const std::string& key)
    {
        const auto* info = HiveEquipment::FindBoxType(key);
        return info ? Tr(info->labelKey) : key;
    }

    std::string BoxRole(const std::string& key)
    {
        const auto* info = HiveEquipment::FindRole(key);
        return info ? Tr(info->labelKey) : std::string();
    }

    std::string FrameType(const std::string& key)
    {
        const auto* info = HiveEquipment::FindFrameType(key);
        return info ? Tr(info->labelKey) : (key.empty() ? std::string(Tr("frame_type.unknown")) : key);
    }

    std::string FrameTypeShort(const std::string& key)
    {
        const auto* info = HiveEquipment::FindFrameType(key);
        return info ? Tr(info->shortKey) : std::string("?");
    }

    std::string BoxTitle(const Box& box)
    {
        std::string title = Localization::Format(
            "boxes.box_number",
            { { "number", std::to_string(box.position) } }
        );

        title += " \xC2\xB7 " + BoxType(box.boxType);

        if (!box.role.empty())
            title += " \xC2\xB7 " + BoxRole(box.role);

        if (!box.label.empty())
            title += " \xC2\xB7 " + box.label;

        return title;
    }

    std::string CombAge(const std::string& installedIso)
    {
        DateFormat::Date installed;

        if (!DateFormat::ParseIso(installedIso, installed))
            return "";

        const int months = DateFormat::MonthsBetween(installed, DateFormat::Today());
        const int years = months / 12;
        const int rest = months % 12;

        if (months == 0)
            return Tr("frames.age_under_month");

        if (years == 0)
            return Localization::FormatPlural("frames.age_months", rest);

        if (rest == 0)
            return Localization::FormatPlural("frames.age_years", years);

        return Localization::Format(
            "frames.age_years_months",
            {
                { "years", Localization::FormatPlural("frames.age_years", years) },
                { "months", Localization::FormatPlural("frames.age_months", rest) }
            }
        );
    }

    std::string CombAgeShort(const std::string& installedIso)
    {
        DateFormat::Date installed;

        if (!DateFormat::ParseIso(installedIso, installed))
            return "";

        const int months = DateFormat::MonthsBetween(installed, DateFormat::Today());

        // Nothing for frames younger than a month (avoids
        // "0 mo" on every new frame).
        if (months == 0)
            return "";

        return months >= 12
            ? Localization::FormatPlural("frames.age_years_short", months / 12)
            : Localization::FormatPlural("frames.age_months_short", months);
    }

    std::string Date(const std::string& iso)
    {
        return DateFormat::IsoToDisplay(iso, DatePattern());
    }

    std::string DatePattern()
    {
        return Localization::DatePattern();
    }

    std::string DateHint()
    {
        return Localization::DateHintFor(DatePattern());
    }

    bool ParseDateInput(
        const std::string& text,
        bool required,
        FutureDates future,
        std::string& iso,
        std::string& error)
    {
        iso.clear();
        error.clear();

        DateFormat::Date date;

        switch (DateFormat::Parse(text, DatePattern(), date))
        {
            case DateFormat::ParseResult::Ok:
                break;

            case DateFormat::ParseResult::Empty:
                if (required)
                    error = Tr("validation.date_required");
                return !required;

            case DateFormat::ParseResult::BadFormat:
                error = Localization::Format(
                    "validation.date_format",
                    { { "pattern", DateHint() } }
                );
                return false;

            case DateFormat::ParseResult::InvalidDate:
                error = Tr("validation.date_invalid");
                return false;
        }

        if (future == FutureDates::Rejected
            && DateFormat::Compare(date, DateFormat::Today()) > 0)
        {
            error = Tr("validation.date_future");
            return false;
        }

        iso = DateFormat::ToIso(date);
        return true;
    }

    // ---------------- Time ----------------

    namespace
    {
        std::string TimePattern() { return Tr("format.time_pattern"); }
        std::string Am() { return Tr("format.am"); }
        std::string Pm() { return Tr("format.pm"); }

        TemperatureUnit g_temperatureUnit = TemperatureUnit::Fahrenheit;
    }

    std::string Time(const std::string& storage)
    {
        TimeFormat::Time time;
        return TimeFormat::FromStorage(storage, time) ? TimeFormat::Format(time, TimePattern(), Am(), Pm()) : "";
    }

    std::string TimeNow()
    {
        return TimeFormat::Format(TimeFormat::Now(), TimePattern(), Am(), Pm());
    }

    std::string TimeHint()
    {
        return Localization::Format(
            "format.time_hint",
            { { "example", TimeFormat::Format(TimeFormat::Time { 14, 30 }, TimePattern(), Am(), Pm()) } });
    }

    bool ParseTimeInput(const std::string& text, std::string& storage, std::string& error)
    {
        error.clear();
        TimeFormat::Time time;

        switch (TimeFormat::Parse(text, Am(), Pm(), time))
        {
            case TimeFormat::ParseResult::Ok:
                storage = TimeFormat::ToStorage(time);
                return true;

            case TimeFormat::ParseResult::Empty:
                storage.clear();
                return true;

            case TimeFormat::ParseResult::Invalid:
                break;
        }

        error = Localization::Format(
            "validation.time_invalid",
            { { "example", TimeFormat::Format(TimeFormat::Time { 14, 30 }, TimePattern(), Am(), Pm()) } });
        return false;
    }

    // ---------------- Temperature ----------------

    void SetTemperatureUnit(TemperatureUnit unit)
    {
        g_temperatureUnit = unit;
    }

    TemperatureUnit CurrentTemperatureUnit()
    {
        return g_temperatureUnit;
    }

    const char* TemperatureSymbol()
    {
        return g_temperatureUnit == TemperatureUnit::Celsius ? "\xC2\xB0" "C" : "\xC2\xB0" "F";
    }

    std::optional<double> CelsiusToDisplay(const std::optional<double>& celsius)
    {
        if (!celsius)
            return std::nullopt;

        const double value = g_temperatureUnit == TemperatureUnit::Celsius ? *celsius : *celsius * 9.0 / 5.0 + 32.0;
        return std::round(value * 10.0) / 10.0;
    }

    std::optional<double> DisplayToCelsius(const std::optional<double>& value)
    {
        if (!value)
            return std::nullopt;

        return g_temperatureUnit == TemperatureUnit::Celsius ? *value : (*value - 32.0) * 5.0 / 9.0;
    }

    std::string Temperature(const std::optional<double>& celsius)
    {
        const auto value = CelsiusToDisplay(celsius);

        if (!value)
            return "";

        char buffer[32];
        std::snprintf(buffer, sizeof(buffer), "%g %s", *value, TemperatureSymbol());
        return buffer;
    }
}
