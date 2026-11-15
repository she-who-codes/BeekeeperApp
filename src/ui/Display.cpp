
#include "Display.h"

#include "localization/DateFormat.h"
#include "localization/Localization.h"
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

    std::string Date(const std::string& iso)
    {
        return DateFormat::IsoToDisplay(iso, DatePattern());
    }

    std::string DatePattern()
    {
        return Tr("format.date_pattern");
    }

    const char* DateHint()
    {
        return Tr("format.date_hint");
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
