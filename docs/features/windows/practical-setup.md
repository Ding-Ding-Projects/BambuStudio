# Set up Bambu Studio

**Surface:** Help > Set up Bambu Studio, the command palette (`Ctrl+Shift+F`,
type "Set up"), or the Every feature page → `SetupWizardDialog`
(`src/slic3r/GUI/SetupWizard/SetupWizardDialog.{hpp,cpp}`). The rules (steps,
saved progress, navigation, what is still missing) live in the wx-free
`src/slic3r/GUI/SetupWizard/SetupModel.{hpp,cpp}`.

Design: [design/local-tools-2026-11](../../../design/local-tools-2026-11/README.md),
board Setup.

A new installation opens it after the printer guide. Everyone else finds it in
Help whenever they want to change something.

## The steps

| Step | Kind | What it does |
| --- | --- | --- |
| Language and humour | | English, 廣東話 or both; the English and Cantonese funny levels (1 to 5); emoji in dialogs; a personal wording file |
| Look and motion | | Light, dark or follow Windows; comfortable or compact; system or reduced motion |
| Printers | Required | Choose printers (the printer guide), search your network, import a printer profile |
| Inks and filaments | Required | Sync AMS, pick filaments (the filament guide), and see what is on the plate |
| Backups | Optional | Auto-backup of the open project (off, 2 or 10 minutes) and a config backup |
| Extensions | Optional | The built-in extensions, each with **Review and allow** (the consent prompt) or **Turn off**; see [Extensions](extensions.md) |
| Finish | | What setup did; Open Prepare, or take the feature tour |

Every action is real and applies at once, through the same settings
Preferences uses. Each one adds a line to **Result of each action**, so you can
see what happened, including refusals: a wording file that was not applied
says why, and an import that brought no printer says so.

- **Language** reloads the window, as it does in Preferences. Setup marks the
  step done, saves its place and opens again on the next step in the new
  language.
- **School mode** hides the humour, emoji and wording settings here too.
- **Search my network** lists the printers that answered on your network,
  each with **Connect**, which saves setup and opens the Device tab on that
  printer. Connecting needs the printer's access code, which you enter there.
- **Follow Windows** sets light or dark from Windows' own choice for apps,
  and switches the whole app within three seconds whenever Windows switches,
  also while Bambu Studio is open. Choosing Light or Dark, here or in
  Preferences, stops following. Where Windows does not say (another system),
  the look stays as it is.
- **Choose printers** lists the printers the printer guide added, with
  **Undo**: after you agree, the guide's printer and filament choices go back
  to how they were before it opened and the presets it added are hidden
  again. Unsaved changes to the printer in use are saved or dropped first, as
  the guide asks. Nothing you saved is deleted, a printer one of your presets
  still uses stays, and the result says how many were removed. Undo acts
  once.
- An import that brought no printer offers **Choose another file**.
- A bar under the step counter shows how many steps are done or skipped.
- **Back up** opens Config profiles & backup: Snapshot now keeps a complete
  local copy of your settings, and Export moves everything, secrets included,
  to another PC behind its own confirmation.

## Required steps

Setup can finish once you have at least one printer and one filament preset.
**Next** never greys out: on a required step that is not satisfied, it explains
what is missing and becomes **Next anyway**, so you can go on and come back. The
header counts the required steps left, and the Finish step lists what is still
needed.

## Saving and continuing

Every change saves the progress at once (`setup_wizard_progress` in
`BambuStudio.conf`): the current step, which steps are done or skipped, and the
time. Close it any time (Esc saves and closes) and open it again from Help; it
says where it stopped, for example "Welcome back. Setup stopped at step 3 of 7
on 2026-11-07 at 23:58:12; 2 finished steps are kept", with **Continue** and
**Start over**.

## Keyboard

| Keys | Action |
| --- | --- |
| `Alt+N` | Next |
| `Alt+B` | Back |
| `Alt+S` | Skip an optional step |
| `Esc` | Save and close |

The steps on the left can also be clicked.

## Stored values

| AppConfig key | Value |
| --- | --- |
| `setup_wizard_progress` | JSON: current step, step states, time, finished, first run |
| `setup_wizard_reopen` | `1` while the window is rebuilt for a language change |
| `theme_follow_windows` | `1` while Windows decides light or dark; `dark_color_mode` still holds the look in effect |

The steps themselves write the settings Preferences already uses: `language`,
`funny_level_en`, `funny_level_yue`, `dialog_emojis`, `dark_color_mode`,
`ui_density`, `motion_preference`, `backup_switch` and `backup_interval`.

## Checks

`tests/setup_model.test.mjs` compiles `tests/setup_model_test.cpp`: the steps,
required facts, completing and skipping (a required step can never be
skipped), resuming, and saving, including a hand-edited record that tries to
skip a required step. A negative build lets a required step be skipped and must
fail.

`tests/theme_follow.test.mjs` compiles `tests/theme_follow_test.cpp`: what
Light, Dark and Follow Windows store, Windows' own setting, and when following
switches the look. A negative build follows Windows while following is off and
must fail. It also checks the sources: the look is set before the first
window, the watcher starts after startup and stops at exit and changes the
look only when the rules say so, Preferences and setup store both values
through the shared rules, and Undo asks first, handles unsaved changes, puts
the guide's choices back and acts once; each check must fail on a broken
copy.

## Not done yet

Everything on the board is built. Following Windows reads Windows' setting for
apps; on other systems the choice keeps the look in effect.
