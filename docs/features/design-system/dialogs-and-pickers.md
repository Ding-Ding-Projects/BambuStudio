# Dialogs and pickers

Every dialog the Windows app opens to ask for text, a number or a choice, to say it is busy, or to
pick a colour is a Material dialog. wxWidgets' stock versions of these are system dialogs: their
frame, fields, buttons and colour grid ignore the theme and the language modes.

## What replaced what

| Stock wxWidgets | Material | Where the app asks |
| --- | --- | --- |
| `wxTextEntryDialog` | `TextEntryDialog`: the kit text field, or the kit text area for several lines | Appearance editor, Save as preset; the Workspace panel's prompts; the web view's developer script prompts |
| `wxNumberEntryDialog`, `wxGetNumberFromUser` | `NumberEntryDialog`: the kit spin field | Number of copies; Clone |
| `wxMultiChoiceDialog`, `wxGetSelectedChoices` | `MultiChoiceDialog`: a kit checkbox row per choice | Object list, Add settings on a layer range; compatible presets |
| `wxGetSingleChoiceIndex` | `SingleChoiceDialog`, the kit dialog Windows already used, now on every platform | The language list and other single choices |
| `wxBusyInfo` | `BusyInfo`: a rounded panel in the dialog surface colours | Reload from, Replace from |
| `wxColourDialog` | The Material colour picker with a "Recently used" row | Ink Dispenser slot colour; the ink colour of a preset box and its default picker; the ink picker's More Colors; texture import; bulk ink colour; the settings colour field |

## How

- The three prompts are built on `MsgDialog`, so they have the Material shell, header, footer
  actions, Escape and the close button. They take the stock arguments in the stock order, so a
  call site only changes the class name. The prompt text goes through the same body renderer as
  every message, which puts the Cantonese under the English in bilingual mode. Enter accepts, and
  the field starts with the focus and its text selected.
- `NumberEntryDialog::GetValue()` reads what was typed even when the field has not committed it
  yet, and keeps it inside the range.
- `BusyInfo` paints before its constructor returns, because the caller blocks the event loop next.
  The message is the headline, with its Cantonese under it in bilingual mode, and the detail, such
  as the file being read, follows in the smaller supporting style. A long path wraps at any
  character instead of running off the panel.
- `pick_filament_color()` in `wxExtensions.cpp` opens the Material picker for an ink colour. Ink
  colours are stored as `#RRGGBB`, so the opacity slider is left out and the pick is always opaque.
  The recently used colours, the sixteen custom colours the system dialog kept in the app config,
  are one-click picks under "Recently used", and every accepted pick moves to the front of that
  list. The settings colour field keeps its opacity slider and offers the same list.
- Two stock calls had their words swapped: the clone prompt showed "Clone" beside the field and
  "Number of copies:" as its title, and the compatible-presets chooser used its explanation as the
  title. Both now read the right way round.

## What stays native, and why

- The five message boxes that fire before the Material layer exists or while it is being torn
  down: GUI initialisation failing (twice), fatal and critical errors, and the first language load.
- The system file dialogs for opening, saving and choosing folders, which bring the user's places,
  previews and the Windows shell with them.
- The log window behind Preferences, Developer Tools, Internal developer mode. The Developer Tools
  tab is compiled out of every release, so no released build can open it.

## Verification

- `node --test ui-md3/tests/stock-dialogs.test.mjs` refuses every stock prompt, chooser, busy
  notice and colour dialog outside its allowlist, and checks that the Material versions are built
  from kit fields.
- `node --test ui-md3/tests/dialog-header-includes.test.mjs` fails when a source uses a Material
  dialog with no include path to `MsgDialog.hpp`, the mistake that once broke a build.
- A released build has not been driven through these dialogs yet.
