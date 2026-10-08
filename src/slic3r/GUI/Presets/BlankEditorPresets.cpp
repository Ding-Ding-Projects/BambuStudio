#include "BlankEditorPresets.hpp"

#include "libslic3r/PresentationDefaults.hpp"

#include <utility>

// L() only marks a literal for extraction (bbl/i18n/list.txt); the GUI
// translates the stored English text where it shows it.
#ifndef L
#define L(s) s
#endif

namespace Slic3r { namespace GUI { namespace BlankEditorPresets {

namespace {

bool accepts(const EditorSpec &spec, const std::string &key, const std::string &value)
{
    return !spec.valid || spec.valid(key, value);
}

Assignment assign(const Field &field, const std::string &value, bool from_default)
{
    Assignment assignment;
    assignment.key          = field.key;
    assignment.label        = field.label;
    assignment.value        = value;
    assignment.shown        = shown_value(field, value, assignment.shown_translatable);
    assignment.from_default = from_default;
    return assignment;
}

Omission omit(const Field &field, std::string reason)
{
    return Omission{field.key, field.label, std::move(reason)};
}

Preset shipped_defaults(const EditorSpec &spec)
{
    Preset preset;
    preset.id      = kShippedDefaultsId;
    preset.origin  = Origin::ShippedDefaults;
    preset.title   = L("Shipped defaults");
    preset.creates = spec.creates_defaults;
    for (const Field &field : spec.fields) {
        const auto shipped = PresentationDefaults::value(field.key);
        if (!shipped) {
            const char *reason = unfixed_reason(field.key);
            preset.left_out.push_back(omit(field, reason ? reason : L("The app does not ship a fixed value for this setting.")));
        } else if (!accepts(spec, field.key, *shipped)) {
            preset.left_out.push_back(omit(field, L("This editor does not accept the shipped value for this setting.")));
        } else {
            preset.sets.push_back(assign(field, *shipped, false));
        }
    }
    return preset;
}

Preset saved_settings(const EditorSpec &spec, const Values &saved)
{
    Preset preset;
    preset.id      = kSavedSettingsId;
    preset.origin  = Origin::SavedSettings;
    preset.title   = L("Your saved settings");
    preset.creates = spec.creates_saved;
    for (const Field &field : spec.fields) {
        const auto stored = saved.find(field.key);
        if (stored != saved.end() && accepts(spec, field.key, stored->second)) {
            preset.sets.push_back(assign(field, stored->second, false));
            continue;
        }
        // Nothing usable is saved, so the setting is running on its shipped
        // default: start from that and say so.
        const auto shipped = PresentationDefaults::value(field.key);
        if (shipped && accepts(spec, field.key, *shipped))
            preset.sets.push_back(assign(field, *shipped, true));
        else
            preset.left_out.push_back(omit(field, L("Nothing usable is saved for this setting, and the app does not ship a fixed value for it.")));
    }
    return preset;
}

Preset empty(const EditorSpec &spec)
{
    Preset preset;
    preset.id      = kEmptyId;
    preset.origin  = Origin::Empty;
    preset.title   = L("Start empty");
    preset.creates = spec.creates_empty;
    return preset;
}

} // namespace

Values Preset::values() const
{
    Values out;
    for (const Assignment &assignment : sets)
        out[assignment.key] = assignment.value;
    return out;
}

std::vector<Preset> start_presets(const EditorSpec &spec, const Values &saved)
{
    return {shipped_defaults(spec), saved_settings(spec, saved), empty(spec)};
}

Preset template_preset(std::string id, std::string title, std::string creates, std::vector<Assignment> sets)
{
    Preset preset;
    preset.id      = std::move(id);
    preset.origin  = Origin::Template;
    preset.title   = std::move(title);
    preset.creates = std::move(creates);
    preset.sets    = std::move(sets);
    return preset;
}

std::string shown_value(const Field &field, const std::string &value, bool &translatable)
{
    for (const Choice &choice : field.choices)
        if (choice.value == value) {
            translatable = true;
            return choice.label;
        }
    if (value.empty()) {
        translatable = true;
        return field.empty_label.empty() ? std::string(L("(empty)")) : field.empty_label;
    }
    translatable = false;
    return value;
}

const char *unfixed_reason(const std::string &key)
{
    if (key == "language")
        return L("Follows your system language the first time the app starts, so there is no single shipped value.");
    return PresentationDefaults::is_unfixed(key) ? L("The app does not ship a fixed value for this setting.") : nullptr;
}

} } } // namespace Slic3r::GUI::BlankEditorPresets
