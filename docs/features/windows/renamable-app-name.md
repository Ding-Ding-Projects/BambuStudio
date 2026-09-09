# Renamable app name (display label only)

The user can change the name the application shows for itself. The new name appears in the custom
title bar wordmark, the taskbar/window title, the About dialog caption, the splash screen title,
the Help menu's "About ..." entry, the File menu's "Quit ..." hint, and the captions of the shared
message dialogs that introduce the app ("... error", "... warning", "... info", "... information",
"... - Notice", "... - Ongoing uploads", "... - Newer 3mf version").

It is a **label only**. Nothing that identifies the software to the operating system, to Bambu
services, to the updater, or to a support engineer reads the display name.

## Behavior

- **Where:** Preferences > Appearance > **App name**.
- **Field:** a Material Design 3 text field pre-filled with the effective name. While no custom
  name is stored it shows the shipped name (`Bambu Studio`, from `SLIC3R_APP_FULL_NAME`), which is
  also the field's hint when it is emptied.
- **Commit:** <kbd>Enter</kbd> or leaving the field. The value is sanitized first (control
  characters removed, leading/trailing whitespace trimmed, internal whitespace runs collapsed to one
  space, truncated to 40 code points) and the sanitized text is echoed back into the field.
- **Live update:** the title bar wordmark and the window title change immediately; no restart. Dialog
  captions and menus pick the new name up the next time they are built.
- **Reset to shipped name:** clears the stored value; the accessor falls back to the shipped name and
  the field shows it.
- **Explanation:** the caption behind **What does renaming change?** unfolds a plain statement of
  what the rename does and does not touch (progressive disclosure; the row reads as one field until
  asked).
- **Provenance line:** beneath the field, either `Stored in your settings (app_display_name).
  Shipped name: Bambu Studio` or `Not set. Showing the shipped name, Bambu Studio.` The line names
  the real default value rather than the word "default".

### Language modes

The label, captions, error texts and provenance line go through `_L()` and follow the active language
mode (English, Hong Kong Cantonese, bilingual) and funny levels like every other Preferences string.
The shipped product name itself is a brand string and is **not** translated: it reads `Bambu Studio`
in every mode, and the topbar wordmark no longer routes `"Bambu Studio"` through the catalog.

## Configuration

| Key | Location | Values |
| --- | --- | --- |
| `app_display_name` | `BambuStudio.conf` (AppConfig) | empty or missing = shipped name; otherwise a 1-40 code-point UTF-8 string without control characters |

An invalid stored value (for example one edited by hand into the config file) is ignored: the
accessor falls back to the shipped name and the Preferences field reports the problem inline when
opened. The stored value is never rewritten silently.

Code entry points:

- `Slic3r::GUI::AppDisplayName` (`src/slic3r/GUI/AppDisplayName.hpp/.cpp`): the pure rules
  (`validate`, `sanitize`, `resolve`, `provenance`, `to_stored_value`). No wxWidgets or libslic3r
  dependency, so the rules are unit-tested on their own.
- `GUI_App::app_display_name()`: the single accessor every presentational surface reads. Returns the
  stored name when valid, else `SLIC3R_APP_FULL_NAME`; safe before `app_config` exists.
- `GUI_App::set_app_display_name(candidate)`: sanitizes, validates, persists (`""` when the result
  equals the shipped name), calls `MainFrame::on_app_display_name_changed()` and then broadcasts
  `EVT_APP_DISPLAY_NAME_CHANGED` on the app object for any other live surface.
- `BBLTopbar::SetBrandLabel()`: relabels the wordmark and re-fits the project chip.

## Validation rules

| Input | Result |
| --- | --- |
| empty or whitespace-only | `Enter a name of 1 to 40 characters, or reset to the shipped name.` |
| more than 40 code points | `Too long: N characters. Use 40 or fewer.` |
| line breaks, tabs, DEL, C0/C1 controls (including NEL) | `Remove line breaks and other control characters.` -- refused as typed; nothing is stored until they are removed |
| `  My   Slicer  ` | stored as `My Slicer` |
| the shipped name, in any spacing | stored as `""` (indistinguishable from a reset) |

Lengths are counted in Unicode code points, so 40 CJK characters or 40 emoji are valid.

## What deliberately keeps the real product name

These read `SLIC3R_APP_NAME` / `SLIC3R_APP_FULL_NAME` / `SLIC3R_APP_KEY` and are guarded by
`tests/app_display_name/app_display_name_identity_contract.cmake`:

- the data directory and `BambuStudio.conf` path (`wxApp::SetAppName(SLIC3R_APP_KEY)`,
  `AppConfig::config_path`, the `--datadir` default in `BambuStudio.cpp`);
- log file headers (`LogSink.cpp`) and the CLI fatal-error caption;
- the G-code header, 3MF `Application` metadata, and G-code producer detection;
- the HTTP `User-Agent`, the `X-BBL-Client-Name` cloud header, Helio client-name headers;
- update feed logging (`PresetUpdater.cpp`) and the Squirrel/installer identity (build scripts, `.rc`);
- crash and diagnostic reports (`SendSystemInfoDialog.cpp`, the `header_json["name"]` diagnostic
  header in `GUI_App.cpp`, `SysInfoDialog.cpp` whose text is pasted into bug reports);
- file associations and the registry ProgID description (`prog_desc = L"BambuStudio"`);
- temporary upload file names, exported G-code renderer headers.

Presentational captions that still carry the constant and were **not** switched in this pass (they
are one-off dialog titles built at show time, listed so the gap is a decision rather than an
oversight): `ConfigWizard.cpp`, `CreatePresetsDialog.cpp`, `Plater.cpp` save/restore/delete
captions, `Preferences.cpp` save captions, `Tab.cpp`, `UserPresetsDialog.cpp`, `StatusPanel.cpp`,
`AMSSetting.cpp`, `AMSMaterialsSetting.cpp`, `PhysicalPrinterDialog.cpp`, `PrintHostDialogs.cpp`,
`UpdateDialogs.cpp`, and the `MainFrame` settings-dialog title. They can move to the accessor
individually.

## Failure modes

- **Invalid input:** the field keeps the typed text, the status line turns to the error role and says
  what to change; nothing is stored until the input is valid.
- **Config unreadable / `app_config` missing:** the accessor returns the shipped name.
- **Over-long paste:** the control caps raw input at 200 characters and reports `Too long` inline
  rather than clipping mid-word; committing truncates to 40 code points.
- **Support reports:** a user who renamed the app still produces diagnostics that say `Bambu Studio`,
  so the report names the software rather than the label.

## Security considerations

- Control characters (including C1 controls and NEL) are refused and stripped, so a stored name can
  never inject line breaks into a title bar or a log line that quotes it.
- The display name never reaches network headers, file names, registry keys or exported files; the
  contract test fails the build if an identity-bound file starts referencing the key or the module.
- The value lives in the user's own `BambuStudio.conf`; it is not synced or transmitted.

## Verification

- `tests/app_display_name/app_display_name_tests` (Catch2): bounds, code-point counting, control
  and whitespace handling, sanitization idempotence, fallback, provenance, stored-value folding.
- `tests/app_display_name/app_display_name_identity_contract.cmake`: hand-listed identity files
  contain no `app_display_name` / `AppDisplayName` reference; `GUI_App.cpp` keeps its identity lines
  and the accessor falls back to `SLIC3R_APP_FULL_NAME`; the topbar, MainFrame, About, MsgDialog,
  GUI.cpp and Preferences surfaces read the accessor; the shipped name is not passed through `_L()`.
- Manual: rename in Preferences > Appearance, watch the wordmark and taskbar title change without
  restart; open Help > About and read the caption; press **Reset to shipped name**; confirm
  `%APPDATA%\BambuStudio` and the log file names are unchanged.

## Suggested articles

- [Appearance customization](appearance-customization.md) -- the tab this control lives on.
- [English, Hong Kong Cantonese, and bilingual modes](language-modes.md) -- how the captions are
  localized while the brand string is not.
- [Native Material Design 3 UI](md3-native-ui.md) -- the text field, button and caption roles used.
- [App updates from this fork's releases](app-updates.md) -- the updater identity the rename must
  never touch.
