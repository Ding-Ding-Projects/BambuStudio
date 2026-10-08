# Start-from presets for blank editors

**Surfaces:** the **Start from** picker
(`src/slic3r/GUI/Presets/StartFromPicker.{hpp,cpp}`), the preset model
(`src/slic3r/GUI/Presets/BlankEditorPresets.{hpp,cpp}`) and the shipped
defaults table (`src/libslic3r/PresentationDefaults.hpp`).

An editor that would otherwise open to nothing first offers presets, so a new
item starts from something real instead of an empty form or a made-up value.

## Where a preset's values come from

A preset can only take its values from one of these sources:

- **Shipped defaults.** The value the app ships with, read from the shipped
  defaults table. `AppConfig::set_defaults()` (which fills a missing setting)
  reads the same table, and a source contract fails if **Preferences ▸
  Appearance ▸ Reset appearance to defaults** ever restores a different value,
  so a preset and a reset always agree on what the shipped value is.
  A setting without one fixed shipped value is left out and the reason is
  shown: the interface language follows your system language the first time
  the app starts.
- **Your saved settings.** Your own saved values. When nothing usable is saved
  for a setting, the preset starts that setting from its shipped default and
  marks it "(shipped default, nothing saved)" instead of copying an invalid
  value.
- **Start empty.** Creates the item and sets nothing; you choose every value.
- **Templates the app ships.** For example the shipped appearance presets.

The preset model has no way to accept any other starting value, and its tests
check that every value a preset sets equals either the shipped default or your
saved value.

## What the picker shows

- The presets in a list, filtered by the shared search field. Plain text search
  is the default; the regex builder is available from the field. The search
  also matches what each preset sets, so searching "Theme" finds every preset
  that sets the theme. When nothing matches, the picker says so and how to see
  every preset again.
- For the highlighted preset, before anything is applied: what it creates,
  every setting it sets with the value it writes, and every setting it leaves
  out with the reason.
- **Start from this preset** applies the highlighted preset. **Cancel** closes
  the picker without creating anything.

## Appearance presets

The appearance editor's **Presets** page (Ctrl+Shift+E on any styled element)
and the command palette's **Apply appearance preset** rows state what a preset
sets before you apply it:

- Selecting a preset in the list shows that applying it creates nothing new,
  makes it the active preset and keeps your own element overrides on top, then
  lists every property it sets, for every element or for a named element, with
  its value. **Material default** sets nothing, so the Material tokens apply.
- After **Apply** (or a double-click), the same place begins with "Applied
  "Large text". It sets Font size (pt) (every element): 15.5; Line height
  (every element): 1.4".
- Each **Apply appearance preset** row in the command palette (Ctrl+Shift+F)
  describes the same settings in its second line.

## Applying a preset is recorded in local history

Applying a preset is an ordinary change you can undo from **File ▸ Version
history**:

- The appearance (element overrides, saved presets and the active preset) has
  its own **Appearance** category in Version history, stored beside the
  preferences history and never synced.
- Applying, saving or deleting an appearance preset, resetting one element or
  all of them, importing a theme and restoring an earlier version are each
  recorded at once under their own name, for example "Apply appearance preset:
  Large text". Other edits are grouped into an "Appearance change" after two
  seconds without further edits, and any such edit waiting to be recorded is
  recorded first, so the state before a preset keeps a version of its own.
- The appearance at startup is recorded too, so even the first change of a
  session can be undone.
- Restoring an Appearance version replaces the live appearance and is itself
  recorded. **Compare selected** lists every property that differs.

## Keyboard and accessibility

- The preset list has focus when the picker opens. The arrow keys move the
  highlight, and the statement follows it.
- **Enter** in the list, or a double-click, starts from the highlighted preset.
  **Escape** cancels.
- The list is named "Presets to start from" and the statement is named "What
  the highlighted preset creates and sets" for screen readers. On the
  appearance Presets page the statement is named "What the selected appearance
  preset sets".

## Verification

- `tests/blank_editor_presets` (Catch2, wxWidgets-free): every presentation
  preference has a shipped value or a stated reason; every shipped value is
  accepted by its setting; the standard presets come in a fixed order; a preset
  never carries a value from anywhere but the shipped defaults or your saved
  values; rejected saved values fall back honestly; an appearance preset is
  stated property by property; history messages name the preset applied.
- `ui-md3/tests/blank-editor-presets.test.mjs`: the reset and the shared table
  agree, the picker keeps its search, statement and keyboard paths, every
  appearance preset path states and records what it does, and Version history
  lists, compares and restores Appearance versions.
- Still to observe in a built Windows application: the picker and Presets page
  layout at every density and language mode, screen reader announcements, and
  an Appearance version being listed and restored.
