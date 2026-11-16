
#include "InspectionOptions.h"

#include "localization/Localization.h"

namespace InspectionOptions
{
    const std::vector<Option>& Weather()
    {
        static const std::vector<Option> options =
        {
            { "sunny",         "inspection.weather.sunny"         },
            { "partly_cloudy", "inspection.weather.partly_cloudy" },
            { "overcast",      "inspection.weather.overcast"      },
            { "light_rain",    "inspection.weather.light_rain"    },
            { "windy",         "inspection.weather.windy"         },
        };
        return options;
    }

    const std::vector<Option>& Temperament()
    {
        static const std::vector<Option> options =
        {
            { "calm",      "inspection.temperament.calm"      },
            { "nervous",   "inspection.temperament.nervous"   },
            { "defensive", "inspection.temperament.defensive" },
        };
        return options;
    }

    const std::vector<Option>& QueenStatus()
    {
        static const std::vector<Option> options =
        {
            { "seen",      "inspection.queen.seen"      },
            { "eggs_only", "inspection.queen.eggs_only" },
            { "not_seen",  "inspection.queen.not_seen"  },
            { "queenless", "inspection.queen.queenless" },
        };
        return options;
    }

    const std::vector<Option>& Strength()
    {
        static const std::vector<Option> options =
        {
            { "weak",     "inspection.strength.weak"     },
            { "moderate", "inspection.strength.moderate" },
            { "strong",   "inspection.strength.strong"   },
        };
        return options;
    }

    const std::vector<Option>& Stores()
    {
        static const std::vector<Option> options =
        {
            { "low",       "inspection.stores.low"       },
            { "adequate",  "inspection.stores.adequate"  },
            { "plentiful", "inspection.stores.plentiful" },
        };
        return options;
    }

    const std::vector<Option>& Findings()
    {
        static const std::vector<Option> options =
        {
            { "chalkbrood",          "inspection.finding.chalkbrood"          },
            { "sacbrood",            "inspection.finding.sacbrood"            },
            { "suspected_foulbrood", "inspection.finding.suspected_foulbrood" },
            { "small_hive_beetle",   "inspection.finding.small_hive_beetle"   },
            { "wax_moth",            "inspection.finding.wax_moth"            },
            { "deformed_wings",      "inspection.finding.deformed_wings"      },
        };
        return options;
    }

    const std::vector<Option>& CombDrawn()
    {
        static const std::vector<Option> options =
        {
            { "none",    "inspection.comb.none"    },
            { "partial", "inspection.comb.partial" },
            { "full",    "inspection.comb.full"    },
        };
        return options;
    }

    std::string Label(const std::vector<Option>& options, const std::string& key)
    {
        for (const Option& option : options)
        {
            if (key == option.key)
                return Tr(option.labelKey);
        }

        return "";
    }

    int IndexOf(const std::vector<Option>& options, const std::string& key)
    {
        for (size_t i = 0; i < options.size(); ++i)
        {
            if (key == options[i].key)
                return static_cast<int>(i);
        }

        return -1;
    }
}
