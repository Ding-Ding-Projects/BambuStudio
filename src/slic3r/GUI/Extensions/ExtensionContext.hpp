#ifndef slic3r_GUI_Extensions_ExtensionContext_hpp_
#define slic3r_GUI_Extensions_ExtensionContext_hpp_

// What Bambu Studio tells an extension, and the one change it makes to a file
// on an extension's behalf, without wxWidgets: the "slice", "printer", "model"
// and "send" sections of a request built from plain facts the app gathered, the
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
    int         plate = 0;         // set when every plate is added up: the plate it came from
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
    int                           plate_count = 1; // more than 1: every plate added up (see combine_plates)
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
// model.imported: the model section of only the objects that arrived, with
// "imported": true, the names of the files they came from (never a folder;
// at most 50) and "units": "mm", the unit every size is in.
nlohmann::json imported_section(const std::vector<ModelObjectFacts> &objects, int plate, const std::vector<std::string> &files);

// A request context made of invented values, marked "sample": true in every
// section, for trying an extension without slicing anything.
nlohmann::json sample_context(Hook hook);

// ---------------------------------------------------------------- after a print ends
struct PrintEndFacts
{
    std::string result;               // finished, failed or cancelled
    std::string printer_model;        // the model, as the app names it; never the printer's own name
    double      seconds        = -1;  // how long Bambu Studio saw it run; < 0 when unknown
    bool        seen_from_start = false; // false: it was already running when Bambu Studio started watching
    int         layers         = 0;   // the job's layers, as the printer reports them; 0 when unknown
    int         error          = 0;   // the printer's error code for a failed print; 0 when none
    // When Bambu Studio sent this print and saw it start: the filament the
    // slice planned per slot. A failed or cancelled print used less.
    bool                     planned_known = false;
    std::vector<FilamentUse> planned;
};

// The "print" section of a print.finished request (needs printer.read).
nlohmann::json print_section(const PrintEndFacts &facts);

// ---------------------------------------------------------------- before a print is sent
struct SendFilamentFacts
{
    int         slot = 0;            // 1-based, the project's filament
    std::string type;                // as sliced
    std::string color;               // as sliced: #RRGGBB, RRGGBB or RRGGBBAA
    std::string tray;                // the printer's name for the slot it is mapped to ("A1", "Ext"); empty when unmapped
    std::string tray_type;           // what the printer reports in that slot
    std::string tray_color;
    int         remain_percent = -1; // 0 to 100; -1 when the printer does not say
};

struct SendFacts
{
    // Where the print comes from and goes: "project" (a plate of this project
    // to the printer), "storage" (to the printer's storage, not printed),
    // "several" (one plate to several printers), "printer-file" (a file
    // already on the printer) or "queue" (a job from the print queue). Only
    // the first three were sliced in this project, so only they carry "slice".
    std::string                                      source     = "project";
    std::string                                      file;            // printer-file and queue: the file's or job's name, never a path
    bool                                             to_queue   = false; // project: added to the print queue instead of printed now
    int                                              printers   = 1;  // several: how many printers
    std::vector<std::string>                         printer_models;  // several: each model once
    int                                              plate      = 0; // 1-based; 0 when every plate is sent
    bool                                             all_plates = false;
    std::string                                      printer_model;   // the model of the printer it goes to, as the app names it
    bool                                             sliced_for_this_model = true;
    std::string                                      connection;      // "cloud" or "lan"
    std::vector<std::pair<std::string, std::string>> options;         // the print dialog's options shown, id and value
    std::vector<SendFilamentFacts>                   filaments;
};

// The "send" section of a print.before_send request (needs printer.read). It
// never holds the printer's own name, serial number, address or access code:
// SendFacts has no place for them.
nlohmann::json send_section(const SendFacts &facts);
// Whether this send was sliced in this project, so the request may carry the
// project's "slice" section.
bool sliced_here(const SendFacts &facts);
// Every sliced plate as one, for sending all plates at once: plate 0, times,
// layers, filament changes and each slot's filament added up, the bed type
// when every plate shares it, warnings in plate order (at most 20).
SliceFacts combine_plates(const std::vector<SliceFacts> &plates);
// "#RRGGBB" in capitals from #RRGGBB, RRGGBB, #RRGGBBAA or RRGGBBAA; empty for anything else.
std::string normalise_color(const std::string &color);

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
