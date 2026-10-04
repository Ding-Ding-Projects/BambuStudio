#include "SettingsDraftUndo.hpp"
#include "libslic3r/PresetBundle.hpp"

namespace Slic3r { namespace GUI {

std::shared_ptr<const SettingsDraftUndoState> SettingsDraftUndoState::capture(
    const PresetBundle& bundle, const std::string& filename, size_t generation)
{
    auto state = std::make_shared<SettingsDraftUndoState>();
    state->configs = { bundle.prints.get_edited_preset().config,
                      bundle.filaments.get_edited_preset().config,
                      bundle.printers.get_edited_preset().config };
    state->preset_names = { bundle.prints.get_selected_preset_name(),
                           bundle.filaments.get_selected_preset_name(),
                           bundle.printers.get_selected_preset_name() };
    state->project_filename = filename;
    state->project_generation = generation;
    return state;
}

bool SettingsDraftUndoState::matches(const PresetBundle& bundle, const std::string& filename, size_t generation) const
{
    return project_generation == generation && project_filename == filename &&
           preset_names == std::array<std::string, 3>{ bundle.prints.get_selected_preset_name(),
                                                     bundle.filaments.get_selected_preset_name(),
                                                     bundle.printers.get_selected_preset_name() };
}

bool SettingsDraftUndoState::restore(PresetBundle& bundle, const std::string& filename, size_t generation) const
{
    if (!matches(bundle, filename, generation))
        return false;
    // Allocate all three copies before mutating live state, then use noexcept moves.
    auto restored = configs;
    bundle.prints.get_edited_preset().config = std::move(restored[0]);
    bundle.filaments.get_edited_preset().config = std::move(restored[1]);
    bundle.printers.get_edited_preset().config = std::move(restored[2]);
    return true;
}

} } // namespace Slic3r::GUI
