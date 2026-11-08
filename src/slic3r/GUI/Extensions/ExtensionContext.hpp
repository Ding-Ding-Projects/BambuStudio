#ifndef slic3r_GUI_Extensions_ExtensionContext_hpp_
#define slic3r_GUI_Extensions_ExtensionContext_hpp_

// What Bambu Studio tells an extension, and the one change it makes to a file
// on an extension's behalf, without wxWidgets: the "slice", "printer" and
// "model" sections of a request built from plain facts the app gathered, the
// clearly marked sample requests used by "Try with a sample", and adding an
// extension's comment lines to an exported G-code file through a copy beside
// it. Tested on its own by tests/extension_context_test.cpp.

#include "ExtensionCore.hpp"

#include <nlohmann/json.hpp>

#include <array>
#include <cstddef>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace Slic3r { namespace GUI { namespace Extensions {

struct FilamentUse
{
    int         slot = 0;          // 1-based, as the plate shows it
    std::string type;              // PLA, PETG, ...
    std::string color;             // #RRGGBB, may be empty
    double      volume_mm3  = 0;   // everything the slot extrudes, flushes included
    double      density     = 0;   // g/cm3; 0 when the preset has none
    double      diameter    = 1.75;
    double      cost_per_kg = 0;   // 0 when the preset has none
};

struct SliceWarningFact
{
    int         level = 1;         // 0 tip, 1 warning, 2 error
    std::string code;
    std::string message;
};

struct SliceFacts
{
    int                           plate = 0; // 1-based
    std::string                   plate_name;
    double                        seconds         = 0;
    double                        prepare_seconds = 0;
    int                           layers          = 0;
    std::string                   bed_type;  // cool_plate, textured_plate, ...
    int                           filament_changes = 0;
    std::vector<FilamentUse>      filaments;
    std::vector<SliceWarningFact> warnings;
    std::string                   file;      // gcode.exported: the file name, never its folder
};

struct PrinterFacts
{
    std::string         preset;            // the printer preset name
    std::string         model;             // printer_model
    std::vector<double> nozzles;           // nozzle diameters
    std::vector<std::string> filament_presets;
};

struct ModelObjectFacts
{
    std::string           name;
    int                   instances = 1;
    std::size_t           triangles = 0;
    std::array<double, 3> size{{0, 0, 0}}; // millimetres
    bool                  selected  = false;
};

double grams(const FilamentUse &use);
double meters(const FilamentUse &use);

nlohmann::json slice_section(const SliceFacts &facts);
nlohmann::json printer_section(const PrinterFacts &facts);
nlohmann::json model_section(const std::vector<ModelObjectFacts> &objects, int plate);

// A request context made of invented values, marked "sample": true in every
// section, for trying an extension without slicing anything.
nlohmann::json sample_context(Hook hook);

// ---------------------------------------------------------------- G-code comment lines
struct CommentBlock
{
    std::string              extension; // id
    std::string              version;
    std::vector<std::string> prepend;   // every line starts with ';' (checked again here)
    std::vector<std::string> append;
};

// Adds every block's lines to the file: the start lines after Bambu Studio's
// header block (or at the very start when there is none), the end lines after
// the last line. Each block is introduced by "; added by extension <id> <version>".
// The file is written beside the original as "<name>.partial", checked for
// its exact size and then put in place; on any problem the original is left as
// it was. problem: stable ASCII: unreadable, not-comment, unwritable,
// write-failed, size-mismatch, rename-failed.
bool add_comment_lines(const std::filesystem::path &gcode, const std::vector<CommentBlock> &blocks, std::string &problem);

}}} // namespace Slic3r::GUI::Extensions

#endif // slic3r_GUI_Extensions_ExtensionContext_hpp_
