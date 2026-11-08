#pragma once

#include "PersonalVocabularySchema.hpp"

#include <wx/arrstr.h>
#include <wx/string.h>

#include <string>

class wxFileDropTarget;
class wxWindow;

// Apply a personal vocabulary file by dropping it anywhere on the main window.
//
//   * A drop of exactly one .json file goes through the same validation as the
//     Settings upload (PersonalVocabulary::load); a rejected file changes
//     nothing and the wording on screen stays as it was.
//   * A successful apply plays the Windows "tada" sound once and shows a
//     non-blocking, click-through celebration over the window for about three
//     seconds. Its headline is the wording the dropped file gives for
//     "private vocabulary", read at apply time; without one it reads
//     "Personal wording applied". Reduced motion shows it without animation.
//   * A rejected file plays the Windows error sound once and shows a short
//     notice naming the rule it broke, without the celebration.
//   * Both outcomes are announced to screen readers through UI Automation.
//   * Sounds are skipped while Windows reports quiet hours or presentation
//     mode; the notice and announcement still appear.
namespace Slic3r::GUI::VocabularyDrop {

// True when files is exactly one file with a .json extension.
bool is_vocabulary_file(const wxArrayString &files);

// Validate and apply file, then show and announce the outcome over frame.
void apply_dropped(wxWindow *frame, const wxString &file);

// One sentence naming the rule a vocabulary file broke; empty for None.
wxString rejection_text(PersonalVocabulary::Rejection reason);

// Drop target for windows that accept no other files: it handles vocabulary
// drops and refuses everything else.
wxFileDropTarget *create_drop_target(wxWindow *frame);

// A vocabulary file dropped on a page shown inside the application (the Home
// page and other local web pages). The page reads the file and hands over its
// bytes; they go through exactly the same validation as a file dropped on a
// native window. Nothing is kept outside the private copy the validator writes.
void apply_dropped_bytes(wxWindow *frame, const std::string &bytes);
// Shows and announces a refusal for a drop that never reached the validator,
// for example a page that could not read the file or a file over the size limit.
void refuse_dropped(wxWindow *frame, PersonalVocabulary::Rejection reason);

} // namespace Slic3r::GUI::VocabularyDrop
