#ifndef slic3r_GUI_SettingsDraftUndo_hpp_
#define slic3r_GUI_SettingsDraftUndo_hpp_

#include "../Utils/UndoRedo.hpp"
#include "libslic3r/PrintConfig.hpp"
#include <array>
#include <cstddef>
#include <memory>
#include <string>

namespace Slic3r {
class PresetBundle;
namespace GUI {

// Complete edited configurations, not just the keys displayed by the draft editor.
struct SettingsDraftUndoState final : UndoRedo::SnapshotAttachment
{
    std::array<DynamicPrintConfig, 3> configs;
    std::array<std::string, 3> preset_names;
    std::string project_filename;
    size_t project_generation = 0;

    static std::shared_ptr<const SettingsDraftUndoState> capture(
        const PresetBundle& bundle, const std::string& filename, size_t generation);
    bool matches(const PresetBundle& bundle, const std::string& filename, size_t generation) const;
    bool restore(PresetBundle& bundle, const std::string& filename, size_t generation) const;
};

} // namespace GUI
} // namespace Slic3r
#endif
