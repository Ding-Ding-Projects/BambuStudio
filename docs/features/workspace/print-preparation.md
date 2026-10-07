# Print preparation and workflow navigation

The native workspace navigation presents Prepare, Preview, Print, and Monitor
in that visual order. Existing page IDs remain unchanged: Print is appended as
a real page, while the navigation control projects the workflow order. Home,
Project, Calibration, Filament, and Multi-device retain their existing routes;
destinations that do not fit remain available through **More workspaces**.

## Review without starting work

Opening Print does not start slicing, submit a job, cancel a pending operation,
or select Preview. Preview retains its existing behavior, including the ability
to initiate slicing when explicitly selected. Print reads the current plate,
printer preset, available slicing estimates, and the existing output readiness.
The printer preset is identified as a preset, not presented as a connected
destination.

The page contains plate readiness, destination and mapping guidance, deliberate
continuation, and a print summary. The summary describes the selected plate;
the selected output action determines whether one or all plates are included.
Missing estimates are labelled unavailable. Empty, unsliced, slicing, ready,
and unavailable states have explicit text. Existing disabled reasons remain
visible beside the output action and in its tooltip.

## Explicit actions and confirmation

**Output options** opens the existing output-mode menu at the initiating
control. The primary button displays the selected action's existing label.
Print, Send, export, multiple-printer, and other available modes keep their
original validation and confirmation paths. Destination selection and material
mapping remain in those existing paths; this page does not invent a connected
printer, mapping result, or readiness promise.

Slice, Slice and print, and Slice and send invoke the same handlers as Prepare.
Readiness is refreshed synchronously before activation, so an enabled control
from an earlier plate cannot authorize stale output. Existing version, plate,
material, pending-continuation, and printer checks remain authoritative. An
animation never submits an action or delays its callback. Monitor preserves its
existing network checks, telemetry, camera, and printer controls.

## Appearance, language, and data

Cards use shared Material Design roles, typography, density spacing, and the
Device accent. The two-column review stacks when the available width or
measured control minimums require it. Content can scroll vertically; controls
wrap and labels are measured again after resizing or scaling.

Copy uses the existing `_L` translation route. English, Hong Kong Cantonese,
and bilingual presentation remain governed by the application language mode;
the existing tone and local wording controls are unchanged. The Print source is
registered in `bbl/i18n/list.txt` for normal extraction. Cantonese additions are
marked as agent-drafted pending fluent human review.

Print stores no new preference, printer credential, command history, or project
data. Observation makes no new network request. Only a deliberate action enters
its established operation path, which retains that path's access and
confirmation requirements.

## Verification and limitations

`node --test ui-md3/tests/workflow-print-state.test.mjs` compiles the production
state model and checks explicit action dispatch, busy and stale readiness,
recovery navigation, and side-effect-free observation. Removing its production
readiness check deliberately fails the same behavioral assertions.

`python ui-md3/tests/test_workflow_print_localization.py` checks the extraction
registration, actual GNU gettext output, English and Cantonese catalog entries,
compiled translation lookup, placeholder preservation, and the paired article.
Its negative cases remove registration or required translations and verify that
those omissions are detected. The normal catalog validator additionally checks
coverage metadata and deterministic compilation.

These checks establish source and catalog behavior. Native target compilation,
rendered text fit, accessibility interaction, language/scale/theme combinations,
and real printer outcomes remain unverified. No runtime screenshot or physical
printing evidence is claimed by this feature change.

## Catalog maintenance evidence

The combined workflow/navigation source extraction added 143 POT records and
removed 26 old POT records. Of the 114 extracted messages absent from the prior
English catalog, this unit authored only the 30 workflow/navigation messages;
the other 84 remain deferred. All prior English and Cantonese PO entries were
preserved, including the 185 English-only records that are no longer extracted.
The POT remains the canonical current-source extraction, not a hand-edited union
with old keys.

The 26 removed POT keys were checked against the current source tree. Twenty-three
have no literal source occurrence. `Deleting…` remains only in the embedded
DeviceWeb locale JSON files, which use their separate catalog route. Two old
partial sentences remain as prefixes in registered native sources:
`GUI_ObjectList.cpp` and `UserPresetsDialog.cpp`; GNU gettext now extracts their
complete concatenated C++ strings. Neither is an omitted source registration.
The exact removed keys are listed below as message identifiers, not new copy.

<details><summary>26 removed extraction identifiers</summary>

- `"Add an object to the build plate, select a material and printer that Helio supports, then slice."`
- `"Browse complete project snapshots saved automatically in a private local Git repository."`
- `"By default, Liveview will pause after 15 minutes of inactivity on the computer. Check this box to disable this feature during printing."`
- `"Click the Optimize/Enhance button to start your first optimization."`
- `"Could not load version history."`
- `"Deleting…"`
- `"Each object is removed from its plate together with all of its parts and instances. "`
- `"First Guide"`
- `"Great! Now click the Helio button to start optimization."`
- `"Keep liveview when printing."`
- `"LAN Connection Failed (Failed to start liveview)"`
- `"Liveview Retry"`
- `"Navigation rail"`
- `"Permanently delete %d notification entries from the history. This cannot be undone; the export button above keeps a copy first."`
- `"Permanently remove the selected entries from the history"`
- `"Released %s"`
- `"Selected commit: "`
- `"Slide to delete these entries permanently"`
- `"Start (UTC)"`
- `"Supported printers and materials"`
- `"The new profile folder could not be created."`
- `"The preset files are removed from the user preset folder and cannot be recovered "`
- `"This is your first time printing tpu filaments with the dual extruder machine.\nWould you like to watch a quick tutorial video?"`
- `"This is your first time slicing with the dual extruder machine.\nWould you like to watch a quick tutorial video?"`
- `"Version history is unavailable because its local repository could not be initialized."`
- `"Version history is unavailable for this project."`

</details>

The strict full-catalog source-membership check still reports three pre-existing
Cantonese-only keys: `Interface motion`, `Reduce motion`, and
`Reduce motion settles supported transitions immediately. System follows your operating system preference.`
The existing CMake catalog command uses `--allow-unreferenced` for that known
extraction lag. With that established option plus `--require-complete`, the
catalog validates 8,021 translated messages. The focused workflow test enforces
strict English/POT/Cantonese membership for all 30 new keys without an exception.
Coverage metadata was refreshed by the established authoring script, preserving
its human-review metadata; 30 entries were added as agent drafts.

The missing extraction tool was obtained through `scripts/i18n/update_catalogs.py`:
GNU gettext package `gettext1.0-iconv1.19-shared-64.zip`, release `v1.0-v1.19`,
from the canonical `mlocati/gettext-iconv-windows` release. Its verified SHA-256
is `c2f195fc4ed3df4070fb08ff88c2f724afd1eed4e28f859efae1000bb7ba1152`.
The extractor reports 11 existing warnings; the first is the empty message ID
at `src/slic3r/GUI/AMSMaterialsSetting.cpp:179`. Those warnings and the deferred
catalog gaps are not claimed fixed by this workflow correction.
