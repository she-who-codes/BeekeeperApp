
#include "SettingsEditors.h"

#include "config/LayoutUI.h"
#include "config/Theme.h"
#include "localization/DateFormat.h"
#include "localization/Localization.h"
#include "settings/Seasons.h"
#include "settings/SettingsDefaults.h"
#include "ui/Widgets.h"

#include "imgui.h"
#include "imgui_stdlib.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace
{
    using namespace SettingsEditors;

    // Date patterns offered in Settings (besides "auto").
    const char* const DatePatterns[] =
    {
        "YYYY-MM-DD",
        "MM/DD/YYYY",
        "DD/MM/YYYY",
        "DD.MM.YYYY",
    };

    constexpr int DatePatternCount = static_cast<int>(sizeof(DatePatterns) / sizeof(DatePatterns[0]));

    std::string Trim(const std::string& text)
    {
        const size_t first = text.find_first_not_of(" \t\r\n");

        if (first == std::string::npos)
            return "";

        const size_t last = text.find_last_not_of(" \t\r\n");
        return text.substr(first, last - first + 1);
    }

    const char* UnitShort(MiteUnit unit)
    {
        switch (unit)
        {
            case MiteUnit::PercentBees:  return Tr("mite_unit.percent_bees");
            case MiteUnit::MitesPerDay:  return Tr("mite_unit.mites_per_day");
            case MiteUnit::PercentCells: return Tr("mite_unit.percent_cells");
        }

        return "";
    }

    const char* SampleLabel(MiteUnit unit)
    {
        switch (unit)
        {
            case MiteUnit::PercentBees:  return Tr("settings.mites.sample_bees");
            case MiteUnit::MitesPerDay:  return Tr("settings.mites.sample_days");
            case MiteUnit::PercentCells: return Tr("settings.mites.sample_cells");
        }

        return "";
    }

    const char* IntervalUnitName(IntervalUnit unit)
    {
        switch (unit)
        {
            case IntervalUnit::Days:   return Tr("interval_unit.days");
            case IntervalUnit::Weeks:  return Tr("interval_unit.weeks");
            case IntervalUnit::Months: return Tr("interval_unit.months");
            case IntervalUnit::Years:  return Tr("interval_unit.years");
        }

        return "";
    }

    const char* IntervalPluralKey(IntervalUnit unit)
    {
        switch (unit)
        {
            case IntervalUnit::Days:   return "interval.every_days";
            case IntervalUnit::Weeks:  return "interval.every_weeks";
            case IntervalUnit::Months: return "interval.every_months";
            case IntervalUnit::Years:  return "interval.every_years";
        }

        return "interval.every_months";
    }

    const char* AfterTreatmentPluralKey(IntervalUnit unit)
    {
        switch (unit)
        {
            case IntervalUnit::Days:   return "interval.after_treatment_days";
            case IntervalUnit::Weeks:  return "interval.after_treatment_weeks";
            case IntervalUnit::Months: return "interval.after_treatment_months";
            case IntervalUnit::Years:  return "interval.after_treatment_years";
        }

        return "interval.after_treatment_weeks";
    }

    std::string IntervalText(const MaintenanceTaskType& task)
    {
        return Localization::FormatPlural(
            task.trigger == TaskTrigger::AfterTreatment
                ? AfterTreatmentPluralKey(task.intervalUnit)
                : IntervalPluralKey(task.intervalUnit),
            task.intervalValue
        );
    }

    std::string TodayIn(const std::string& pattern)
    {
        return DateFormat::Format(DateFormat::Today(), pattern);
    }

    std::string DatePatternLabel(const std::string& pattern)
    {
        return Localization::Format(
            "settings.region.date_option",
            {
                { "format", Localization::DateHintFor(pattern) },
                { "example", TodayIn(pattern) }
            }
        );
    }

    // Section sub-heading with optional help text.
    void SubHeading(const char* title, const char* help = nullptr)
    {
        ImGui::Spacing();
        UI::TextStyled(UI::Text::Title, Theme::Colors::TextPrimary, title);

        if (help)
            UI::FieldHelp(help);

        ImGui::Spacing();
    }

    // Month combo + day combo. Returns true when changed.
    bool MonthDayInput(const char* id, MonthDay& value)
    {
        bool changed = false;

        ImGui::PushID(id);

        char monthKey[16];
        std::snprintf(monthKey, sizeof(monthKey), "month.%d", value.month);

        ImGui::SetNextItemWidth(UI::Em(UI::Controls::ComboFieldEm));

        if (ImGui::BeginCombo("##Month", Tr(monthKey)))
        {
            for (int m = 1; m <= 12; ++m)
            {
                std::snprintf(monthKey, sizeof(monthKey), "month.%d", m);

                ImGui::PushID(m);

                if (ImGui::Selectable(Tr(monthKey), m == value.month))
                {
                    value.month = m;
                    value.day = std::min(value.day, Seasons::MaxDay(m));
                    changed = true;
                }

                ImGui::PopID();
            }

            ImGui::EndCombo();
        }

        ImGui::SameLine();

        char dayText[16];
        std::snprintf(dayText, sizeof(dayText), "%d", value.day);

        ImGui::SetNextItemWidth(UI::Em(4.5f));

        if (ImGui::BeginCombo("##Day", dayText))
        {
            for (int d = 1; d <= Seasons::MaxDay(value.month); ++d)
            {
                std::snprintf(dayText, sizeof(dayText), "%d", d);

                if (ImGui::Selectable(dayText, d == value.day))
                {
                    value.day = d;
                    changed = true;
                }
            }

            ImGui::EndCombo();
        }

        ImGui::PopID();
        return changed;
    }

    bool IsPercent(MiteUnit unit)
    {
        return unit == MiteUnit::PercentBees || unit == MiteUnit::PercentCells;
    }
}

namespace SettingsEditors
{
    // ----------------------------------------------
    // Names
    // ----------------------------------------------

    const char* SectionTitle(Section section)
    {
        switch (section)
        {
            case Section::Region:         return Tr("settings.section.region");
            case Section::Seasons:        return Tr("settings.section.seasons");
            case Section::MiteMonitoring: return Tr("settings.section.mites");
            case Section::Maintenance:    return Tr("settings.section.maintenance");
            case Section::Count:          break;
        }

        return "";
    }

    std::string MethodName(const MiteCheckMethod& method)
    {
        if (const auto* info = SettingsDefaults::FindMiteMethod(method.key))
            return Tr(info->labelKey);

        const std::string name = Trim(method.customName);
        return name.empty() ? std::string(Tr("settings.mites.unnamed")) : name;
    }

    std::string TaskName(const MaintenanceTaskType& task)
    {
        if (const auto* info = SettingsDefaults::FindTask(task.key))
            return Tr(info->labelKey);

        const std::string name = Trim(task.customName);
        return name.empty() ? std::string(Tr("settings.maintenance.unnamed")) : name;
    }

    const char* SeasonName(Season season)
    {
        switch (season)
        {
            case Season::Spring: return Tr("season.spring");
            case Season::Summer: return Tr("season.summer");
            case Season::Fall:   return Tr("season.fall");
            case Season::Winter: return Tr("season.winter");
        }

        return "";
    }

    std::string MonthDayText(const MonthDay& value)
    {
        char monthKey[16];
        std::snprintf(monthKey, sizeof(monthKey), "month.%d", value.month);

        return Localization::Format(
            "format.month_day",
            { { "month", Tr(monthKey) }, { "day", std::to_string(value.day) } }
        );
    }

    // ----------------------------------------------
    // Validation
    // ----------------------------------------------

    bool Validation::IsSectionValid(Section section, bool allowIncomplete) const
    {
        auto allEmpty = [](const std::vector<std::string>& messages)
        {
            return std::all_of(messages.begin(), messages.end(),
                [](const std::string& m) { return m.empty(); });
        };

        switch (section)
        {
            case Section::Region:         return true;
            case Section::Seasons:        return seasons.empty();
            case Section::MiteMonitoring:
                return allEmpty(methods) && (allowIncomplete || methodSelection.empty());
            case Section::Maintenance:    return offSeason.empty() && allEmpty(tasks);
            case Section::Count:          break;
        }

        return true;
    }

    bool Validation::IsValid(bool allowIncomplete) const
    {
        for (int s = 0; s < static_cast<int>(Section::Count); ++s)
        {
            if (!IsSectionValid(static_cast<Section>(s), allowIncomplete))
                return false;
        }

        return true;
    }

    Validation Validate(const SettingsData& data)
    {
        Validation v;

        if (!Seasons::AreStartsValid(data.app.seasonStarts))
            v.seasons = Tr("settings.seasons.invalid_order");

        if (data.app.offSeasonPauseEnabled && data.app.offSeasonStart == data.app.offSeasonEnd)
            v.offSeason = Tr("settings.maintenance.off_season_same");

        if (data.app.miteTrackingEnabled)
        {
            const bool anySelected = std::any_of(
                data.miteMethods.begin(), data.miteMethods.end(),
                [](const MiteCheckMethod& m) { return m.enabled && !m.archived; });

            if (!anySelected)
                v.methodSelection = Tr("settings.mites.select_one");
        }

        // Mite methods: always validated (even when disabled)
        // because invalid values cannot be stored.
        v.methods.resize(data.miteMethods.size());

        for (size_t i = 0; i < data.miteMethods.size(); ++i)
        {
            const MiteCheckMethod& method = data.miteMethods[i];

            if (method.archived)
                continue;

            if (!method.IsBuiltIn() && Trim(method.customName).empty())
            {
                v.methods[i] = Tr("settings.mites.name_required");
                continue;
            }

            for (int s = 0; s < SeasonCount && v.methods[i].empty(); ++s)
            {
                const MiteThreshold& level = method.thresholds[s];
                const char* season = SeasonName(static_cast<Season>(s));

                if (IsPercent(method.unit)
                    && ((level.watch && *level.watch > 100.0) || (level.act && *level.act > 100.0)))
                {
                    v.methods[i] = Localization::Format("settings.mites.percent_max", { { "season", season } });
                }
                else if (level.watch && level.act && *level.watch > *level.act)
                {
                    v.methods[i] = Localization::Format("settings.mites.watch_above_act", { { "season", season } });
                }
            }
        }

        v.tasks.resize(data.taskTypes.size());

        for (size_t i = 0; i < data.taskTypes.size(); ++i)
        {
            const MaintenanceTaskType& task = data.taskTypes[i];

            if (task.archived)
                continue;

            if (!task.IsBuiltIn() && Trim(task.customName).empty())
                v.tasks[i] = Tr("settings.maintenance.name_required");
            else if (task.intervalValue < 1)
                v.tasks[i] = Tr("settings.maintenance.interval_required");
        }

        return v;
    }

    // ----------------------------------------------
    // Region & language
    // ----------------------------------------------

    bool DrawRegion(SettingsData& data)
    {
        AppSettings& app = data.app;
        bool changed = false;

        const float fieldWidth = std::min(UI::Em(24.0f), ImGui::GetContentRegionAvail().x);

        // Language
        UI::FormLabel(Tr("settings.region.language"));
        UI::FieldHelp(Tr("settings.region.language_help"));

        const auto languages = Localization::AvailableLanguages();

        std::string preview = Tr("settings.region.language_auto");

        for (const auto& language : languages)
        {
            if (language.tag == app.locale)
                preview = language.name;
        }

        ImGui::SetNextItemWidth(fieldWidth);

        if (ImGui::BeginCombo("##Language", preview.c_str()))
        {
            if (ImGui::Selectable(Tr("settings.region.language_auto"), app.locale == "auto"))
            {
                changed = changed || app.locale != "auto";
                app.locale = "auto";
            }

            for (const auto& language : languages)
            {
                ImGui::PushID(language.tag.c_str());

                if (ImGui::Selectable(language.name.c_str(), app.locale == language.tag))
                {
                    changed = changed || app.locale != language.tag;
                    app.locale = language.tag;
                }

                ImGui::PopID();
            }

            ImGui::EndCombo();
        }

        ImGui::Spacing();
        ImGui::Spacing();

        // Date format
        UI::FormLabel(Tr("settings.region.date_format"));
        UI::FieldHelp(Tr("settings.region.date_format_help"));

        const std::string localeDefault = Tr("format.date_pattern");

        const std::string autoLabel = Localization::Format(
            "settings.region.date_auto",
            { { "example", TodayIn(localeDefault) } }
        );

        const std::string datePreview = app.datePattern == "auto"
            ? autoLabel
            : DatePatternLabel(app.datePattern);

        ImGui::SetNextItemWidth(fieldWidth);

        if (ImGui::BeginCombo("##DateFormat", datePreview.c_str()))
        {
            if (ImGui::Selectable(autoLabel.c_str(), app.datePattern == "auto"))
            {
                changed = changed || app.datePattern != "auto";
                app.datePattern = "auto";
            }

            for (int i = 0; i < DatePatternCount; ++i)
            {
                ImGui::PushID(i);

                if (ImGui::Selectable(DatePatternLabel(DatePatterns[i]).c_str(), app.datePattern == DatePatterns[i]))
                {
                    changed = changed || app.datePattern != DatePatterns[i];
                    app.datePattern = DatePatterns[i];
                }

                ImGui::PopID();
            }

            ImGui::EndCombo();
        }

        ImGui::Spacing();
        ImGui::Spacing();

        // Temperature
        UI::FormLabel(Tr("settings.region.temperature"));
        UI::FieldHelp(Tr("settings.region.temperature_help"));

        const char* units[] = { Tr("units.fahrenheit"), Tr("units.celsius") };
        int unit = app.temperatureUnit == TemperatureUnit::Celsius ? 1 : 0;

        if (UI::SegmentedControl("Temperature", units, 2, unit))
        {
            app.temperatureUnit = unit == 1 ? TemperatureUnit::Celsius : TemperatureUnit::Fahrenheit;
            changed = true;
        }

        return changed;
    }

    // ----------------------------------------------
    // Seasons
    // ----------------------------------------------

    bool DrawSeasons(SettingsData& data, const Validation& validation)
    {
        AppSettings& app = data.app;
        bool changed = false;

        UI::FormLabel(Tr("settings.seasons.hemisphere"));
        UI::FieldHelp(Tr("settings.seasons.hemisphere_help"));

        const char* hemispheres[] = { Tr("settings.seasons.northern"), Tr("settings.seasons.southern") };
        int hemisphere = app.hemisphere == Hemisphere::Southern ? 1 : 0;

        if (UI::SegmentedControl("Hemisphere", hemispheres, 2, hemisphere))
        {
            app.hemisphere = hemisphere == 1 ? Hemisphere::Southern : Hemisphere::Northern;

            // Reset dates to that hemisphere's defaults.
            app.seasonStarts = SettingsDefaults::SeasonStarts(app.hemisphere);
            SettingsDefaults::OffSeason(app.hemisphere, app.offSeasonStart, app.offSeasonEnd);
            changed = true;
        }

        SubHeading(Tr("settings.seasons.starts"), Tr("settings.seasons.starts_help"));

        if (ImGui::BeginTable("SeasonStarts", 2,
            ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoHostExtendX |
            ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH))
        {
            ImGui::TableSetupColumn(Tr("settings.seasons.season"));
            ImGui::TableSetupColumn(Tr("settings.seasons.starts_on"));
            ImGui::TableHeadersRow();

            for (int s = 0; s < SeasonCount; ++s)
            {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted(SeasonName(static_cast<Season>(s)));

                ImGui::TableNextColumn();
                ImGui::PushID(s);
                changed = MonthDayInput("Start", app.seasonStarts[s]) || changed;
                ImGui::PopID();
            }

            ImGui::EndTable();
        }

        UI::FieldError(validation.seasons);

        if (validation.seasons.empty())
        {
            const Season current = Seasons::SeasonFor(DateFormat::Today(), app.seasonStarts);

            const std::string text = Localization::Format(
                "settings.seasons.current",
                { { "season", SeasonName(current) } }
            );

            ImGui::Spacing();
            UI::TextStyled(UI::Text::Caption, Theme::Colors::TextSecondary, text.c_str());
        }

        return changed;
    }

    // ----------------------------------------------
    // Mite monitoring
    // ----------------------------------------------

    bool DrawMiteMonitoring(SettingsData& data, const Validation& validation)
    {
        using namespace Theme::Colors;

        AppSettings& app = data.app;
        bool changed = false;

        changed = ImGui::Checkbox(Localization::Label("settings.mites.track", "TrackMites").c_str(), &app.miteTrackingEnabled) || changed;
        UI::FieldHelp(Tr("settings.mites.track_help"));

        if (!app.miteTrackingEnabled)
        {
            ImGui::Spacing();
            UI::TextStyledWrapped(UI::Text::Body, TextSecondary, Tr("settings.mites.tracking_off"));
            return changed;
        }

        SubHeading(Tr("settings.mites.methods"), Tr("settings.mites.methods_help"));

        if (!validation.methodSelection.empty())
        {
            UI::TextStyledWrapped(UI::Text::Body, Warning, validation.methodSelection.c_str());
            ImGui::Spacing();
        }

        // Legend
        UI::StatusChip(Tr("settings.mites.watch"), Warning);
        ImGui::SameLine();
        UI::StatusChip(Tr("settings.mites.act"), Danger);

        // Caption vertically centred on the chips.
        const char* blankHelp = Tr("settings.mites.blank_help");
        UI::SameLineOrWrap(ImGui::CalcTextSize(blankHelp).x);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + UI::Em(0.18f));
        UI::TextStyled(UI::Text::Caption, TextMuted, blankHelp);

        ImGui::Spacing();

        for (size_t i = 0; i < data.miteMethods.size(); ++i)
        {
            MiteCheckMethod& method = data.miteMethods[i];

            if (method.archived)
                continue;

            ImGui::PushID(static_cast<int>(i));
            UI::BeginCard("Method");

            // Header: enabled + name (+ remove for custom)
            changed = ImGui::Checkbox("##Enabled", &method.enabled) || changed;
            ImGui::SameLine();

            if (method.IsBuiltIn())
            {
                ImGui::AlignTextToFramePadding();
                UI::TextStyled(UI::Text::Title, method.enabled ? TextPrimary : TextSecondary, MethodName(method).c_str());
            }
            else
            {
                ImGui::SetNextItemWidth(std::min(UI::Em(16.0f), ImGui::GetContentRegionAvail().x));
                changed = ImGui::InputTextWithHint("##Name", Tr("settings.mites.name_hint"), &method.customName) || changed;
            }

            const std::string unitText = UnitShort(method.unit);
            UI::SameLineOrWrap(ImGui::CalcTextSize(unitText.c_str()).x);
            ImGui::AlignTextToFramePadding();
            UI::TextStyled(UI::Text::Caption, TextMuted, unitText.c_str());

            if (!method.IsBuiltIn())
            {
                const std::string removeLabel = Localization::Label("common.remove", "Remove");
                UI::SameLineOrWrap(UI::ButtonWidth(removeLabel.c_str()));

                if (UI::Button(removeLabel.c_str(), UI::ButtonKind::Secondary, -1.0f))
                {
                    method.archived = true;
                    changed = true;
                }
            }

            if (method.enabled && !method.archived)
            {
                ImGui::Spacing();

                // Unit (custom methods only)
                if (!method.IsBuiltIn())
                {
                    ImGui::AlignTextToFramePadding();
                    ImGui::TextUnformatted(Tr("settings.mites.measured_as"));
                    ImGui::SameLine();

                    const MiteUnit unitsList[] = { MiteUnit::PercentBees, MiteUnit::MitesPerDay, MiteUnit::PercentCells };

                    // Wide enough for the longest (translated) option.
                    float unitWidth = 0.0f;

                    for (MiteUnit unit : unitsList)
                        unitWidth = std::max(unitWidth, ImGui::CalcTextSize(UnitShort(unit)).x);

                    ImGui::SetNextItemWidth(std::min(
                        unitWidth + ImGui::GetStyle().FramePadding.x * 2.0f + ImGui::GetFrameHeight(),
                        ImGui::GetContentRegionAvail().x
                    ));

                    if (ImGui::BeginCombo("##Unit", UnitShort(method.unit)))
                    {

                        for (MiteUnit unit : unitsList)
                        {
                            if (ImGui::Selectable(UnitShort(unit), unit == method.unit))
                            {
                                method.unit = unit;
                                changed = true;
                            }
                        }

                        ImGui::EndCombo();
                    }
                }

                // Sample size
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted(SampleLabel(method.unit));
                ImGui::SameLine();
                ImGui::SetNextItemWidth(UI::Em(UI::Controls::NumberFieldEm));
                changed = UI::IntInput("##Sample", method.sampleSize, 1, 100000) || changed;

                // Thresholds per season
                ImGui::Spacing();

                const ImGuiTableFlags tableFlags =
                    ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoHostExtendX |
                    ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH;

                if (ImGui::BeginTable("Thresholds", 3, tableFlags))
                {
                    ImGui::TableSetupColumn(Tr("settings.seasons.season"));
                    ImGui::TableSetupColumn(Tr("settings.mites.watch"), ImGuiTableColumnFlags_WidthFixed, UI::Em(UI::Controls::NumberFieldEm + 1.0f));
                    ImGui::TableSetupColumn(Tr("settings.mites.act"), ImGuiTableColumnFlags_WidthFixed, UI::Em(UI::Controls::NumberFieldEm + 1.0f));

                    // Coloured header labels.
                    ImGui::TableNextRow(ImGuiTableRowFlags_Headers);

                    for (int c = 0; c < 3; ++c)
                    {
                        ImGui::TableSetColumnIndex(c);
                        const ImVec4 color = c == 1 ? Warning : (c == 2 ? Danger : TextSecondary);
                        UI::TextStyled(UI::Text::Label, color, ImGui::TableGetColumnName(c));
                    }

                    for (int s = 0; s < SeasonCount; ++s)
                    {
                        ImGui::PushID(s);
                        ImGui::TableNextRow();

                        const MiteThreshold& level = method.thresholds[s];

                        // Same rules as Validate(): outline the
                        // offending cells in red.
                        const bool orderBad = level.watch && level.act && *level.watch > *level.act;
                        const bool watchBad = orderBad || (IsPercent(method.unit) && level.watch && *level.watch > 100.0);
                        const bool actBad = orderBad || (IsPercent(method.unit) && level.act && *level.act > 100.0);

                        auto beginInvalid = [](bool invalid)
                        {
                            if (invalid)
                            {
                                ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, UI::Px(1.5f));
                                ImGui::PushStyleColor(ImGuiCol_Border, Danger);
                            }
                        };

                        auto endInvalid = [](bool invalid)
                        {
                            if (invalid)
                            {
                                ImGui::PopStyleColor();
                                ImGui::PopStyleVar();
                            }
                        };

                        ImGui::TableNextColumn();
                        ImGui::AlignTextToFramePadding();
                        ImGui::TextUnformatted(SeasonName(static_cast<Season>(s)));

                        ImGui::TableNextColumn();
                        ImGui::SetNextItemWidth(-FLT_MIN);
                        beginInvalid(watchBad);
                        changed = UI::OptionalNumberInput("##Watch", method.thresholds[s].watch, "-") || changed;
                        endInvalid(watchBad);

                        ImGui::TableNextColumn();
                        ImGui::SetNextItemWidth(-FLT_MIN);
                        beginInvalid(actBad);
                        changed = UI::OptionalNumberInput("##Act", method.thresholds[s].act, "-") || changed;
                        endInvalid(actBad);

                        ImGui::PopID();
                    }

                    ImGui::EndTable();
                }
            }

            if (i < validation.methods.size())
                UI::FieldError(validation.methods[i]);

            UI::EndCard();
            ImGui::PopID();

            ImGui::Spacing();
        }

        const std::string addLabel = Localization::Label("settings.mites.add_custom", "AddMethod");

        if (UI::Button(addLabel.c_str()))
        {
            MiteCheckMethod method;
            method.unit = MiteUnit::PercentBees;
            method.sampleSize = 300;
            data.miteMethods.push_back(method);
            changed = true;
        }

        return changed;
    }

    // ----------------------------------------------
    // Maintenance
    // ----------------------------------------------

    bool DrawMaintenance(SettingsData& data, const Validation& validation)
    {
        using namespace Theme::Colors;

        AppSettings& app = data.app;
        bool changed = false;

        // ---------------- Off-season ----------------
        UI::FormLabel(Tr("settings.maintenance.off_season"));
        UI::FieldHelp(Tr("settings.maintenance.off_season_help"));

        changed = ImGui::Checkbox(
            Localization::Label("settings.maintenance.pause_enabled", "PauseEnabled").c_str(),
            &app.offSeasonPauseEnabled) || changed;

        if (app.offSeasonPauseEnabled)
        {
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(Tr("settings.maintenance.pause_from"));
            ImGui::SameLine();
            changed = MonthDayInput("OffStart", app.offSeasonStart) || changed;

            UI::SameLineOrWrap(UI::Em(UI::Controls::ComboFieldEm + 8.0f));
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(Tr("settings.maintenance.pause_until"));
            ImGui::SameLine();
            changed = MonthDayInput("OffEnd", app.offSeasonEnd) || changed;

            const std::string winterLabel = Localization::Label("settings.maintenance.use_winter", "UseWinter");
            UI::SameLineOrWrap(UI::ButtonWidth(winterLabel.c_str()));

            if (UI::Button(winterLabel.c_str(), UI::ButtonKind::Secondary, -1.0f))
            {
                app.offSeasonStart = app.seasonStarts[static_cast<int>(Season::Winter)];
                app.offSeasonEnd = app.seasonStarts[static_cast<int>(Season::Spring)];
                changed = true;
            }

            UI::FieldError(validation.offSeason);
        }

        // ---------------- Tasks ----------------
        SubHeading(Tr("settings.maintenance.tasks"), Tr("settings.maintenance.tasks_help"));

        const ImGuiTableFlags tableFlags =
            ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg |
            ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_PadOuterX;

        if (ImGui::BeginTable("Tasks", 4, tableFlags))
        {
            ImGui::TableSetupColumn(Tr("settings.maintenance.col_on"));
            ImGui::TableSetupColumn(Tr("settings.maintenance.col_task"), ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn(Tr("settings.maintenance.col_interval"));
            ImGui::TableSetupColumn("##Actions");
            ImGui::TableHeadersRow();

            for (size_t i = 0; i < data.taskTypes.size(); ++i)
            {
                MaintenanceTaskType& task = data.taskTypes[i];

                if (task.archived)
                    continue;

                const bool miteTask = task.key == SettingsDefaults::MiteCheckTaskKey;
                const bool unavailable = miteTask && !app.miteTrackingEnabled;

                ImGui::PushID(static_cast<int>(i));
                ImGui::TableNextRow();

                ImGui::BeginDisabled(unavailable);

                // On
                ImGui::TableNextColumn();
                changed = ImGui::Checkbox("##Enabled", &task.enabled) || changed;

                // Task name
                ImGui::TableNextColumn();

                if (task.IsBuiltIn())
                {
                    ImGui::AlignTextToFramePadding();
                    ImGui::TextUnformatted(TaskName(task).c_str());
                }
                else
                {
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    changed = ImGui::InputTextWithHint("##Name", Tr("settings.maintenance.name_hint"), &task.customName) || changed;
                }

                if (unavailable)
                    UI::TextStyledWrapped(UI::Text::Caption, TextMuted, Tr("settings.maintenance.mite_tracking_off"));

                if (i < validation.tasks.size())
                    UI::FieldError(validation.tasks[i]);

                // Interval: number + unit (+ "after treatment")
                ImGui::TableNextColumn();
                ImGui::SetNextItemWidth(UI::Em(4.0f));
                changed = UI::IntInput("##Interval", task.intervalValue, 1, 999) || changed;
                ImGui::SameLine();
                ImGui::SetNextItemWidth(UI::Em(UI::Controls::ComboFieldEm));

                if (ImGui::BeginCombo("##Unit", IntervalUnitName(task.intervalUnit)))
                {
                    const IntervalUnit unitsList[] = { IntervalUnit::Days, IntervalUnit::Weeks, IntervalUnit::Months, IntervalUnit::Years };

                    for (IntervalUnit unit : unitsList)
                    {
                        if (ImGui::Selectable(IntervalUnitName(unit), unit == task.intervalUnit))
                        {
                            task.intervalUnit = unit;
                            changed = true;
                        }
                    }

                    ImGui::EndCombo();
                }

                if (task.trigger == TaskTrigger::AfterTreatment)
                    UI::TextStyled(UI::Text::Caption, TextMuted, Tr("settings.maintenance.after_treatment"));

                ImGui::EndDisabled();

                // Remove (custom only)
                ImGui::TableNextColumn();

                if (!task.IsBuiltIn())
                {
                    const std::string removeLabel = Localization::Label("common.remove", "Remove");

                    if (UI::Button(removeLabel.c_str(), UI::ButtonKind::Secondary, -1.0f))
                    {
                        task.archived = true;
                        changed = true;
                    }
                }

                ImGui::PopID();
            }

            ImGui::EndTable();
        }

        ImGui::Spacing();

        const std::string addLabel = Localization::Label("settings.maintenance.add_custom", "AddTask");

        if (UI::Button(addLabel.c_str()))
        {
            MaintenanceTaskType task;
            task.intervalValue = 1;
            task.intervalUnit = IntervalUnit::Months;
            data.taskTypes.push_back(task);
            changed = true;
        }

        return changed;
    }

    bool DrawSection(Section section, SettingsData& data, const Validation& validation)
    {
        switch (section)
        {
            case Section::Region:         return DrawRegion(data);
            case Section::Seasons:        return DrawSeasons(data, validation);
            case Section::MiteMonitoring: return DrawMiteMonitoring(data, validation);
            case Section::Maintenance:    return DrawMaintenance(data, validation);
            case Section::Count:          break;
        }

        return false;
    }

    // ----------------------------------------------
    // Review
    // ----------------------------------------------

    void DrawReview(const SettingsData& data)
    {
        using namespace Theme::Colors;

        const AppSettings& app = data.app;

        auto row = [](const char* label, const std::string& value)
        {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            UI::TextStyled(UI::Text::Label, TextSecondary, label);
            ImGui::TableNextColumn();
            ImGui::PushTextWrapPos(0.0f);
            ImGui::TextUnformatted(value.c_str());
            ImGui::PopTextWrapPos();
        };

        // The card is always begun/ended; the table only
        // when BeginTable() returned true (it can be
        // skipped while clipped or measuring).
        bool tableOpen = false;

        auto beginSection = [&tableOpen](Section section, const char* id)
        {
            UI::BeginCard(id);
            UI::TextStyled(UI::Text::Title, TextPrimary, SectionTitle(section));
            ImGui::Spacing();

            tableOpen = ImGui::BeginTable("Rows", 2, ImGuiTableFlags_SizingFixedFit);

            if (tableOpen)
            {
                // Same label width in every card so values line up.
                ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, UI::Em(13.0f));
                ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
            }

            return tableOpen;
        };

        auto endSection = [&tableOpen]()
        {
            if (tableOpen)
                ImGui::EndTable();

            tableOpen = false;
            UI::EndCard();
            ImGui::Spacing();
        };

        // Region
        if (beginSection(Section::Region, "ReviewRegion"))
        {
            std::string language = Tr("settings.region.language_auto");

            for (const auto& info : Localization::AvailableLanguages())
            {
                if (info.tag == app.locale)
                    language = info.name;
            }

            row(Tr("settings.region.language"), language);
            row(Tr("settings.region.date_format"),
                app.datePattern == "auto"
                    ? Localization::Format("settings.region.date_auto", { { "example", TodayIn(Tr("format.date_pattern")) } })
                    : DatePatternLabel(app.datePattern));
            row(Tr("settings.region.temperature"),
                app.temperatureUnit == TemperatureUnit::Celsius ? Tr("units.celsius") : Tr("units.fahrenheit"));
        }

        endSection();

        // Seasons
        if (beginSection(Section::Seasons, "ReviewSeasons"))
        {
            row(Tr("settings.seasons.hemisphere"),
                app.hemisphere == Hemisphere::Southern ? Tr("settings.seasons.southern") : Tr("settings.seasons.northern"));

            for (int s = 0; s < SeasonCount; ++s)
                row(SeasonName(static_cast<Season>(s)), MonthDayText(app.seasonStarts[s]));
        }

        endSection();

        // Mite monitoring
        if (beginSection(Section::MiteMonitoring, "ReviewMites"))
        {
            if (!app.miteTrackingEnabled)
            {
                row(Tr("settings.mites.track"), Tr("common.off"));
            }
            else
            {
                int enabled = 0;

                for (const MiteCheckMethod& method : data.miteMethods)
                {
                    if (method.archived || !method.enabled)
                        continue;

                    ++enabled;

                    const std::string detail = Localization::Format(
                        "settings.review.method_detail",
                        {
                            { "sample", std::to_string(method.sampleSize) },
                            { "sample_label", SampleLabel(method.unit) },
                            { "unit", UnitShort(method.unit) }
                        }
                    );

                    row(MethodName(method).c_str(), detail);
                }

                if (enabled == 0)
                    row(Tr("settings.mites.methods"), Tr("settings.review.no_methods"));
            }
        }

        endSection();

        // Maintenance
        if (beginSection(Section::Maintenance, "ReviewMaintenance"))
        {
            row(Tr("settings.maintenance.off_season"),
                app.offSeasonPauseEnabled
                    ? Localization::Format(
                        "settings.review.off_season_range",
                        { { "start", MonthDayText(app.offSeasonStart) }, { "end", MonthDayText(app.offSeasonEnd) } })
                    : std::string(Tr("common.off")));

            for (const MaintenanceTaskType& task : data.taskTypes)
            {
                if (task.archived || !task.enabled)
                    continue;

                if (task.key == SettingsDefaults::MiteCheckTaskKey && !app.miteTrackingEnabled)
                    continue;

                row(TaskName(task).c_str(), IntervalText(task));
            }
        }

        endSection();
    }
}
