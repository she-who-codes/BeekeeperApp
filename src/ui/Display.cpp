
#include "Display.h"

#include "localization/DateFormat.h"
#include "localization/Localization.h"
#include "models/Box.h"
#include "models/HiveEquipment.h"
#include "models/HiveType.h"

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
}
