
#include "RegionalSettings.h"

#include "localization/Localization.h"

namespace RegionalSettings
{
    bool Apply(const AppSettings& settings)
    {
        const std::string system = Localization::DetectSystemLocale();

        std::string locale = system;

        if (settings.locale != "auto" && !settings.locale.empty())
        {
            // Keep the Windows region when it matches the
            // chosen language (English + Windows en-GB ->
            // en-GB), otherwise use the language alone.
            const bool sameLanguage =
                system.rfind(settings.locale + "-", 0) == 0 || system == settings.locale;

            locale = sameLanguage ? system : settings.locale;
        }

        const bool ok = Localization::Initialize(locale);

        Localization::SetDatePatternOverride(settings.datePattern);
        return ok;
    }
}
