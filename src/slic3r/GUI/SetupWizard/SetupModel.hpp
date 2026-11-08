#ifndef slic3r_GUI_SetupWizard_SetupModel_hpp_
#define slic3r_GUI_SetupWizard_SetupModel_hpp_

// The practical setup wizard's rules, without wxWidgets: the steps, which are
// required, the saved progress, navigation that skips steps not built yet, and
// what is still missing before setup can finish. Tested on its own by
// tests/setup_model_test.cpp.

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace Slic3r { namespace GUI { namespace Setup {

enum class Step { Language = 0, Look, Printers, Inks, Backups, Extensions, Finish };
constexpr int kStepCount = 7;

struct StepInfo
{
    Step        step;
    const char *id;        // stable key in the saved progress
    const char *title;     // untranslated; the dialog translates it
    bool        required;  // must be satisfied before Finish
    bool        optional;  // may be skipped with Alt+S
    bool        available; // false: designed but not in this build; navigation passes over it
};
const std::array<StepInfo, kStepCount> &steps();
const StepInfo                         &info(int index);
int                                     index_of(Step step);

enum class State { Pending, Done, Skipped };

struct Progress
{
    int                           current = 0;
    std::array<State, kStepCount> states {};
    std::int64_t                  updated   = 0; // seconds since 1970-01-01 UTC; 0 = never saved
    bool                          finished  = false;
    bool                          first_run = false;
};

constexpr const char *kProgressKey = "setup_wizard_progress";
// Set before the window is rebuilt for a language change, so the wizard opens again.
constexpr const char *kReopenKey = "setup_wizard_reopen";

std::string serialize(const Progress &progress);
bool        parse(std::string_view json, Progress &progress);

// What the running app has right now.
struct Facts
{
    int printers  = 0; // visible printer presets that are not the built-in defaults
    int filaments = 0; // visible filament presets that are not the built-in defaults
};

enum class Missing { None, Printer, Filament };
Missing missing_for(Step step, const Facts &facts);
// Required steps whose facts are not satisfied yet.
int  required_left(const Facts &facts);
bool can_finish(const Facts &facts);

// Navigation over available steps only.
int next_index(int from);
int previous_index(int from);
// Where a saved setup continues: its current step, or the first step that is
// neither done nor skipped when the current one is.
int resume_index(const Progress &progress);
int done_count(const Progress &progress);
bool has_saved_work(const Progress &progress);

// Marks a step done and moves on.
Progress complete(Progress progress, int index, std::int64_t now);
// Skips an optional step and moves on; a required step is left unchanged.
Progress skip(Progress progress, int index, std::int64_t now);
Progress go_to(Progress progress, int index, std::int64_t now);
Progress finish(Progress progress, std::int64_t now);
Progress start_over(bool first_run, std::int64_t now);

}}} // namespace Slic3r::GUI::Setup

#endif // slic3r_GUI_SetupWizard_SetupModel_hpp_
