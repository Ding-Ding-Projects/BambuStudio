#include "SettingsDraftUndo.hpp"
#include "libslic3r/PresetBundle.hpp"

namespace Slic3r { namespace GUI {

std::shared_ptr<const SettingsDraftUndoState> SettingsDraftUndoState::capture(
    const PresetBundle& bundle, const std::string& identity, size_t generation)
{
    auto state = std::make_shared<SettingsDraftUndoState>();
    state->configs = { bundle.prints.get_edited_preset().config,
                      bundle.filaments.get_edited_preset().config,
                      bundle.printers.get_edited_preset().config };
    state->preset_names = { bundle.prints.get_selected_preset_name(),
                           bundle.filaments.get_selected_preset_name(),
                           bundle.printers.get_selected_preset_name() };
    state->project_id = identity;
    state->project_generation = generation;
    return state;
}

size_t SettingsDraftUndoState::memsize() const
{
    size_t bytes = sizeof(*this) + project_id.capacity();
    for (const auto &name : preset_names) bytes += name.capacity();
    for (const auto &config : configs)
        for (const auto &key : config.keys())
            bytes += key.capacity() + config.opt_serialize(key).size() + sizeof(ConfigOption) + 128;
    return bytes;
}

bool SettingsDraftUndoState::matches(const PresetBundle& bundle, const std::string& identity, size_t generation) const
{
    return project_generation == generation && project_id == identity &&
           preset_names == std::array<std::string, 3>{ bundle.prints.get_selected_preset_name(),
                                                     bundle.filaments.get_selected_preset_name(),
                                                     bundle.printers.get_selected_preset_name() };
}

bool SettingsDraftUndoState::restore(PresetBundle& bundle, const std::string& identity, size_t generation) const
{
    if (!matches(bundle, identity, generation))
        return false;
    // Allocate all three copies before mutating live state, then use noexcept moves.
    auto restored = configs;
    bundle.prints.get_edited_preset().config = std::move(restored[0]);
    bundle.filaments.get_edited_preset().config = std::move(restored[1]);
    bundle.printers.get_edited_preset().config = std::move(restored[2]);
    return true;
}

} } // namespace Slic3r::GUI
