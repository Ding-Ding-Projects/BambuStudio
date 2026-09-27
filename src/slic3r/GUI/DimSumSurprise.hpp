#ifndef slic3r_GUI_DimSumSurprise_hpp_
#define slic3r_GUI_DimSumSurprise_hpp_

namespace Slic3r { namespace GUI { namespace DimSumSurprise {

// Startup dim sum surprise: on one launch in ten a small non-blocking card
// shows a randomly chosen dish from the public Ding-Ding-Projects/dim-sum-photos
// catalog, with its English and Traditional Chinese name and its photo.
//
//   * The draw is a fresh random draw per process and happens at most once.
//   * It never runs on a first launch, when files were passed on the command
//     line, when the config wizard ran, after a startup error, while a modal
//     dialog is open, or while the OS reports a quiet/presentation state.
//   * The card never takes focus, never blocks, and dismisses itself after a
//     few seconds. Click or Escape dismisses it early.
//   * Names and photos come only from the public catalog, cached under
//     data_dir()/dim-sum/. With an empty cache and no network the launch shows
//     nothing; there is no placeholder and no opt-out setting.
//
// Call once from GUI_App::post_init after the main frame is up and the
// startup wizard decision is known.
void maybe_show_after_startup(bool config_wizard_shown);

// Record that an error dialog was raised during startup; the surprise then
// stays home for this launch.
void mark_startup_error();

// True while a card is on screen (for tests and the visual smoke harness).
bool card_visible();

} } } // namespace Slic3r::GUI::DimSumSurprise

#endif // slic3r_GUI_DimSumSurprise_hpp_
