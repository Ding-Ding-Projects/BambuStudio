#pragma once
// Pure (standard library only) description of what the narrator can say.
//
// An event carries at most one integer (a printer error code). It never carries
// a file path, account name, token, printer name or any other free text, so the
// spoken summary cannot disclose private data by construction.
#include <algorithm>
#include <string>

namespace Slic3r::GUI::Narration {

// Independent speech tracks. A track has its own switch, its own replacement
// and cooldown bookkeeping, and an event on one track never replaces, delays
// or cancels a queued event on another.
enum class Track { Printer, App, Accessibility };

enum class EventKind {
    PrintStarted, PrintPaused, PrintResumed, PrintFinished, PrintFailed, PrintCancelled,
    FilamentRunout, AmsError, PrinterError,
    SliceFinished, SliceFailed, ExportDone, UpdateReady, AppError,
    VoicePreview
};

enum class Tone { Serious, Friendly, Playful };

inline constexpr int kFunnyMin = 1;
inline constexpr int kFunnyMax = 5;

// Funny levels 1 and 2 are plain, 3 is friendly, 4 and 5 are playful.
// The facts in every variant are identical; only the delivery changes.
inline Tone tone_for_level(int level)
{
    level = std::clamp(level, kFunnyMin, kFunnyMax);
    return level <= 2 ? Tone::Serious : level == 3 ? Tone::Friendly : Tone::Playful;
}

inline Track track_of(EventKind kind)
{
    switch (kind) {
    case EventKind::PrintStarted: case EventKind::PrintPaused: case EventKind::PrintResumed:
    case EventKind::PrintFinished: case EventKind::PrintFailed: case EventKind::PrintCancelled:
    case EventKind::FilamentRunout: case EventKind::AmsError: case EventKind::PrinterError:
        return Track::Printer;
    case EventKind::VoicePreview:
        return Track::Accessibility;
    default:
        return Track::App;
    }
}

// Errors name the failure and are never held back by cooldown or replaced.
inline bool is_error(EventKind kind)
{
    return kind == EventKind::PrintFailed || kind == EventKind::FilamentRunout || kind == EventKind::AmsError ||
           kind == EventKind::PrinterError || kind == EventKind::SliceFailed || kind == EventKind::AppError;
}

inline const char* track_name(Track track)
{
    return track == Track::Printer ? "printer" : track == Track::App ? "app" : "accessibility";
}

// Queue category. Errors keep the literal "error" so the queue's priority rules apply.
inline std::string category_of(EventKind kind)
{
    return is_error(kind) ? "error" : kind == EventKind::VoicePreview ? "preview" : "state";
}

// Only a printer error carries a value, formatted by the caller with %06X.
inline bool has_code(EventKind kind) { return kind == EventKind::PrinterError; }

// English source strings (msgids). Cantonese comes from the catalogue.
// Rows follow EventKind; columns are Serious, Friendly, Playful.
inline const char* const* utterance_row(EventKind kind)
{
    static const char* const rows[][3] = {
        {"Printing started.", "Your print has started.", "And we're off. The print has started."},
        {"Print paused.", "The print is paused.", "The print has paused. Take a breather."},
        {"Printing resumed.", "The print has resumed.", "Back to work. The print has resumed."},
        {"Print finished.", "Your print is finished.", "Print finished. Time to peel it off the plate."},
        {"Print failed.", "The print has failed. Check the printer.", "Oh no, the print failed. Check the printer."},
        {"Print cancelled.", "The print was cancelled.", "The print was cancelled. Nothing more to see here."},
        {"Filament has run out. Load more filament.", "The printer has run out of filament. Load more to continue.",
         "The spool is empty and the printer is hungry. Load more filament."},
        {"AMS error. Check the printer screen.", "The AMS reported an error. Check the printer screen.",
         "The AMS is complaining. Check the printer screen."},
        {"Printer error %06X. Check the device screen for details.", "Printer error %06X. Check the device screen for details.",
         "Printer error %06X. Check the device screen for details."},
        {"Slicing finished.", "Slicing is finished. Your plate is ready.", "Slicing done. Fresh layers, straight from the oven."},
        {"Slicing failed. Check the notification for details.", "Slicing failed. Check the notification for details.",
         "Slicing failed. Check the notification for details."},
        {"Export finished.", "Export finished successfully.", "Export done. Your file is safely written."},
        {"An update is ready. Restart to install it.", "An update is ready. Restart the app to install it.",
         "A fresh version is ready. Restart the app to install it."},
        {"An error occurred. Check the notification for details.", "An error occurred. Check the notification for details.",
         "An error occurred. Check the notification for details."},
        {"This is a preview of the selected narrator voice.", "This is a preview of the selected narrator voice.",
         "This is a preview of the selected narrator voice."},
    };
    return rows[static_cast<int>(kind)];
}

inline const char* utterance_msgid(EventKind kind, Tone tone)
{
    return utterance_row(kind)[static_cast<int>(tone)];
}

inline constexpr int kEventKindCount = static_cast<int>(EventKind::VoicePreview) + 1;

struct Event {
    EventKind kind = EventKind::AppError;
    int code = 0; // only meaningful when has_code(kind)
    bool operator==(const Event& o) const { return kind == o.kind && code == o.code; }
};

} // namespace Slic3r::GUI::Narration
