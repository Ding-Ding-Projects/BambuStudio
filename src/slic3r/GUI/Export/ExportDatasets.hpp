#ifndef slic3r_GUI_Export_ExportDatasets_hpp_
#define slic3r_GUI_Export_ExportDatasets_hpp_

// Builders that turn each exportable surface's live data into a Dataset the
// shared ExportDialog can write in every format. Keeping them together makes
// the "every record the app owns is exportable" list auditable in one place.

#include "ExportFormats.hpp"

#include <string>
#include <vector>

namespace Slic3r {
class AppConfig;
class Model;
class Preset;
struct ProjectHistoryVersion;
struct GCodeProcessorResult;
} // namespace Slic3r

namespace Slic3r::GUI::Export {

// Project version history: one row per snapshot commit.
Dataset project_history_dataset(const std::vector<ProjectHistoryVersion> &versions, const std::string &project_name);

// Preferences / AppConfig: section -> key -> value tree.
Dataset app_config_dataset(const AppConfig &config);

// One preset (print / filament / printer): metadata plus every option
// serialized the way the .ini stores it.
Dataset preset_dataset(const Preset &preset, const std::string &preset_type);

// The object list: one row per model object with its geometry summary.
Dataset object_list_dataset(const Model &model);

// Print statistics of one sliced plate.
Dataset print_statistics_dataset(const GCodeProcessorResult &result, int plate_index, const std::string &plate_name);

} // namespace Slic3r::GUI::Export

#endif // slic3r_GUI_Export_ExportDatasets_hpp_
