# Bambu Studio handoff

## MCP automation implementation, 2026-10-02

Task: [issue #53](https://github.com/Ding-Ding-Projects/BambuStudio/issues/53), with
[rolling progress #54](https://github.com/Ding-Ding-Projects/BambuStudio/discussions/54).
Implementation is on `feature/mcp-integration`. It is not yet integrated into
`main`. Verification below records exact source identities rather than treating
the branch's moving tip as tested.

The native bridge is opt-in and exposes current-user named-pipe project, model,
preset, settings, slicing, export, printer and job operations. A self-contained
companion under `automation/` provides 20 typed MCP tools through stdio and
authenticated Streamable HTTP, plus the same JSON CLI service. Native slicing
uses generation, plate and revision identities. Printer starts use the native
task manager with a durable intent journal and an immutable staging-file lease.
Current print-start support is restricted to a verified single nozzle and one
external-spool filament; AMS and dual-nozzle mappings fail explicitly.

Builds, tests, packaging, installation and application execution for this task
run only on GitHub-hosted Windows runners. No local product execution occurred.
The first focused run, `37038981051`, failed compilation on nullable HTTP Host
handling. The next run, `37040259653`, compiled and passed 22 cases but exposed
an incorrect child-process startup path in both transport checks. Both defects
are repaired. At `65dc4577fb29f2d00fd525a1de11e370784c5b3e`, the managed job in
[`37045639569`](https://github.com/Ding-Ding-Projects/BambuStudio/actions/runs/37045639569)
passed all 24 checks: 22 in `BoundaryTests.cs` and 2 in `TransportTests.cs`.
Self-contained publication and documentation parsing also passed. Native compilation,
payload checks, Squirrel creation, and package-contract validation passed in job
`110966508477`. At the one-hour observation boundary, release job `110986759894`
was still validating release assets and metadata (last observation 2026-10-02T19:10:35Z).
The preceding candidate passed native
compilation and Squirrel packaging, but reached its one-hour observation limit
with release validation still running. The two superseded native runs were
canceled because their older release dependency did not include the red MCP
verdict; current release publication requires both native and managed jobs.

A dedicated automation recipient and strict schema-v2 evidence reader preserve
historical GUI recipients. Only the public PEM is tracked. Protected review
material remains local. Administrative cryptographic initialization is distinct
from product execution; no application or product test ran locally.

Remaining work: terminal hosted release result; packaged runtime verification
against the exact released SHA; hosted evidence parser and strict-reader use;
genuine capture review; documentation completion; default-branch
integration and remote proof. No physical printer operation has been verified.
The agreed one-hour observation limit was reached. A structured request to extend
this run to two hours has no answer yet. The hosted run was not canceled. Continue
only after an authorized extension or a later terminal-state handoff; do not treat
the pending release as successful. All task work remains retained and no cleanup
has run. The wiki is published at `f0dc140038ae19d0487f74a4b4ba0a64b265485b`.
See [automation documentation](docs/features/automation/mcp-and-cli.md) for the
transport, security, setup and operation contract.

## Earlier handoff, retained as historical context

The following record predates this MCP task. Its timestamps, releases and
verification claims describe that earlier work and are not current MCP evidence.

## Current state

At 2026-09-28 08:40 UTC, `main` is at `e4fc4be11c0e87a9600f1257b92e4bc6c64acac7`, confirmed with
`git ls-remote`. The production build is green: Build BambuStudio passed in
[run 36383765054](https://github.com/Ding-Ding-Projects/BambuStudio/actions/runs/36383765054) at
`fda9ba2840c35f3aef500ab0810db9463fe0c0bf`, and the same run published
[`md3-v135`](https://github.com/Ding-Ding-Projects/BambuStudio/releases/tag/md3-v135) with an unsigned
Squirrel.Windows `Setup.exe` (708,598,272 bytes), the full `.nupkg`, `RELEASES` and `Setup.exe.sha256`.
The `C1041` PDB failure was removed by `6bb896464` (no `/Zi` when `SLIC3R_MSVC_PDB` is off) and the
`CBaseException` link failure by `ee8ab3eb1`. The startup exit `0x80070057`, both file-opening routes and the
DPI matrix have not been re-checked against `md3-v135`; an isolated install of that package is not yet
verified.

A direct launch of the `md3-v143` payload exits -1 after 0.2 s: `BambuStudio.dll` fails to load
(`LoadLibrary` error 126, recorded in `%TEMP%\bbs-launcher-trace.log`) because the Ninja build skipped the
dependency DLL copy, so the payload held `BambuStudio.dll` alone. With the OpenCascade, FFmpeg, GMP and MPFR
DLLs added beside it, the same payload starts on a hidden desktop. The build now copies them for Ninja too,
ships the Visual C++ runtime beside the app, and packaging refuses a payload that misses a DLL
(`docs/features/releases/windows-release-supply-chain.md`, "Payload DLLs"). Whether the installed
`0x80070057` exit shares this cause is re-checked on the first release built from that change.

The integrated source includes the fresh reapplication on official Bambu Studio `v02.08.04.57` source
`f977235e6d736c4c0b650520ac5a5b72cbfe9244`, plus bounded native model observation (`257c700e3`) and exact
3MF model-state verification (`68d1be871`). The pinned source for the earlier fork features is
`c5df6199e1a83b1c94be12e999c0b322fded8730`; that is a source input, not the new package's verification
identity.

## Language modes and Material Design 3 (issues #43 and #45)

Scope: every element on the Material Design 3 kit, and every element in English, Hong Kong Cantonese and
bilingual mode. Every change is pushed straight to `main`; the task branch `claude/lang-gui-elements-9cc0be` is
kept equal to `main` and exists only as the checkout of the session that started the work. The README's
"New features in this line" table lists every feature this work added (automatic updates, the moving
release feed, the splash release date, bilingual coverage, the Cantonese catalog and content,
Cantonese line breaking, the clipping fixes and the verification tools) with its status and what is
still to verify.

- Landed and compiled (release `md3-v135`): kit conversion (36 of 36 contracts, `de1f25259`); complete
  Cantonese catalogue with a drafted-entry audit (`db39e3bb1`, `50e82299f`); about 200 literals routed through
  the translation layer (`e3686f59c`); the bilingual registry and fit-aware decorator (`97133f71f`,
  `9b6baf096`); local web language routing (`08ae598e4`); Cantonese docs and changelog with a matching command
  palette and changelog viewer (`e604fefc9`, `fda9ba284`).
- Checks: `scripts/i18n/Test-LanguageModes.ps1` (catalogues, audit, web keys, docs and changelog) and
  `scripts/ci/Test-InkTerminology.ps1` run locally before a push; the workflow builds and publishes only
  (`e4fc4be11`). `language_mode_tests` is not built by the workflow.
- The in-app changelog covers releases up to `md3-v154` (160 releases, 1,307 entries) with Cantonese text for
  every commit it lists; `check_translated_content.py` reports 0 problems.
- Also landed and compiled: bilingual self-drawn widgets (`adcf23a2f`), the bilingual 3D canvas with a Traditional
  Chinese CJK font (`7bacc40ce`, release `md3-v138`), the last literals (`a479a3020`), the web surfaces
  (`68f42a887`) and the extracted messages (`15a864edd`, catalogue 7,644 entries). The layout probe's
  `language-audit` command (`adbd538e9`) lists native labels still English-only in bilingual mode. Release
  `md3-v139` (target `15a864edd`) is the first package that carries every one of these lanes.
- On `main`, compile pending when written: message dialog actions keep the full width of their labels
  (`9615c9418`, clipping inventory CJ-014, fixed-unverified), and the splash screen says when the running
  version was released, in all three modes (`docs/features/windows/splash-release-date.md`, contract test
  `ui-md3/tests/splash-release-date.test.mjs`). Cantonese docs that rendered "release" and "splash" literally
  now use 發佈 and 啟動畫面.
- On `main`, compile pending when written (2026-09-29): installed copies update themselves through Squirrel
  (issue #46; `44e3940d7`, then `b2370c9a7` so a failed update falls back to the download dialog on every
  check); the layout probe compares only child windows with their parent's client area (`f2110b6cc`); a field
  that draws a unit after its number keeps room for the number (`a849963bf`, CJ-019); section headers and list
  and table column titles show both languages in bilingual mode (`87ec5c44c`); Keyboard Shortcuts labels sit on the dialog
  surface (`466c24da9`). The sweep of the remaining dialogs on `md3-v143` (Temperature, Flow rate, Pressure
  advance, Retraction test, the three Export items, Setup Wizard, the gear menu, AI ink scanner) found CJ-019
  and otherwise only probe false positives in English and Cantonese; the Export items and the gear menu open
  popups that the capture route paints black, so they have probe data and no image. Correction: the bilingual
  pass had no probe data at all (its dump path was 272 characters, past the 260 limit) and the driver counted
  the missing dumps as zero findings; the driver now dumps through a short folder and reports a missing dump.
- The update feed was stuck: a main build became the latest release only if `main` had not moved while
  it built, so every build after `md3-v143` was published as superseded. From `904ccc37a` a main build
  becomes latest when its commit is newer than the latest release's. The ready notice is now a banner
  that stays until the user acts, with Restart to install update, Release notes and a notice that
  updates are not code-signed, and an installed copy re-checks every six hours (issue #46). Placeholder
  hints are bilingual too (`f0fb0c297`). Notification links are bilingual when the pair fits a line of the
  notification (on `main` after this note; see ROADMAP).
- Verified from released packages with nothing added (2026-09-29): `md3-v148` (`3a935ed9f`) starts and
  every import of its 40 files resolves; CJ-014 (`md3-v148`) and CJ-015, CJ-017, CJ-018 (`md3-v150`,
  `74641b8c0`) have before and after captures. The `md3-v150` captures also showed two regressions of this
  task's own: every kit button English only in bilingual mode (from `efaa98db2`, which pinned any control
  with a minimum width) and CJ-016 still cut (`d27eadfdb` counted on the dialog growing); both are fixed in
  `00b14ca67`, and CJ-019's fix was corrected in `f3aab6af1` (a centred field drew its unit under the entry).
  The bilingual sweep's missing-dump reporting (`e5a7e12ad`) and the probe's visibility rules (`2906c2820`)
  were fixed on the way; the earlier bilingual sweep had no probe data (correction on Discussion #44).
- `md3-v151` (`ca2b6e101`, 2026-09-29) is the first release published as latest by the newer-than-latest
  rule. Its package matches `RELEASES` (SHA-1 `6c6031131b79e06836825c9d4442f18fda5d7239`), every import of
  its 40 files resolves, and its Preferences probe dumps list the "Update automatically" row in all three
  modes, so the automatic updater ships in it. Its bilingual Preferences showed a new defect, CJ-021: every
  page grew a sideways scrollbar and the controls at the end of each row moved out of sight. The decorator
  let a label count on the dialog growing inside a page that scrolls, paired the labels Preferences wraps to
  320 DIP on one unwrapped line, and counted a stretching label's spare room twice. Fixed in `2e80091ef`,
  which also sends back the paired labels of a page that still needs more width than it shows
  (fixed-unverified until a release carries it). Preferences searched for "Update automatically"
  shows the new row, switched on, in all three modes (`scripts/md3/capture-preferences-search.py`,
  since the row is below the fold and the hidden desktop cannot scroll). The Cantonese capture showed
  a wrapped description starting its second line with "。": the label wrapper counted only characters
  above U+4E00 as CJK. `c7309b889` makes it follow the CJK punctuation rules and never split an emoji.
  The `md3-v151` Cantonese probe dumps also showed CJ-022: a catalog answers the empty message with its
  header, and the Cantonese fallback to the English catalog did not refuse an empty string, so every
  settings field without a unit carried "Project-Id-Version: ..." as its unit (two spin fields drew their
  number 0 px wide) and every `_L("")` in the source showed the header, such as the description of
  Preferences > User "Auto-fill previously logged-in accounts.". `c591f1b39` skips an empty message in
  every direct catalog lookup (contract test `ui-md3/tests/empty-message-lookup.test.mjs`, and a case in
  the hand-built `language_mode_tests`).
- The same `md3-v151` Prepare captures showed CJ-023 and CJ-024: the section strip (Ink / Process /
  Objects) docks left of the sidebar body at 128 DIP inside the same pane, and no pane width counted
  it, so the full process-settings tree got 334 of its roughly 417 px and every value field ran past
  the edge; the Process title kept a 56 DIP minimum and read "打印設…" in Cantonese. `11cf45423`
  adds the strip to every pane width and lets the title be as wide as its text.
- `md3-v153` (`1757d880f`, 2026-09-29) is verified from its package (SHA-1
  `3a97afd5d4e083e92a40c72d2c62310e66dcd8c2`, imports resolve): the splash reads its release date in all
  three modes, captured through the splash's own bitmap hook, and CJ-020 is verified. A first Smart
  home capture raced the repaint of an auto-wrapping label and showed its paragraph English only; a
  second run showed both languages (noted in the screenshots README).
- `md3-v154` (`00b14ca67`, 2026-09-29) is verified from its package (SHA-1
  `8ac42b12ec13fd2e814690414882e9926cdd0a3f`, imports resolve): CJ-016 and CJ-019 are verified, and kit
  buttons read "OK · 確定" and "Close · 關閉" again in bilingual mode. The same sweep found two things
  on Temperature calibration, both fixed in `32a36b134`: its units read "Â°C", because twelve narrow
  literals with a degree sign, "mm³" or a separator went into wxString through the Windows code page
  (`ui-md3/tests/wx-narrow-literals.test.mjs` now scans for them), and its bilingual section header
  read "SETTINGS · 設", because the header measured itself with GDI and one whole-string extent but
  paints with GDI+ glyph by glyph (CJ-025).
- `md3-v155` (`c7309b889`, 2026-09-29, package SHA-1 `00332a4f71ff225d1efc073bb3fb2b9243778bc1`, matching
  its `RELEASES`) did not verify CJ-021: bilingual Preferences > General still scrolls sideways, its rows
  681 px wide in a 560 px page. The fit rules of `2e80091ef` never ran on the funny-level and emoji rows,
  whose helpers built their own "English · 廣東話" line; `dd95aace8` hands those pairs to the bilingual
  registry (`ui-md3/tests/preferences-funny-rows-bilingual.test.mjs`). The same commit fixes the layout
  probe's row judge, which counted every sizer border twice, so every `oversubscribed` figure taken before
  it is inflated (`ui-md3/tests/layout-probe-row-judge.test.mjs`); the bottom button row it flagged in
  bilingual Preferences fits exactly (783 of 783 px). The same captures showed CJ-026: on every
  Preferences page opened after the first (User, 3D, Other), each stacked description's Cantonese line
  is drawn under the next row's title, because a page keeps its size when its dialog lays out and its
  own sizer never ran; `a07353987` lays out the page around every label the decorator changes. Seen
  once, to re-check on the next release: the bilingual Other capture shows the search field without its
  hint, while the other four pages show "Search settings · 搜尋設定".
- `md3-v156` (published 08:14 UTC, not latest) targets `2e80091ef`, an ancestor of `md3-v155`'s target,
  and carries the same Squirrel package version, `2.8.4155`, with different bytes. The packaging step
  took the number as "highest md3 release plus one" long before the release job assigns the tag, so
  builds queued behind one another share it, and Squirrel installs nothing when the feed's version
  equals the installed one: the later of two such latest releases would never arrive by itself.
  Hosted builds now take the number from the workflow run number, which only grows in push order
  (`.github/workflows/build_bambu.yml`, contract in `ui-md3/tests/md3-conversion-contracts.test.mjs`);
  the one-click local build keeps its own rule. Builds that were already queued still packaged under
  the old rule.
- `md3-v157` (`11cf45423`, 2026-09-29, latest, package `2.8.4156`, SHA-1
  `1d14417aa7037d47d7b7e5c1b2de75172dde4dab` matching its `RELEASES`, every import resolves, splash painted
  in all three modes) verifies CJ-022, CJ-023 and CJ-024 and the Cantonese line break before "。". The
  Cantonese Prepare dump shows the "10" and "1" spin boxes 89 px wide (0 px on `md3-v151`) and no label
  carrying the catalog header (8 on `md3-v151`). The bilingual "Update automatically" description shows
  its Cantonese below the English, and the bilingual Other page shows its search hint, so the empty hint
  of one `md3-v155` capture did not reproduce. CJ-026 is still visible on `md3-v157`; its fix came later.
- `md3-v158` (`32a36b134`, package `2.8.4158`, SHA-1 `8613eadca952103e32a2cb2bfbd348b02b18113e` matching
  its `RELEASES`, every import resolves, splash painted in all three modes) verifies CJ-025 and the unit
  fix: Temperature calibration reads "SETTINGS · 設定" and "°C", Max flowrate "mm³/s", and What's new
  separates its dates with " · ". The same sweep found CJ-027, the What's new date hint cut in all three
  modes; the probe does not measure placeholder hints, so the sweep reported no finding. `1af648025`
  sizes each date field to its hint and lets the date row wrap.
- `md3-v159` (`d8fe5047a`, published 09:50 UTC as latest) carries package `2.8.4158`, the same version as
  `md3-v158` before it: an installed `md3-v158` does not take `md3-v159` by itself. Both were packaged
  under the old "highest release plus one" rule; `363181692` (run #607 on) numbers packages by run.
  Its package verifies (SHA-1 `83b29ae59437103fc7599578eb048838055aad01`, imports resolve), and its What's
  new lists 160 versions and 1307 changes, newest v154, every entry in Cantonese in Cantonese mode.
- `md3-v160` (`dd95aace8`, published 10:20 UTC as latest, package `2.8.4159`, SHA-1
  `57a99f72f9cb99efd1ff7e299be5c3b581838678` matching its `RELEASES`, every import resolves, splash painted in all
  three modes) verifies CJ-021: bilingual Preferences > General has no sideways scrollbar and its widest row
  ends at 556 px in the 560 px page. The provenance line under each Funny level slider still shows English
  with its Cantonese in the tooltip; `4d5bfc93f` wraps it. CJ-026 is still visible here; its fix came later.
- `md3-v161` (`a0e408559`, published 10:51 UTC as latest, package `2.8.4608`, SHA-1
  `a2c80a7ef125f3167baa06e891f124832e2f8f58` matching its `RELEASES`, every import resolves, splash painted in all
  three modes) is the first run-numbered package: `2.8.4608` after `md3-v160`'s `2.8.4159`, so installed
  copies move forward again. It verifies CJ-026 (stacked descriptions no longer drawn under the next row on the
  bilingual 3D and Other pages) and keeps "What does this change? · 呢個會改變啲乜？" paired. It also carries the
  upstream `v02.08.04.61` merge, which starts and draws every captured page in all three modes.
- `md3-v162` (`9b1337ccc`, published 11:26 UTC as latest, package `2.8.4611`, SHA-1
  `b62ab87ee23cb36518731316e8a560d2ced0a3df` matching its `RELEASES`, every import resolves, splash painted in all
  three modes) carries every fix of this line up to `9b1337ccc`. It verifies CJ-027 (What's new shows its whole
  date hint; the chips take a line of their own), the Funny level lines stacked in bilingual mode ("Not stored
  yet; ..." over "未儲存；..."), hint measurement in the layout dumps (354 hints in the bilingual sweep, none cut),
  and rising package versions (`2.8.4159`, `2.8.4608`, `2.8.4611` for three latest releases in a row). Language
  audit: Prepare 8 labels paired, 9 with the Cantonese in the tooltip, 0 English-only; Preferences > General 58
  paired, 15 in the tooltip, 0 English-only. Full dialog sweep: bilingual 20 dialogs, no finding; English 20 and
  Cantonese 19 dialogs, the same two with findings: Model Creator (CJ-028, its form outgrew the fixed 720 x 780
  dialog; fixed in `13dd18236` by a scrolling form) and Smart home's Close button (CJ-029, 59 px against a 70 px
  minimum; open, the moment its minimum grows is not established). The sweep harness did not open Show Tip of the
  Day in English and Cantonese, nor About in Cantonese.
- CJ-029 cause found in the source (2026-09-29): a kit Button that reaches its first paint unstyled adopts the
  Outlined style there (18 DIP padding each side instead of the 10 px it was measured with, medium label font), so
  its minimum grows after the parent's sizer has placed it, and nothing laid the Smart home footer out again outside
  bilingual mode. `e5faf503d` makes the first paint queue one layout of the parent when the style changed the minimum
  (`ui-md3/tests/button-first-paint-layout.test.mjs`). Waiting for a release to verify Smart home in English and
  Cantonese.
- `md3-v165` verified (2026-09-29): target `e5faf503d`, package `2.8.4614`, SHA-1 `bb7984ac...`, every import resolved;
  splash, pages and searches in three modes; a full sweep of 20 dialogs and the six top menus in each mode with no
  finding; the language audit found no English-only label on Prepare or Preferences > General. CJ-029 verified (Close
  70 px in English, 64 px in Cantonese). CJ-028 verified for the form and key row; its four footer buttons still capture
  as blank boxes at full size, now CJ-030 (open, cause not found; the same boxes are in the `md3-v162` captures).
  Evidence: `docs/screenshots/md3-everything/*--md3-v165.png` and the key-row pair, cropped to leave out folder paths.
- Canvas capture (2026-09-29): on the real graphics driver PrintWindow gets a blank 3D canvas, so no capture of an
  unmodified release package had shown a gizmo panel, a notification or the Daily Tips panel (the earlier canvas
  captures staged Mesa's software OpenGL beside a local build). `a40082726` adds the probe command
  `canvas-png <path>`: the canvas reads its frame back after the ImGui pass and before the buffer swap and writes the
  PNG through a `.part` file. `scripts/md3/sweep-dialogs.py` now sweeps Show Tip of the Day as a `canvas:` entry (it
  opens no window) and finds About in Cantonese mode by the fixed words of "&About %s"; tried on `md3-v162`, the
  Cantonese About opens with no finding. Tests: `canvas-frame-capture.test.mjs`, `sweep-dialogs-menus.test.mjs`.
- In-app changelog (2026-09-29): exported up to `md3-v164` (170 releases, 1,355 entries); the 43 new commits were
  drafted in Cantonese in a batch and reviewed line by line, 32 of them rewritten for sense or Hong Kong usage.
- Context menus (2026-09-29): every context menu is the Material one. `MD3::EnableTextContextMenus` (an event filter
  installed by `GUI_App` at startup) answers every text entry's menu request with Undo, Cut, Copy, Paste, Delete and
  Select all, never Cut or Copy for a masked field; the kit fields let the right-click through; the copyable device
  labels use `MD3::PopupMenu`; the wiping dialog's web page has no browser menu; What's new's year field is the kit
  `SpinInput`. `scripts/md3/check-context-menus.py` found the system menu on `md3-v162` (keyboard request on the Smart
  home and Preferences fields, nothing on a right-click) and fails on it. It also showed that the dialog sweep's gear
  entry had measured a 160 x 243 popup instead of Preferences on every release so far; the sweep now waits for the
  dialog, and captures the six caption bar menus as `menu:` entries. Tests: `context-menus.test.mjs`,
  `check-context-menus.test.mjs`.
- Last native-looking fields (2026-09-29): the appearance editor's three decimal fields are `AppearanceDecimalField`
  (the kit `TextInput` with two kit chevron steppers, same ranges and steps; the steppers are named with the new
  catalogue strings Increase and Decrease, Cantonese 增加 and 減少); the colour picker's HEX and any-format fields sit
  in the kit `TextInput`; the object list's rename editor is a Material filled field. `md3-conversion-contracts`
  refuses `wxSpinCtrl` and `wxSpinCtrlDouble`. Tests: `appearance-decimal-field.test.mjs`, `kit-text-fields.test.mjs`.
- Message boxes (2026-09-29): 22 system message boxes had come back with later features (workspace panel 15,
  appearance editor 6, ink map swap 1) although the parity register marked the sweep done. `md3_message_box()` in
  `MsgDialog.{hpp,cpp}` takes wxMessageBox's arguments and returns its values on the Material `MessageDialog`, so each
  site only changed its name; `GUI_App::show_message_box` uses it too. The five fatal-path boxes (GUI_Init twice,
  GUI_App fatal, critical and first language load) stay native. `message-boxes.test.mjs` refuses any other.
- Tooltips (2026-09-29): `style_tooltips_md3()` in `GUI_App.cpp` removes the visual style of wx's shared tooltip
  control and gives it the Material plain tooltip's colours (InverseSurface, InverseOn), margins, the kit's small font
  and Windows 11 small rounded corners, once the main window exists and after every theme change.
  `scripts/md3/check-tooltips.py` hovers a Smart home button on a hidden desktop and judges the tooltip by its colour.
- Missing include (2026-09-29): `8c1e4a5ab` made the appearance editor call `md3_message_box()` and `MessageDialog`
  with no include path to `MsgDialog.hpp` (the forced precompiled header does not include it), so
  `AppearanceEditorPopover.cpp` cannot compile in that commit or any later one before `784d86ff3`, which adds the
  include. `dialog-header-includes.test.mjs` follows every source's quoted includes along the compiler's search path
  and fails when a file that uses a Material dialog cannot reach the header.
- Stock dialogs (2026-09-29): `TextEntryDialog`, `NumberEntryDialog` and `MultiChoiceDialog` in `MsgDialog.{hpp,cpp}`
  take the stock wx arguments on the MsgDialog shell with kit fields; `BusyInfo` is a rounded SurfaceContainerHigh
  panel painted before the blocking work. They replace 11 stock calls (appearance editor, workspace panel, Plater
  copies, clone, reload and replace, the layer range settings chooser, compatible presets, the web view's developer
  prompts), and `GetSingleChoiceIndex` uses the kit `SingleChoiceDialog` everywhere. Every ink colour goes through
  `pick_filament_color()` (`wxExtensions`): the Material picker, opaque, with the app config's sixteen recently used
  colours as quick picks; the three `wxColourDialog` sites are gone. `stock-dialogs.test.mjs` refuses the stock
  dialogs; only the developer-only log window stays (compiled out of releases).
- Native controls (2026-09-29): a disabled kit Button shows its tip in `ButtonDisabledTip` (the Material plain
  tooltip) instead of `wxTipWindow`; the web panel's cloud-page notice is `MD3InfoBanner` instead of `wxInfoBar`; the
  Workspace panel uses `TextTabbar` + `wxSimplebook`, `wxDataViewListCtrl` tables, the kit `ListBox` with the new
  `EnableChecks()`, and `wxGenericCalendarCtrl` in Material colours. `TextTabbar` lost its legacy white, grey and
  brand green. `md3_style_data_view()` (`wxExtensions`) styles the Workspace tables, Config profiles, Export, Version
  history and the notification centre. `native-controls.test.mjs` refuses the native classes. Still native with a
  group box: `wxStaticBoxSizer` in 8 files, the option groups outside settings tabs, and the bed shape page chooser.
- Group boxes and standard buttons (2026-09-29): `MD3GroupBox` (`Widgets/StaticGroup.{hpp,cpp}`) is a `wxStaticBox`
  whose `PaintForeground()` draws an OutlineVariant outline and a Head_14 title, the same hook `StaticGroup` already
  overrides; it replaces every native group box (option groups outside tabs, bed shape, calibration wizard pages, Save
  preset, the unsaved changes comparison, the ink picker preview, the Plater's sliced info). `StaticGroup` takes the
  Material roles. The bed shape dialog chooses its page with the kit `ComboBox` over a `wxSimplebook`, without fixed
  white or raw red. Bed shape, System info and the full comparison use kit OK and Cancel buttons with the standard ids
  in place of `CreateButtonSizer()` / `CreateStdDialogButtonSizer()`. The never-called native preset tree in
  CreatePresetsDialog is gone. `native-controls.test.mjs` refuses all of these.
- Scrollbars (2026-09-29): `MD3ScrollBars` (`Widgets/MD3ScrollBars.{hpp,cpp}`) draws the kit scrollbar for a window
  whose native scrollbar calls are forwarded to it: `SetScrollbar`/`SetScrollPos`/`GetScroll*` record the scroll
  helper's position, page and range instead of calling Windows, `MSWGetStyle()` drops `WS_HSCROLL`/`WS_VSCROLL`, and
  `MSWWindowProc()` offers each message to `Before()`/`After()`, which reserve a 10 DIP strip per shown bar in
  `WM_NCCALCSIZE` (so the client area shrinks like it did for a native bar), paint it on `WM_NCPAINT` and `WM_PAINT`,
  claim it as `HTBORDER` in hit testing, drag the thumb with `wxEVT_SCROLLWIN_THUMBTRACK`/`THUMBRELEASE`, and page with
  `PAGEUP`/`PAGEDOWN` on a 350 ms then 50 ms timer. Showing or hiding a bar calls `SetWindowPos(SWP_FRAMECHANGED)`,
  which brings the same `WM_SIZE` a native bar brings. `MD3ScrolledWindow` replaces all 84 `wxScrolledWindow`
  constructions and subclasses (56 files), and the kit `ListBox` now creates its window from its own constructor body
  so the overrides apply from the start. The owner must be created through its own `Create()`, never through a base
  constructor, or Windows gets a scrollbar style before the override exists. The layout probe writes `scrollbars`
  (`native_v`, `native_h`, `kit_v`, `kit_h`) for every window. `scrollbars.test.mjs` and the include guard cover it;
  not yet in a release.
- Context menus verified (2026-09-29): on `md3-v169` (`087fe6f70`, package `2.8.4620`, SHA-1 `7e63bc7e…`),
  `scripts/md3/check-context-menus.py` got the Material menu for 8 of 8 requests per mode in English, Cantonese and
  bilingual (right-click and keyboard, two Smart home fields and two Preferences fields); the full sweeps found no
  layout finding in 20 dialogs per mode, and the language audit found no English-only label. Captures are in
  `docs/screenshots/md3-everything/` under the md3-v169 section.
- Table scrollbars (2026-09-29): `MD3DataViewCtrl` and `MD3DataViewListCtrl` (`Widgets/MD3DataView.{hpp,cpp}`) do
  the same forwarding for tables. ObjectList, AuxiliaryList and DiffViewCtrl derive from `MD3DataViewCtrl`, and the
  seven `wxDataViewListCtrl` tables are `MD3DataViewListCtrl`. `wxDataViewCtrl::MSWWindowProc` is private, so both
  call `wxDataViewCtrlBase::MSWWindowProc` and add the `DLGC_WANTARROWS` it added, keeping the arrow keys for the
  selection. The Objects list's ink editor leaves room for the kit bar.
- Text box scrollbars (2026-09-29): `TextAreaEditor` (`Widgets/TextArea.{hpp,cpp}`) is the multi-line editor for
  `TextArea`, the settings' G-code fields (`Field.cpp`, `Builder<TextAreaEditor>`) and the regex builder's sample and
  results. A Windows edit control sets and draws its own bar, so the editor creates it without `WS_VSCROLL`/`WS_HSCROLL`
  (a rich one also without `ES_DISABLENOSCROLL`, bit `0x2000`, which a plain edit uses for `ES_NUMBER`), lets it keep
  scrolling itself, and after every message that can move the text reads `EM_GETFIRSTVISIBLELINE`, `EM_GETLINECOUNT` and
  `EM_GETRECT` / `GetCharHeight()` and hands them to `MD3ScrollBars::SetScrollbar()`. `MD3ScrollBars::SetScrollHandler()`
  routes the strip's drag and page to `EM_LINESCROLL` / `EM_SCROLL` instead of a wx scroll event. `m_syncing` guards the
  re-entry the `EM_GET*` queries cause.
- HTML view scrollbars (2026-09-30): that entry once said no Windows scrollbar was left in the GUI sources. It was wrong:
  `wxHtmlWindow` is a `wxScrolledWindow` of its own, and nine of them in seven dialogs (the message box body when it holds
  a table or a link, System Information, About, the setup wizard, Helio's terms, the version policy notice, Send system
  information) still drew the Windows bar. `MD3HtmlWindow` (`Widgets/MD3HtmlWindow.{hpp,cpp}`) is MD3ScrolledWindow's
  arrangement for `wxHtmlWindow`; wx lays an HTML page out by forcing the bar on and then letting it hide, which reaches
  the kit strip as a range of -1 that it already shows as an empty track. `scrollbars.test.mjs` refuses a new
  `wxHtmlWindow`. Still open from the same audit: the object settings table is a `wxGrid` with Windows bars, and a few
  native combo boxes, buttons and text fields remain (tracked for the next change).
  `scripts/md3/scan-scrollbars.py <dump folders>` lists every shown window with a Windows bar in a release's
  layout-probe dumps (exit 1), or says the build predates the `scrollbars` field (exit 2).
- Closeout cleanup (2026-09-29): the linked worktree of the auto-updater lane
  (`BambuStudio-claude-auto-updater`) and its branch `claude/auto-updater` (`37b3fce78`, contained in `main`, no copy
  on the remote) were removed after an archive of the repository to the maintainer's cloud folder was written and
  read back (4,468,361,508 bytes, 151,602 entries). The other worktrees and the primary checkout's untracked
  `fonts/` folder were kept: none is proven to belong to this work.
- Upstream Bambu Studio `v02.08.04.61` (tagged 2026-09-29, seven commits on `v02.08.04.57`) is merged in
  `a0e408559` without conflicts: error dialog buttons act on mouse-up, the progress dialog no longer
  yields on Windows (upstream's fix for hangs after sending a print or loading a project from the device
  page), a macOS-only WebView change, and `version.inc` at `02.08.04.61`. The network agent version
  stays `02.08.04.57`, as upstream left it. The official baseline diagnostics still compare against the
  official `v02.08.04.57` release on purpose.
- Privacy of the capture evidence (2026-09-29): capture profiles had lived under the Windows user
  profile, so two public Config profiles captures (`md3-v143`, `md3-v150`, added in `e92b7fa2d`) showed
  the account name in the data folder, and twelve layout dumps from 2026-09-06/07 recorded it in the
  download folder field. The captures were taken again with a profile under `C:\Users\Public`, the
  dumps' folder value is redacted, `prepare-capture-datadirs.py` refuses a root inside the profile,
  and `ui-md3/tests/evidence-privacy.test.mjs` guards the text evidence. The old files remain in the
  repository history; removing them there needs a history rewrite and a force push, which has not
  been done and needs the owner's decision.
- Release jobs share one concurrency group, and GitHub keeps one running and one waiting: a newer build
  that finishes while one waits cancels the waiting release job. The `b5cde7521` build passed but its
  release was cancelled that way on 2026-09-29, so not every push gets a release when pushes come fast.
- Not verified: behaviour in a running application in any of the three modes (no captures from a released build
  yet), and human review of the agent-drafted Cantonese.

## Dependency security alerts (issue #47, 2026-09-29)

Scope: the 17 open Dependabot alerts (9 high, 8 moderate) that every push reported. The full record,
including how to triage the next alert, is
[`docs/features/releases/dependency-security-alerts.md`](docs/features/releases/dependency-security-alerts.md);
the tracking issue is [#47](https://github.com/Ding-Ding-Projects/BambuStudio/issues/47).

- None of the seven flagged packages ships. The built device page bundles 36 runtime packages (React,
  Radix UI, TanStack Router, i18next, immer, zustand and their helpers); every flagged package is a build,
  lint or test tool, and no vulnerable function is called with outside input.
- Three security pins in `src/slic3r/GUI/DeviceWeb/device_page/package.json` (`pnpm.overrides`) held
  versions inside an advisory range and were raised one patch release each: `js-yaml` 4.3.2 and `nanoid`
  3.3.18 in `75fc64c69`, then `undici` 7.29.1 in `9eb6ee5d2`. `pnpm-lock.yaml` was regenerated with pnpm
  10.12.1 `--lockfile-only`; only those packages changed.
- All 17 alerts are dismissed: 13 as `not_used` (the unused npm `package-lock.json`, vitest's unused plugin
  path, the manually run `tests/web-e2e` harness) and 4 as `inaccurate` (#2, #19, #10, #17: the lockfile on
  `main` no longer holds the flagged version).
- Verified locally with the Node 22.22.2 and pnpm 10.12.1 from `../node-cache`: the frozen install passed and
  all 16 files of `dist/` (4,079,543 bytes) were byte-identical before any pin moved, after `75fc64c69` and
  after the `undici` pin; `pnpm run test:a11y` passed 7 of 7; four of the five dependency-free tests passed.
  The C++ app was not built locally.
- Verified on the hosted runner and in the shipped package: the `device_page_build` step of runs
  [36611172274](https://github.com/Ding-Ding-Projects/BambuStudio/actions/runs/36611172274) (`75fc64c69`,
  build passed, its release job was cancelled while waiting behind a newer one) and
  [36614198573](https://github.com/Ding-Ding-Projects/BambuStudio/actions/runs/36614198573) (`46792f02a`)
  logged "Lockfile is up to date" and built the page. The second run then failed in
  `AppearanceEditorPopover.cpp` (`C2065 MessageDialog`, `C3861 md3_message_box`), a file this work never
  touched: the errors came from `8c1e4a5ab` and `784d86ff3` repaired them. Releases
  [`md3-v170`](https://github.com/Ding-Ding-Projects/BambuStudio/releases/tag/md3-v170) (`784d86ff3`) and
  [`md3-v171`](https://github.com/Ding-Ding-Projects/BambuStudio/releases/tag/md3-v171) (`85a1d9e86`) carry
  all three pins (`md3-v169` from `087fe6f70` has the first two), and the CycloneDX inventory of
  `md3-v171` lists all 16 device page files with the same SHA-256 values as the local build from before
  any pin moved.
- Found on the way:
  - The repository's dependency graph reported "disabled", so Dependabot never rescanned pushes and
    fixed alerts never closed by themselves. With the maintainer's approval it was switched back on at
    22:03 UTC through `gh api -X PUT repos/Ding-Ding-Projects/BambuStudio/vulnerability-alerts` (alerts
    and the graph only; the organization's "GitHub recommended" configuration was not attached because
    it would add CodeQL runs). Its snapshot still dated from 2026-08-11 afterwards, so every September
    alert had been matched against August lockfiles; the next push that changes the device page
    manifests should refresh it.
  - With the graph back on, three advisories published that afternoon raised #25 (auto-dismissed), #26
    and #27 on `undici` 8.9.0 in the unused npm lockfile; #26 and #27 are dismissed as `not_used`. The
    pnpm pin already holds 7.29.1, the first fixed 7.x release for both.
  - An auto-triage rule dismisses low-impact alerts on development-scope packages within a second
    (#4, #5, #8, #9, #22, #23, #24, #25); check those against the lockfiles too, which is how the
    `undici` pin was found.
  - `tests/buildSpoolFromTray.test.ts` failed with `ERR_MODULE_NOT_FOUND` from `68f42a887` until
    `0a0bb64c1` (an extensionless import in `src/features/filament-manager/constants.ts`), independently
    of this work; all five DeviceWeb tests pass since `0a0bb64c1`.
  - A local pnpm check in a path deeper than about 180 characters fails on Windows (`ENAMETOOLONG` in the
    patched `minimatch` step, then `ERR_PACKAGE_IMPORT_NOT_DEFINED` from `vite`); run it inside the
    repository's own `device_page` folder, as the build does.
- Open: the graph's refresh of the device page manifests (next manifest-changing push), whether to
  remove the unused npm `package-lock.json` (it keeps raising alerts now that the graph is on), and
  vitest 4.x (the only way to clear GHSA-82fw-gwwq-j7x9 at the source).

## Faster hosted Windows builds (2026-09-29)

Scope: shorten the hosted Windows build. Full record:
[`docs/features/releases/windows-release-supply-chain.md`, "Build cache"](docs/features/releases/windows-release-supply-chain.md#build-cache).

- Cause, from run [36631880242](https://github.com/Ding-Ding-Projects/BambuStudio/actions/runs/36631880242)
  (`bb78abee1`): "Build slicer Win" took 72 minutes of the 80-minute build job. sccache cached 99 of 795
  compile requests; the other 696 use the precompiled header (`/Fp` 693, `/Yc` 3), which it cannot cache.
- Change (in source, not yet run on GitHub): the build-time stamp moved from `libslic3r_version.h` (in
  every object's includes and the precompiled header) to `libslic3r_build_time.h`, which three sources
  include. `scripts/ci/Restore-BuildCache.ps1` restores the last `main` build tree from the draft release
  `build-cache-windows` before the compile, and `scripts/ci/Save-BuildCache.ps1` saves each successful
  `main` push build there in 7-Zip parts of at most 1,500,000,000 bytes. The steps use the owner token
  `TOKEN_GITHUB`, passed down with `secrets: inherit`. Any problem means a build from scratch, never a
  failed build; `[cold build]` in a commit message forces one.
- Checked: `ui-md3/tests/build-cache.test.mjs`; a local run of both scripts against a stand-in `gh` (34
  checks, including every fallback); a Ninja file whose steps only write files (no compiler), confirming that after the 7-Zip
  round trip only changed sources and their dependents rebuild; a stand-in runner showing that the
  background save must start hidden and log for itself (started with redirected output, it held the
  step's output open for 20 seconds after the step exited); and a live round trip of every `gh
  release` command the scripts use on the (still empty) draft `build-cache-windows`, which anonymous
  visitors cannot see.
- First set saved: run [36645906111](https://github.com/Ding-Ding-Projects/BambuStudio/actions/runs/36645906111)
  (`84b96e720`, run number 633) built from scratch, as its own commit message asked, and its background save finished
  within the packaging steps: a 6,786,436,758 byte tree in one part of 672,688,710 bytes, plus the manifest and
  `windows-build-latest.json`, in the draft `build-cache-windows`. Its compile step took 55 min 52 s.
- First warm build: run [36739933076](https://github.com/Ding-Ding-Projects/BambuStudio/actions/runs/36739933076) (`eff5fe381`, run number 634). Restore step 68 s, notice
  "Build cache: restored the tree built from 84b96e720b8516895bf6b74b0429f46592b3639d (run 633); 23 files changed
  since." Compile step 3 min 4 s with 10 compile requests (795 before the cache); build job 12 min 44 s (80 min 16 s
  before). It published `md3-v176` (package `2.8.4634`) and saved its own tree as the next set, one part.
- One warm run is one measurement. A change to a widely included header still rebuilds every source that includes
  it. If a warm build ever looks stale, push with `[cold build]` and compare.

## Executable path buffer in the file association code (issue #49, 2026-09-29)

Scope: `GetModuleFileNameW` takes its buffer size in characters, but `is_associate_files`,
`GUI_App::associate_files` and `GUI_App::disassociate_files` passed `sizeof(app_path)` for a
`wchar_t app_path[MAX_PATH]` (520 bytes), so Windows could write an executable path of 260 characters or more
past the end of the buffer. The handoff record is [#49](https://github.com/Ding-Ding-Projects/BambuStudio/issues/49).

- `d49b4ea68` takes the path from `current_executable_path()` in `associate_files` and `disassociate_files`
  (the helper the automatic update code already used, which grows its buffer until the path fits) and drops
  the lookup from `is_associate_files`, which never read it. The registry values written are unchanged.
- `ui-md3/tests/module-file-name-size.test.mjs` scans every C and C++ source under `src/slic3r/GUI` (comments
  and literals blanked, arguments split by counting brackets) and fails on a `sizeof` byte count passed to
  `GetModuleFileName`, `GetModuleFileNameW`, `GetModuleFileNameEx` or `GetModuleFileNameExW`. It failed on
  `GUI_App.cpp` lines 323, 9424 and 9447 before the change (3 of 5 tests passed) and passes after it.
- Verified: `node --test` 255 of 255 at `d49b4ea68` and 304 of 304 at `9409f33ae`; Build BambuStudio passed
  in [run 36609309787](https://github.com/Ding-Ding-Projects/BambuStudio/actions/runs/36609309787), which
  published [`md3-v168`](https://github.com/Ding-Ding-Projects/BambuStudio/releases/tag/md3-v168) (target
  `d49b4ea68`, package `2.8.4617`, all five assets download). The C++ app was not built locally.
- Open: file association in a running application (associate, then disassociate `.3mf`, `.stl` and `.step`)
  was not re-checked on `md3-v168`; it rewrites the current user's file associations, so run it on a test
  machine.

## Native controls on the kit, and prompts that can be dismissed for good (2026-09-30)

- `md3-v178` was built from scratch on request (`[cold build]` in `9a0a18064`, run 36784220441) and published.
- Seven conversions of native controls that a release still showed, each reviewed for compile and behaviour before
  it was committed: five dialogs get the kit caption instead of the native title bar (sign-in, dispenser humidity,
  texture add-ink chooser, Add to Library prompt, recorded-ink notice); the save-as replace box, export progress card,
  command palette and DLL warning; the last bare single-line editors and the Height range buttons; image and swatch
  tiles as kit Buttons; the printer connection buttons and the network lookup list; the Parameter Table grid and the
  search popups with the kit scrollbar; ScalableButton as a kit Button with hover, focus and disabled states. Each
  carries its own `ui-md3/tests/lane-*.test.mjs`.
- The sync prompt ("Sync now / Later") and the new-ink prompt have "Don't show again"; the answers are kept in
  `sync_ams_info_choice` and `hide_new_filament_prompt` and cleared by Preferences, Reset all warning dialogs.
- Not compiled locally; the hosted build of this push is the first compile of all of it. ScalableButton is used
  widely, so its conversion is the one to watch in the build and in the first captures.
- Unfinished and not merged, saved as tags on the remote: `preserve/tpu-display-rename-20260930` (the display-only
  rename of the material type TPU-AMS that the owner chose) and `preserve/preview-capture-driver-20260930` (a Preview
  capture driver). Both stopped at a usage limit before their checks ran.

## Preview overlays, ink wording and installer shortcuts (2026-09-30)

- **Preview overlays (issue #51), `856d92a2c`.** Full record: [`docs/features/gcode-preview/preview-overlays.md`](docs/features/gcode-preview/preview-overlays.md).
  The legend dock reports its width and the notification column, the error banner and the slicing card stop left
  of it; the status chip starts right of the plate strip; the All Plates Stats tile paints its glyph and label
  above the wash; the view-mode combo and the time estimation card span the dock; the Ink Grouping card measures
  its content; the Objects list is created without the native header and the system frame; the four plate
  dropdowns follow the row width with the old 12 em as the minimum.
  - Checked: `ui-md3/tests/preview-overlays.test.mjs` and `native-controls.test.mjs` (8 new checks, 6 of them seen
    failing on the previous sources); `node --test` 320 of 320. Compiled by run [36747308882](https://github.com/Ding-Ding-Projects/BambuStudio/actions/runs/36747308882) and published as `md3-v177` (package `2.8.4635`).
  - Not checked: no capture of the Preview tab exists from a release yet. The clipping inventory rows CJ-031 to
    CJ-034 are `fixed-unverified`; CJ-035 (plate type at the default sidebar width) is open.
  - The report came from an installation built on 2026-07-30 (file version 02.08.01.55), not from a current
    release. Two of its eleven items were already fixed in current releases; one is by design (the Preview accent).
- **Ink wording, `4ef3a9394` and `3bb44bc5d`.** 94 English overrides and 19 Cantonese values;
  `scripts/i18n/check_ink_overrides.py` starts from the extracted template and `Test-InkTerminology.ps1` runs it.
  The second commit routes the text that skipped the catalogue (dispenser name tables, names a printer reports,
  undo names, web pages, the device page) through it, with short context wording where the space is narrow.
  Left on purpose: the material type TPU-AMS (stored in presets and 3MF files; the owner is asked), text the
  printer or cloud sends, dated history. The narrow-space widths are estimates from the source, not measured.
  Correction: the message of `4ef3a9394` says 93 messages reached the screen in the upstream words. They did not:
  `LanguageModeService::finish` runs `vocabulary()` over every translated string in every mode, a whole-word
  rewrite, so they already read ink, as a word swap ("a ink"). The overrides give the grammar and put the wording
  in the catalogue; the Cantonese fixes and the text that skipped the translation layer were the visible part.
  The message cannot be changed without rewriting published history, so the article records the correction.
- **Installer shortcuts, [issue #52](https://github.com/Ding-Ding-Projects/BambuStudio/issues/52).** On an installed `md3-v173` the shortcut named
  Bambu Studio MD3 started `bambu-regex-worker.exe`, and the application's own shortcut was named BambuStudio in a
  Start Menu folder "Bambu Research": no executable in the package was marked as aware of the installer, so it
  made a shortcut for every one. The launcher is now marked aware (`040904B0` block), handles the install events
  itself and makes its own shortcuts, and `Verify-HostedSquirrelInstall.ps1` reads them back after the hosted
  install ([record](docs/features/windows/app-updates.md#shortcuts-and-install-events)). In source; the next
  release run is the first real install of it.

## Branch and worktree cleanup (2026-09-29)

- `0d883d9fe` records 21 older `codex/*` branches as merged with `-s ours`: `git cherry` showed every one of their
  commits already on `main` as an identical patch, so the tree did not change (the commit carries `[skip ci]`).
- Deleted only after an ancestry check against the pushed `main`: 39 local branches, 9 remote branches and one clean
  agent worktree. A verified archive of the Git directory and every worktree was taken first.
- Kept on purpose:
  - `main` and the four branches that workflows trigger on (`codex/official-feature-reapply`,
    `codex/official-native-reapply`, `codex/hosted-startup-stack`, `codex/hosted-behavior-verifier`);
  - `codex/workspace-core`, `codex/workspace-history-repair` and `codex/print-setup-quick-swap` (local only): each
    has one commit whose content `main` may hold in a later form, so its author should review it before it is merged
    or deleted;
  - three unfinished agent checkpoints on the remote (`worktree-agent-a24267613001f90e0`,
    `worktree-agent-a28cd0c64451603b9`, `worktree-agent-a470984c061728c07`: an offline documentation browser, bulk
    actions, scheduled settings), never merged;
  - branches and worktrees still open in other sessions.
- Pushing any branch to this repository runs the Windows build and publishes a release, so preserved work that must
  not ship stays local or goes to a tag, never to a new branch.

`docs/reapplication/source-manifest.csv` gives the selected path inventory and
source/official blob IDs. `docs/reapplication/loader-adapters.md` records the
project-loading changes and focused hosted regression cases. The full old-tree
patch was not used as a completion verdict. Official macOS and Linux source
files were retained. Generated translation binaries, untracked fonts, old
captures, legacy NSIS source-repair files, and old build output were excluded.

## Verification

At `acd4c0489`, [run 36349497764](https://github.com/Ding-Ding-Projects/BambuStudio/actions/runs/36349497764)
completed successfully. It passed 10 contract checks and 36 driver checks,
including parsing and manifest negative cases. These checks establish the
hosted diagnostic route's stated contracts, not native application behavior.
The separate [production run 36349494073](https://github.com/Ding-Ding-Projects/BambuStudio/actions/runs/36349494073)
failed during CMake configuration at `CMakeLists.txt:224`, before C++
compilation. A hook-copying repair subsequently reached `main` at `73d50e270`;
no production package verdict follows from that source edit. The follow-up
[production run 36350056149](https://github.com/Ding-Ding-Projects/BambuStudio/actions/runs/36350056149)
passed configuration, then failed at native compilation with MSVC `C1041`
while opening the shared `mcut.pdb`. The command already included `/FS`;
the cause of the PDB access failure is not established.

[Four-case diagnostic run 36348272737](https://github.com/Ding-Ding-Projects/BambuStudio/actions/runs/36348272737)
recorded natural startup exit `0x80070057` in all four cases, with teardown
verified and zero screenshots. It did not establish the native crash cause or
prove that a saved 3MF loaded. No private project path or filename is part
of this public record.

[Startup trace run 36350457096](https://github.com/Ding-Ding-Projects/BambuStudio/actions/runs/36350457096)
at verifier `548ab63d86f88db31e348316c9520d037e250180` passed its five
focused Python checks and PowerShell parsing, but the trace failed. It used
the verified `md3-v125` package `2.8.4124` and the same installed executable
hash recorded below. CDB could not attach because the selected process was
already exiting (`NTSTATUS 0xC000010A`). No attach or breakpoint marker was
observed. Natural process exit remained `0x80070057`; owned process, holder,
debugger stop, and named-desktop teardown were verified. The owner decryptor
validated two restricted entries, a behavior report and debugger log. There
are zero images and no usable stack. Raw diagnostics remain restricted.
The encrypted bundle SHA-256 is
`a7cc5390cc3b3a5814b8f7d53eeef632a1daa81af45809a359cb20ebc971288f`.
The workflow's failed verdict is preserved; successful encryption is only
partial diagnostic transport, not application verification.

[`md3-v129`](https://github.com/Ding-Ding-Projects/BambuStudio/releases/tag/md3-v129)
is a non-draft release targeting earlier source `c7cb11752a65810e4b02f4124b4c4b22c8438218`
and contains package version `2.8.4128`. It does not package the fresh
official-source candidate or the newer `73d50e270` source. The native
behavior, file-opening fix, installed current
package, fresh GUI captures, and localized Features page remain unverified.

### Earlier candidate record

The earlier production [run 36346636917](https://github.com/Ding-Ding-Projects/BambuStudio/actions/runs/36346636917)
started for then-current `main` commit `e2c7d6ab6d55354b48504618c1486285c043d63a`
and was in progress at that earlier handoff. Run
[36345894046](https://github.com/Ding-Ding-Projects/BambuStudio/actions/runs/36345894046)
was in progress at that earlier observation. It later completed with a failure
in the `Build slicer Win` step; its release publication job was skipped. Run
[36345306787](https://github.com/Ding-Ding-Projects/BambuStudio/actions/runs/36345306787)
also remained in progress at that earlier observation. The independent
[diagnostic run 36345897684](https://github.com/Ding-Ding-Projects/BambuStudio/actions/runs/36345897684)
at `0ff1ffdbd` used the installed `md3-v125` executable identified by SHA-256
`f430f31a60486e27debc97cf0e41926072420dbf8683cd0503c6485510d9ef03`.
It recorded natural startup exit `0x80070057` after 20 seconds at
2026-09-27 19:54:58 UTC. Holder identity, desktop identity, and teardown were
validated; it produced no image, matching log, or matching Windows Error
Reporting record. The later four-case diagnostic supersedes that narrow
observation. The startup cause remains undetermined.

`md3-v127` targets `6a45418f` and its production run `36338478333` succeeded,
but it predates the fresh official-source candidate and was not marked latest
at the last API check. `md3-v125` remains the latest-designated release; it
targets `c5df6199e1a83b1c94be12e999c0b322fded8730`. Neither release
verifies the fresh `e2c7d6ab6` source. The source review of native model and
loader changes is not a packaged application or GUI verdict. The public
fixture is parsed as nine model objects, including `flowrate_m5`; its
verification does not infer load success from a filename.

All builds, tests, installation, and GUI checks for this pass are hosted. No
new package, feature interaction, native DPI tuple, or privacy-reviewed image
has been verified for `e2c7d6ab6`. The localized feature guide remains
unpublished. Existing hosted jobs and ownership-uncertain files must be
preserved.

## Next actions

1. Investigate the repeated `C1041` in production runs `36350056149` and
   `36350518177`, and inspect the workflow associated with the eventual
   documentation-only successor. Investigate the MSVC/sccache debug-information path;
   do not assume adding `/FS` fixes a command that already contains it.
2. Move debugger attachment earlier while preserving exact process identity
   and hidden-desktop isolation, then compare official and candidate 3MF
   opening under isolated profiles, including `loader-adapters.md` rollback cases.
3. Establish the cause of the installed startup exit from a genuine trace or
   matching report; the exit code alone does not identify the source defect.
4. Drive requested features in the exact built package, measure native DPI,
   and capture and review fresh pixels before claiming GUI verification.
5. Publish the localized feature guide and release links only for verified
   behavior, then update distribution records with the exact release source.
6. When a release carries `3a935ed9f` (payload DLLs): confirm it starts with no DLL added, capture the
   splash date in the three modes, take the after captures for CJ-014 to CJ-019, rerun the dialog
   sweep with the probe's `truncated` field, and, once a second release follows, install the older one
   and watch it update and restart (issue #46).
7. The updater's runtime check needs an isolated Windows account or machine: Squirrel installs per user
   under `%LOCALAPPDATA%\BambuStudioMD3`, and the development machine's own installed copy
   (`app-2.8.4142`, which is `md3-v143`, the release that cannot start) must not be replaced by a test.
   Installing `md3-v148` or later there by hand fixes that copy; a copy with the updater then updates
   itself from the next release, which is itself a check.

## Preservation boundary

All completed changes above reached remote `main`. Task-owned checkouts were
clean at the preservation inventory. The three older conflicted checkouts,
ownership-uncertain entries, and untracked fonts remain preserved. No local
build, test, installation, rendering, GUI drive, power action, job cancellation,
or destructive cleanup was performed. No new behavior claims or feature
guide were published. Existing workflow handles must be retained and allowed
to finish. This is an incomplete handoff, not a release-completion claim.


## Historical pre-reconstruction handoff

The following record is retained from `c5df6199e1a83b1c94be12e999c0b322fded8730`. Its dated statements describe that earlier baseline; the current reconstruction status above and its verification record take precedence. Unfinished entries are preserved, not newly adopted into the current scope.

<details>
<summary>Earlier record</summary>

# HANDOFF — read this first

> [!IMPORTANT]
> **Current task state, 2026-09-27 02:28 UTC:** The current source and hosted
> installation verifier reached remote `main` at
> `181de2e669aa3b7b243a206372790fd543515ccb` (verified with `git ls-remote`).
> The source includes upstream `v02.08.04.57`, the requested print, LAN,
> camera autoplay, fan, Model Creator, portable-history, workspace, calendar,
> Cantonese, and Prepare sidebar tab changes. The direct
> `DevMappingNozzle.h` include in `SendMultiMachinePage.cpp` is at `fbb5e75`;
> the Model Creator opening change is at `022b159`; the three-tab sidebar
> and its 128 DIP rail are at `93d92e1` and `830b9e1`; localization is at
> `bfcf0bc`. The isolated hosted Squirrel installation verifier is at
> `181de2e`.
>
> [`md3-v120`](https://github.com/Ding-Ding-Projects/BambuStudio/releases/tag/md3-v120)
> is a non-draft, superseded release targeting `fbb5e75`, with the five
> expected asset names. It predates the Model Creator and sidebar changes.
> [Main run 36287411091](https://github.com/Ding-Ding-Projects/BambuStudio/actions/runs/36287411091)
> was still in `Build slicer Win` at its last check for `181de2e`.
> [Branch run 36285563594](https://github.com/Ding-Ding-Projects/BambuStudio/actions/runs/36285563594)
> was still compiling at its one-hour observation bound and was left running.
> The prior main run `36284305193` was likewise left running at its bound.
> No terminal success verdict for the intended current-source candidate was
> recorded in this handoff. Its release, isolated installed-version receipt, real
> GUI behavior matrix, and genuine current-build captures remain unverified.
> The localized Pages feature guide is deferred until release verification;
> the detachable camera widget remains a separate unimplemented follow-up.
> No local native build or change to the user's installation was made.
> `ROADMAP.md` lists the remaining checks.
>
> At this preservation checkpoint, the primary worktree has no tracked
> changes. Pre-existing `.claude/worktrees/` and `fonts/` files have uncertain
> ownership and were left untouched. The `upstream-build-web`, `upstream-gui`,
> and `upstream-preferences` worktrees have unresolved merges with 38, 40, and
> 55 conflicted paths respectively; their owners must resolve and preserve
> those changes before any integration or cleanup. No cleanup archive was
> created and no worktree was removed.
>
> A dedicated public screenshot gallery was not published. The existing Pages
> screenshot manifest contains 18 files with matching hashes, but its capture
> date, timezone, and exact source commit are unavailable. The larger native
> inventory has 167 entries marked done and 127 pending without per-image
> hashes. No image from this inventory was promoted as new release evidence.
> Earlier sections below are historical records and must not be read as the
> current release or current repository state.

> [!NOTE]
> **Requested next task, not implemented here:** Add a desktop widget for the
> full live camera stream. Prefer a detachable, resizable view in the existing
> wxWidgets application, with an optional always-on-top mode. Reuse the current
> camera playback and authentication path instead of creating a second camera
> session. The next owner should verify that the existing player can move into
> a separate top-level window, and record a concrete limitation before choosing
> a companion runtime. Keep printer switching, manual stop, reconnection,
> connection cleanup, privacy, keyboard access, and window-state persistence
> explicit. This is a handoff item only. There is no widget code, build, or
> runtime verification in the current release candidate.

> [!IMPORTANT]
> **Current build repair, 2026-09-25 (America/Toronto):** `main` was verified on the
> remote at `1601ab8c0d5fbc1029342009953258f12233ac0a`. Preferences now uses
> stable tab IDs instead of the removed `TabStrip::SetSelection` API
> (`2db0346091d1c24aa7ff739e1cbc118c713a08f5`), Command Palette's article
> loop is closed (`a02c52bbfbe4d5ede2daf4b8d59410b8c4dc3089`), and preset
> export uses an existing fallback bitmap (`b53fad13cdbea61fceb7896f2b926bf9f023908d`).
> The local Release build exited 0. Its `BambuStudio.dll` SHA-256 is
> `E942B5D328E6E8DD4BDC9B4E08AA3534830A3C5362248FD3D50482C3F8403CEA`.
> A hidden-desktop launch opened Preferences and switched from Appearance to
> General. Genuine captures and privacy review are in
> [`docs/screenshots/preferences/tabstrip-fix/`](docs/screenshots/preferences/tabstrip-fix/).
> [Issue #38](https://github.com/Ding-Ding-Projects/BambuStudio/issues/38) carries
> the before/after images. The hosted
> [Windows build and release run](https://github.com/Ding-Ding-Projects/BambuStudio/actions/runs/36210592947)
> was still in progress when this note was written. Its terminal build, package,
> release, and downloadable-asset verdicts must be read from the run and release
> pages. The older sections below describe historical baselines and may predate
> this repair.

You are taking over work on **this fork of BambuStudio** (`Ding-Ding-Projects/BambuStudio`),
a Windows desktop 3D-printing slicer written in C++ with wxWidgets. This file is written
to be self-contained: it assumes you know nothing about previous sessions. Everything
below was reviewed through **2026-07-30** unless it says otherwise. The Pages dim-sum contract was
updated on **2026-08-09** to a one-in-ten startup draw backed by published public-catalog photos;
there is no opt-out, and old stored opt-out values are migrated away.

---

## 1. The 60-second summary

- The fork is **Windows-only**. macOS and Linux support was deleted from the tree.
- CI **works and publishes releases again**. The latest verified baseline before issue #16 is
  `md3-v27`, built from `efb1689d` by green hosted run `30313911327`.
- There is a **skill that launches and drives the app headlessly** on this machine:
  `.claude/skills/run-bambustudio/`. Use it for every "does it actually work" check.
- The interactive app and GitHub Pages landing now share an eleven-image WebP showcase under
  `ui-md3/assets/showcase/`; its behavior and deployment contract are documented in
  `docs/features/design-system/generated-visual-showcase.md`.
- **Testing the crash? Read §6.95 first** — it is written for the agent on the machine that actually
  has the printer, and it lists exactly what to collect.
- No open PRs and no open branches. Two open issues: #15 is waiting for the requested secret-history policy
  choice, while #16 has a complete local implementation, green focused build/tests, and cross-host
  transport evidence. Its full GUI build and English native clipping review are also complete; the
  bilingual/live-HA/hardware/remote evidence sequence remains in §7.1.
- The whole of the previous §7 to-do list is **finished** (see §5.3). Two of its five items were
  diagnosed wrongly by the previous session; §5.3 records what was actually true.
- **2026-07-30:** `master` had not compiled since the accessibility merge, and CI was separately red
  on a stale i18n tripwire — both fixed, `md3-v80` shipped. The Prepare sidebar was cutting the
  process settings off the right edge with no scrollbar able to reach them; fixed and captured
  (§6.9). **A reported crash and a "model has no data" tab failure remain unreproduced and open —
  see §7 item 0d before claiming either is fixed.** The crash reporter turned out to be disabled
  three ways over, which is why no crash ever left evidence; that is fixed and **merged**
  (`e445d1a19`, branch CI green), so the next crash writes a stack trace to
  `<data_dir>/log/crash_*.log`. **Ask the user for that file.** No open branches.

---

## 2. Machine facts you cannot guess

These cost previous sessions hours. Do not re-derive them.

| Thing | Value |
| --- | --- |
| Repo path | Resolve from the active checkout with `git rev-parse --show-toplevel`. |
| Visual Studio | VS 2022 Build Tools 17.14 at `%LOCALAPPDATA%\material-virtualbox-toolchain\BuildTools`, installed by `build.bat` (2026-09-05). The earlier VS 18 Enterprise path no longer exists on this host. |
| MSBuild | `%LOCALAPPDATA%\material-virtualbox-toolchain\BuildTools\MSBuild\Current\Bin\MSBuild.exe`. A bare MSBuild run cannot regenerate the projects (CMake needs pkg-config on PATH); regenerate through `build.bat /s`. |
| Windows SDK | The generated tree selects **10.0.26100.0** (installed by `build.bat`). |
| Prebuilt deps | `deps\build\BambuStudio_dep\usr\local`, built by `build.bat` (attempt 6, 2026-09-05). |
| App logs | `%APPDATA%\BambuStudioInternal\log\studio_*.log*` |
| App config | `%APPDATA%\BambuStudioInternal\BambuStudio.conf` (e.g. `"dark_color_mode": "1"`). **Ends with a `# MD5 checksum` line.** A stale checksum only logs a warning, but **malformed JSON makes the app silently fall back to `BambuStudio.conf.bak`** — so a botched hand-edit looks exactly like "the app ignored my setting". Edit with a real JSON serializer and recompute the checksum over everything up to and including the last `}`. |
| GPU | NVIDIA GeForce RTX 3050. The app starts on the real driver on a hidden desktop; GL canvases come back blank through `PrintWindow` on that route, so canvas captures use the Mesa pair in `install-dir\mesa`. |
| Display | The current primary display reports **1920 x 1080**. Treat this as a point-in-time host fact and recheck before drawing layout conclusions. |
| Python | The vendored venv is absent on this host. The cheap Lowlevel CLI lives in the sibling checkout at `%USERPROFILE%\Documents\GitHub\lowlevel-computer-use-mcp\.venv\Scripts`; set `LLCU_VENV` to that `.venv` for `driver.py`. Plain `py -3` runs the `scripts/md3/*.py` helpers. |

**Shell gotchas on this box** (these silently produce wrong results):

- `cmd` does **not** resolve executables from the current directory. Always call `.exe`/`.cmd`
  files by absolute path, and use `cmd /c "cd /d <dir> && call C:\full\path\to\thing.bat"`.
- Git-bash `printf` eats backslashes in the **format** string: `printf 'C:\Unescaped\...'` fails with
  "missing unicode digit for \U". Write `.cmd` files with a **quoted heredoc** (`<<'EOF'`) instead.
- Git-bash mangles `git show origin/master:path/to/file` (the colon). Use PowerShell for that,
  or `git show 'origin/master:path' -- ` quoted carefully.
- Python's `Path.write_text` converts `\n` to `\r\n` on Windows. For `.cmd` files that already
  contain `\r\n`, this produces `\r\r\n`, and the stray `\r` **poisons `set` variable values**.
  Write bytes instead.

---

## 3. How to build

### 3.1 Rebuild after editing one or a few GUI/source files (the normal case, ~10–40 min)

The real code lives in `build\src\Release\BambuStudio.dll` (~148 MB). `bambu-studio.exe` is only
a ~173 KB launcher. Rebuilding the `BambuStudio_app_gui` project pulls in everything.

The final issue #16 Release build produced a 151,299,584-byte DLL at
`2026-07-28 08:15:46 -04:00`, SHA-256
`41BB1BFC754E3184C5908E2145A93E3640D3866E59380F32EEFF7A76F418E972`.
That hash predates the GUI accessibility wave and must not be used as evidence for it. The current
accessibility DLL metadata is recorded in §6 only after the exact final rebuild completes.

Create a temporary `.cmd` with the checkout's resolved absolute path:

```bat
@echo off
cd /d <absolute-checkout-path>
"C:\Program Files\Microsoft Visual Studio\18\Enterprise\MSBuild\Current\Bin\MSBuild.exe" build\src\BambuStudio_app_gui.vcxproj /p:Configuration=Release /p:Platform=x64 /m:2 /v:minimal
```

Invoke it as `cmd.exe //d //c "<absolute-temp-cmd-path>"` and capture stdout/stderr. Git Bash
rewrites bare `/p`, `/m`, and `/v` switches, so do not call MSBuild directly from Bash. Then
**always check for compiler, linker, CMake, and MSBuild errors explicitly** and verify
`build\src\Release\BambuStudio.dll` advanced; an old DLL is not a successful current build.

### 3.2 Three build traps that will waste your time

1. **`LNK1104: cannot open file ...BambuStudio.dll`** means **the app is still running** and
   holding the DLL. Stop it first (`driver.py stop`, see §4), then rebuild. This exact error
   ended one build in this session after a 37-minute compile.
2. **Never edit source files while a build is running** in the same tree. MSBuild's FileTracker
   gets corrupted and *later* builds exit 0 while silently skipping compiles and links. If you
   suspect it, delete the target's `.obj` and rebuild, then verify the `.obj` timestamp actually
   moved.
3. After a build, verify `BambuStudio.dll`'s timestamp advanced. MSBuild can leave the thin
   `bambu-studio.exe` stale at exit 0; the **DLL** is what matters.

### 3.3 The narrow display found a real bug — read this before dismissing a layout as "just this host"

The bottom action bar rendered **"Slice pl"** and *no Print button at all*. That was written off
twice as an artefact of the 832 px screen. It was not.

`update_prepare_action_bar_content()` sized the canvas-alignment spacers to the **full** sidebar
width (344 px). Those spacers are **proportion-0** sizer items; the tool row is **proportion-1**.
When a row cannot fit every minimum, `wxBoxSizer` takes its degenerate branch
(`sizer.cpp:2253`): it pays the **fixed** items in full first (`:2257-2269`) and gives the
proportional ones only what is left (`:2274-2286`). So the spacer took its 344 px and the tool row
was truncated to the remainder — short by 214 px, which cascaded down three nested sizers and
reached the Slice pill as a **92 px** window and the Print pill as a **0 px** one.

Two things make this hard to see, and both misled this session:

- **It never looks like overflow.** wx truncates the straddling item and allocates **zero** to
  everything after it (`GetMinOrRemainingSize`, `sizer.cpp:2162-2190`), so every child still
  reports a rect *inside* the frame. Measuring the children and concluding "nothing overhangs, so
  nothing is clipped" is exactly the wrong inference — the starved control is simply gone.
- **A zero-width control leaves no trace.** The Print action was absent from every capture for
  hours without anyone noticing a button was missing rather than merely narrow.

Fixed in `MainFrame.cpp`: the spacers may claim only what the row does not need — cosmetic
alignment with the 3D canvas never outranks a primary action — plus a `BOOST_LOG_TRIVIAL(warning)`
when the row is still over-subscribed, so the next starved control says so instead of vanishing.
Before/after at 846 px: `docs/screenshots/md3-conversion/action-bar-{before,after}-starved-row.png`.

---

## 4. How to RUN and DRIVE the app (this is the important part)

There is no usable interactive desktop and no GPU on this machine. You cannot "just run it".
Use the committed skill.

```
.claude/skills/run-bambustudio/
  SKILL.md        ← read this; it documents every command and every trap
  driver.py       ← the harness
  cube.stl        ← 20 mm test cube
  popovercap.py   ← for transient popovers only
```

**To press a button or menu item, use `press.py` — press by NAME, not by pixel:**

```bash
"$PY" "$D/press.py" menus                    # every menu item + its live command id
"$PY" "$D/press.py" press "Version history"  # opens File > Version history...  (verified)
"$PY" "$D/press.py" controls --filter ink    # labelled child controls
```

This solves what the previous handoff listed as the top blocker ("no known way to open a
topbar menu item programmatically"). Menu ids are `wxID_ANY` allocations that **shift between
builds** — `Version history...` was 849 in one build and 888 in the next — so `press.py`
enumerates them live and caches per frame hwnd. Never hardcode one. Two details make it work
and are easy to break: the frame must be parked at **(-183, -6)** while discovering menus (at
its normal position the owner-drawn strip opens nothing), and **ctypes silently swallows
exceptions raised inside an `EnumChildWindows` callback**, yielding an empty list instead of
an error. Both are documented in `SKILL.md`.

Quick start:

```bash
PY="$PWD/vendor/lowlevel-computer-use-mcp/.venv/Scripts/python.exe"
DRV="$PWD/.claude/skills/run-bambustudio/driver.py"

"$PY" "$DRV" launch                                  # ~1-3 min: waits for "finished init opengl"
"$PY" "$DRV" windows                                 # find the frame (title "Untitled - BambuStudio")
"$PY" "$DRV" ss --hwnd <H> --out shot.png            # screenshot, then LOOK at it
"$PY" "$DRV" open --model .claude/skills/run-bambustudio/cube.stl
"$PY" "$DRV" ahkclick --hwnd <H> --x 975 --y 760     # click "Slice plate" (client coords)
"$PY" "$DRV" stop                                    # ALWAYS do this before rebuilding
```

**Verified working end-to-end**: launch → load cube → click Slice → the sliced Preview with the
gcode legend appears in the screenshot.

Traps the driver already handles, listed so you do not "fix" them back:

- The headless desktop **dies when its last process exits**, so the app runs in the wrapper
  `cmd`'s foreground (no `start`).
- hwnd-addressed calls **fail from the normal desktop** (`IsWindow` fails cross-desktop). Every
  such call is relayed by launching the tool *on* the headless desktop.
- **AutoHotkey never exits** without `ExitApp`, and its runtime errors open **invisible** dialogs
  on the headless desktop. Scripts must try/catch to a result file.
- `ahkclick` uses **client** coords; plain `click` uses **desktop-screen** coords (the frame sits
  at about (136, 95)). Custom wx buttons ignore plain clicks — use `ahkclick`.
- **Transient popovers die if you spawn another process on the desktop while one is open** (it
  steals focus). "Click, then list windows from another process" therefore always reports nothing,
  which reads as "the popover never opened". The click, the WinEvent catch, the repaint and the
  `PrintWindow` must happen in **one** on-desktop process — that is what `popovercap.py` is for.
  Point it at the **control's own hwnd**, not the containing panel: a raw `WM_LBUTTON*` posted to a
  panel does not fire these custom wx controls.
- **WebView2 panes never render** in captures (Home tab, Setup Wizard body come out blank). To
  see those, render the bundled page with headless Edge instead:
  `msedge --headless=new --disable-gpu --screenshot=out.png --window-size=1200,766 file:///.../resources/web/homepage3/home.html`
- Topbar menus are **custom-drawn**: AHK `MenuSelect` fails ("unsupported menu") and clicking the
  menu labels via `ControlClick` did **not** open them in this session. Opening a menu item
  programmatically is still an **unsolved problem** — see §7.

---

## 5. What changed in this session (all of it)

### 5.0.2 Session of 2026-07-28 (latest) — the UI kit stopped calling a CDN

**What was wrong.** `ui-md3/design-system/ui_kits/bambu-studio/index.html`, published at
`/app/design-system/ui_kits/bambu-studio/`, loaded React, ReactDOM and `@babel/standalone` from
unpkg and compiled its own inlined JSX in the visitor's browser on every load. Three third-party
requests and a 2.7 MB compiler on a site whose documentation opens by promising neither — and with
unpkg unreachable the page served `<div id="app"></div>` and stopped. Nothing caught it: the layout
gate's third-party assertion only ever looked at the landing page. The file's own header also
credited an "assembler" that did not exist anywhere in the tree; the `.jsx` sources and the
assembled page were kept in step by hand.

**What is now true.**

- React 18.3.1 and ReactDOM 18.3.1 UMD production builds are vendored under the kit's `vendor/`,
  with their MIT licence. No Babel ships at all.
- `ui-md3/scripts/jsx-transform.mjs` compiles the JSX at build time — a dependency-free compiler for
  the subset the kit uses, which **throws** on anything outside it rather than guessing.
- `ui-md3/scripts/assemble-ui-kit.mjs --check|--write` is the missing assembler, wired into the
  Pages workflow beside `assemble-index.mjs --check`.
- `App.jsx` now aliases `useState` to `useAppState`. Babel used to rewrite `const` to `var`, which
  hid the fact that `Components.jsx` and `App.jsx` both declared a top-level `const { useState }`.
  Compiled as real `const` in two classic scripts that share a global scope, the second is a
  `SyntaxError` that kills every script after it. The assembler now fails the build on any such
  collision.

**Evidence.** All twelve sources compile to a program **byte-for-byte identical to Babel's own
output** (verified by printing both through Babel's printer). Driving both builds through fourteen
states — the initial render, all nine workspaces, the Print-plate dialog, its dismissal, and the
version-history drawer — the compiled page **with the network cut** produced DOM identical to the
old CDN page **online**, in all fourteen. New regression gates: `assert-pages-layout.mjs` now sweeps
every published page rather than the root, and `ui-md3/tests/offline-render.test.mjs` loads the
composed site in headless Chrome with every off-site host blackholed. Both fail on the pre-fix page
and pass on this one. Full local run: 77 static cases, 11 transform cases, the 444-case runtime
suite, and the offline suite — all green.

Documentation: [`docs/features/pages/deployment-and-layout-gate.md`](docs/features/pages/deployment-and-layout-gate.md)
and the kit's own README.

### 5.0 Session of 2026-07-28 — the GitHub Pages site was rebuilt

**What you are inheriting.** `https://ding-ding-projects.github.io/BambuStudio/` is no longer a
single scrolling landing page. It is a tabbed static application built from
`ui-md3/landing.html` + `ui-md3/site/`, and it now carries the same obligations as the desktop app.
Full documentation: [`docs/features/pages/`](docs/features/pages/README.md).

| Commit | What |
| --- | --- |
| `f3ff11044` | Rebuilt the site: eight browser-style tabs, bilingual copy at five funny levels per language, the shared regex builder, the changelog viewer over 34 real releases, notifications, settings, and the original dim sum surprise. |
| `aea1327cd` | Gated the deploy on 444 measured runtime layout cases; replaced the workflow's inline `rsync`/`python3 -m http.server` with `compose-site.mjs` and `serve.mjs`; updated `i18n.test.mjs` for the new shape (this was issue #25). |
| `2b2b7ce45` | Fixed 29 defects confirmed by a twelve-agent adversarial review of the new site. |
| `8f4dba64e` | Fixed the prototype's eight defects from issue #24: 143 icon spans made decorative, `role="switch"` on preference toggles, real dialog semantics in `app/dialogs.js`, a title bar that no longer clips its window controls, and all ten search fields wired. |
| `beeb8703a` | Restored case-sensitive regex search: `SearchField.searchFlags()` returns filter-ready flags and an empty string verbatim, so a consumer can no longer substitute `'i'` for "no flags". |
| `30d3f884e` | Title-bar collapse rules given `!important` (the prototype's inline styles beat them otherwise) and `capture-app.mjs` added. |
| `bfd87cafa` | Removed the changelog freshness gate — every release CI publishes made the committed file stale and would have blocked the next Pages deploy. It is regenerated at deploy time now, with the committed file as the fallback. |
| `b6718eac0` | Renamed the site and prototype's material vocabulary to **ink** / **Ink Dispenser** (display text only), and repaired the Cantonese the English-side rename had silently broken. |
| `65fcd2bc0` | Retook all 23 captures; `capture-app.mjs` was still querying `input[placeholder="Search filaments"]`. |
| `5340bd466` | Reworked the `release` Pages trigger, which had failed **12 times out of 12** and never once done what it claimed. Added the first test that reads the workflow. **Still unverified** — see §5.0.1. |
| `d61e4e47f` | Gave the release dispatcher its own concurrency group so it cannot cancel a deploy and then replace it with nothing. |
| `0b900b73b` | Renamed the **published MD3 UI kit** at `/app/design-system/` — 61 matching lines across eight files that three earlier terminology passes had all walked past. |
| `d9159322e` | Widened the sweep from the kit to the whole design system; the typography specimen page used real product strings as its samples. |
| `da17ee1a7` | Fixed everything a twelve-agent adversarial review found: a GitHub behaviour I had documented backwards, a guard that exempted whole lines, a sweep reading two of five published extensions, and workflow assertions that were comment-satisfiable and vacuous on CRLF. |

**Issues closed this session:** #25 (the `i18n.test.mjs` assertion that broke master — fixed before it
was filed) and #24 (the prototype's eight defects, each closed with measured evidence). #16 and #15
are untouched and remain the concurrent session's.

#### 5.0.1 All three event paths are verified — and why the run history looks otherwise

| Path | Evidence |
| --- | --- |
| `push` → deploy | **Verified.** Many green runs, most recently `da17ee1a7`. |
| `workflow_dispatch` → deploy | **Verified.** Run `30404583829`: `deploy` ran, `redeploy-on-release` **skipped**. |
| `release` → dispatch → deploy | **Verified** by `md3-v62`, tagged at `d9159322e`. |

The release path proved out like this, and it is the only release run in the repository's history
that has not failed — 17 release runs, 16 failures, 1 success:

```
00:09:57  release md3-v62 (tag at d9159322e, contains the fix) -> run 30410305947  success
            redeploy-on-release : success, 3 steps
            deploy              : skipped
            log                 : "Dispatched a master-ref Pages deploy for md3-v62."
00:10:07  workflow_dispatch at master -> run 30410314986  success (deployed)
```

**Why the Actions tab is full of red release runs anyway**, and the trap to inherit: **a `release`
event runs the workflow file as it existed at the tag's commit.** Releases tagged at commits older
than `5340bd466` run the *old* workflow and fail the old way — zero steps, ~2 seconds, no log — no
matter what `master` says. `md3-v58` (`6d1ad69de`), `md3-v59` (`2dd74cfef`), `md3-v60`
(`a00319851`) and `md3-v61` (`a90d72989`) all did exactly that *after* the fix landed. The same rule
explains the original 12-of-12: eleven releases published while `master` carried the `release:`
trigger produced zero release runs, because their tag commits predated the trigger.

So before treating a red release run as a regression, check whether its tag predates the fix:

```bash
gh release view <tag> --json targetCommitish
git merge-base --is-ancestor 5340bd466 <sha>   # exit 0 = post-fix = should have dispatched
```

**Three claims in this file were wrong earlier and are worth knowing as a pattern**, because the
same mistake recurred four times: a check that was sound about what it read, wrapped in a claim
written wider than what it read. "No user-facing filament remains" came from grepping one file;
"72/72 published files clean" came from an audit filtered to three of the seven published
extensions (it never opened the `.jsx` that was shipping the label `Filament`); the file counts in
the correction to that were estimates rather than counts. The guards now strip identifiers and
re-test the residue instead of exempting whole lines, pin real tree sizes instead of floors, and
every number is counted. The full record, including the corrections, is
[discussion #22](https://github.com/Ding-Ding-Projects/BambuStudio/discussions/22).

**Things that will bite you if you do not know them:**

- **The tab strip must not debounce with `requestAnimationFrame`.** A page that is never painted —
  a background tab, a headless capture, the deploy gate — never runs rAF callbacks, so the strip
  would stay frozen in its pre-font-load state, which is "everything overflowed". It uses a timer.
- **No `text-overflow: ellipsis` and no horizontal scroller anywhere in `ui-md3/site/`.** The
  runtime gate fails any element whose `scrollWidth` exceeds its width, and both of those hide a
  clip rather than fix it. Long strings wrap.
- **Where the prototype lives is stamped, not sniffed.** `compose-site.mjs` rewrites
  `<meta name="bambu-app-base">` to `app/` for the published tree. A `github.io` hostname test got
  the local preview wrong — which is exactly the copy the layout gate serves.
- **`ui-md3/index.html` is generated.** Edit `app/screens/*.template.html`, then run
  `node ui-md3/scripts/assemble-index.mjs --write`. `--check` runs in the Pages workflow.
- **`site/changelog.data.js` is generated**, and deliberately **not** gated on freshness. CI
  regenerates it at deploy time from the Releases API with the committed file as the fallback.
  Gating on staleness looked tidy and was a trap: every release CI publishes makes the committed
  file stale by definition, so the next Pages deploy would fail until a human regenerated it.
- **The prototype's collapse rules need `!important`.** Every element in `ui-md3/index.html`
  carries an inline `style="display:flex"`, and an inline style beats a stylesheet rule without it.
  A responsive rule that looks correct in the diff can do absolutely nothing.
- **`ui-md3/index.html` is stored with CRLF.** A search-and-replace whose pattern spans two lines
  will silently never match. Prefer single-line edits, and verify the result rather than the diff.
- **The `github-pages` environment on this fork accepts deployments only from `master`**, and a job
  gated on that environment at any other ref is rejected *before its first step* — three seconds,
  zero steps, no log, conclusion `failure`. That is what made the `release` trigger fail 12 for 12
  without anyone noticing: a `release` event runs at the **tag** ref. Anything that must deploy off
  a non-`master` ref has to dispatch a `master` run instead, which is what `redeploy-on-release`
  does. `ui-md3/tests/site.test.mjs` now asserts that shape.
- **A `release` event runs the workflow file as it existed at the TAG's commit**, not as it exists
  on `master`. This is the part that makes the run history confusing: fixing a release-triggered
  workflow does **nothing** for releases whose tags point at older commits, and they keep failing
  the old way until they drain. `md3-v58` (tagged `6d1ad69de`) and `md3-v59` both failed exactly
  that way *after* the fix landed. The same rule explains the earlier gap: eleven releases published
  while `master` carried the `release:` trigger produced zero release runs, because their tag
  commits predated it. So the fix is only proven once a release tagged at a commit **containing**
  it publishes — until then, treat it as unverified.
- **`GITHUB_TOKEN` CAN dispatch a workflow.** An earlier version of this file said the opposite;
  that was wrong. GitHub's recursive-trigger prevention explicitly exempts two events:
  `workflow_dispatch` and `repository_dispatch` "always create workflow runs", even when signed
  with `GITHUB_TOKEN`. The job needs `actions: write`, which is the real requirement. `TOKEN_GITHUB`
  is the owner PAT this repository has (`RELEASE_TOKEN` and `ORG_TOKEN` are org-convention names it
  does not define at repository scope), and it leads the chain only so a dispatch is attributed to
  the owner rather than to `github-actions[bot]`.
- **The material vocabulary is display-only.** `ink` and `Ink Dispenser` are what a user reads;
  `filamentRows`, `?view=filament`, `.bbsflmt` and the native `.po` msgids keep upstream spelling
  because bindings and file formats match on them. But `ui-md3/app/i18n.resources.js` is the
  exception that will catch you: it is keyed on the **rendered English string**, not on a msgid, so
  renaming display text without renaming its keys makes every lookup miss and fall back to English
  — silently, with nothing anywhere reporting a problem.

**How to verify the site locally** — see
[`docs/features/pages/deployment-and-layout-gate.md`](docs/features/pages/deployment-and-layout-gate.md).
The runtime suite needs Chrome or Edge and takes about three minutes.

### 5.1 Pushed to `master` (already live)

| Commit | What |
| --- | --- |
| `e2d2f4566` | **CI fix.** `scripts/ci/Test-WindowsNativeVisual.ps1` contained raw Cantonese text, but `scripts/ci/Test-BuildFromSourceHelpers.ps1` deliberately parses that file under Windows PowerShell 5.1's ANSI decoding *and* asserts it is byte-level ASCII. Every CI run failed with a ParseException at lines 307–308. The CJK strings are now assembled from explicit code points; output is byte-identical (verified by comparison). |
| `42f7c097b` | **CI fix.** Commit `fa0f0d6ce` added tests using `Slic3r::GUI::DeviceWeb::LatestRequestGate` but never committed the header. Every build died with C1083. Header reconstructed from the tests' contract and verified by compiling + running both test scenarios standalone. |
| `e429048f2` | Replaced blank README/wizard screenshots with genuine captures. Refs issue #5. |
| `2bc2131dc` | Added the `run-bambustudio` skill described in §4. |

**Why releases had stalled:** every run after `md3-v10` failed on the two bugs above. `md3-v11`
shipped an *old* commit simply because an older queued run finished last — the workflow's
supersession labelling was correct, nothing was mixed up. After the fixes, `md3-v12`, `v13`,
`v14`, `v15` all published. **`md3-v14` is Latest.**

### 5.2 On branch `windows-only-and-recovery-hardening` → **PR #13** (CI-green, unmerged)

| Commit | What |
| --- | --- |
| `b365c13f5` | **The fork is now Windows-only.** ~4,200 deletions. |
| `450077be1` | FadeIn hardening + documentation. |
| `477569225` | Restored a CI job that commit `b365c13f5` accidentally deleted. |

**Windows-only removal, in detail** — deleted: `BuildLinux.sh`, `BuildFedora.sh`, `BuildMac.sh`,
`DockerBuild.sh`, `DockerEntrypoint.sh`, `DockerRun.sh`, `Dockerfile`; `src/platform/osx/` and
`src/platform/unix/`; all 10 Objective-C++ `.mm` files; the macOS Homebrew deploy workflow; every
macOS/Ubuntu step in `build_bambu.yml` and `build_deps.yml`; mac/linux branches in four
CMakeLists files; the `SLIC3R_FHS` option and its generated header; GTK / webkit2gtk / GStreamer
/ Wayland / DiskArbitration wiring. `CMakeLists.txt` now **fails immediately** if configured on a
non-Windows system. `src/BambuStudio.cpp` resolves the resources dir directly instead of through
a four-way platform `#ifdef` chain.

**Deliberately NOT done:** `__APPLE__` / `__linux__` blocks *inside* shared source files remain
(~200 files). They compile out on Windows. Removing them is a separate, riskier sweep with no
functional gain. Do not start it casually.

**The FadeIn fix** (`src/slic3r/GUI/Widgets/MD3Motion.cpp`): `FadeIn` applied `WS_EX_LAYERED` with
**alpha 0** and depended entirely on a `wxTimer` to raise it. If that timer never runs, the window
stays fully transparent **while remaining modal and still consuming input** — which is exactly
what the two user reports ("Ctrl+F palette cannot be closed", "regex builder does not pop up")
look like from outside. Entrances now start at a 25% alpha floor, and if `wxTimer::Start` fails
the window jumps straight to opaque. **This is a robustness fix, not a confirmed root cause.**

**The CI job that got deleted and restored** — worth understanding, because it is how you know a
CI run is real: the job graph is
`build_all.yml` → `build_check_cache.yml` (*Check Cache*) → `build_deps.yml` (*Build Deps*) →
`build_bambu.yml` (*Build BambuStudio*). The `build_Bambu` job at the tail of `build_deps.yml` is
the link between the last two. When it was accidentally removed, the run showed *no application
build at all* and Publish failed on an installer that had never been built. **If you ever edit
these workflows, re-check that `Build BambuStudio` still appears in the job list.**

### 5.3 Session of 2026-07-27 (overnight) — §7's list is now finished

Everything the previous §7 listed is done. What it said was wrong in two places; both are
corrected below, because acting on the old text would waste hours.

| Item | Outcome |
| --- | --- |
| 1. Merge PR #13 | Already merged (`29902b4aa`) before this session. |
| 2. Dark-mode Version-history labels | **Fixed — but the diagnosis was wrong.** See below. |
| 3. Crash-backup preservation | **Verified live end-to-end**, including the Cancel branch. |
| 4. FadeIn hypothesis | **Refuted.** Both surfaces open fully opaque. |
| 5. Issue #5 blank crops | **All 14 gizmo crops were blank**, not 2. Recaptured; issue closed. |

**Item 2 — the labels were never the problem.** Pixel-sampling a live capture showed the two
"black-on-white" labels painting correctly dark (`#202127`) while the **`StaticBox` card underneath
them** painted `#F0F0F0`. Two stacked causes, both now fixed in the widget so every themed card in
the app benefits:

- `StateColor::setColorForStates()` only **updates** a state entry that already exists and returns
  `false` otherwise. `StaticBox`'s constructor seeds only `border_color`, so
  `SetBackgroundColorNormal()` was a **silent no-op on every card without an explicit
  `SetBackgroundColor()`**, and `doRender()` fell through to its `count()==0` fallback.
- That fallback fills with the plain `wxWindow` background, which `Create()` seeds once from the
  parent — the light surface for any card built before a theme is applied. `SyncWindowBackground()`
  now keeps it in step.

See `docs/features/design-system/themed-surface-colors.md`. Before/after captures are committed.

**Item 4 — refuted, and two harness traps explain the reports.** The Ctrl+F palette (642x502) and
the regex-builder popover (393x608) both open opaque and fully populated. What made them *look*
absent: a raw `WM_LBUTTON*` posted to a panel does **not** fire these custom wx controls (post to
the control's own hwnd, or use `ahkclick`), and **any process spawned on the headless desktop while
a popover is open focus-kills it** — so "click, then list windows from another process" always
reports nothing. That is exactly why `popovercap.py` exists.

The capture also caught a real defect, now fixed: every regex-builder flag row drew its text twice
(clipped ghost text inside the 44 px checkbox plus the real label), because `CheckBox` is a
`wxBitmapToggleButton` — a native MSW `BUTTON` — and `addFlag()` called `SetLabel()` on it.

**Item 5 — the scope was bigger than recorded.** All 14 gizmo crops were bare rail background
(min luminance 178, zero dark pixels), not just two. Also, two crop names are **aliases of one
gizmo each**: `color-paint` == `mmu-segment` and `support-paint` == `fdm-support`, which is why
those two looked like the only casualties. Recaptured in **light mode** (matching the rest of that
matrix) with a model loaded, since the rail only renders with an object in the scene. A sweep of
all 241 committed captures now reports zero blank images. Issue #5 closed.

**Item 3 — verified, and the fixture recipe is worth keeping.** Load a model, wait for the backup
`.3mf`, hard-kill the process, delete the stale `lock.txt`, point `app/last_backup_path` at that
directory, relaunch, click **Cancel**. Result: the backup directory is deleted and the
`Recovered unsaved project` commit survives carrying the identical 8662-byte `.3mf`. Two traps cost
real time here and are documented in `docs/features/workspace/project-version-history.md`: a
dead-pid `lock.txt` makes `has_restore_data()` return false from its `catch (...)`, and a
**hand-edited `BambuStudio.conf` with malformed JSON is silently ignored in favour of
`BambuStudio.conf.bak`** — the file ends with an MD5 checksum line, so edit it with a real JSON
serializer and recompute the checksum.

### 5.4 Session of 2026-07-28 — bug + clipping sweep, and the MD3 stock-UI purge

Two audits (38 and 42 agents), four fix waves, every patch adversarially reviewed. All of it is
pushed and ancestry-proven. **The full GUI Release build is clean** and the app runs on it.

**Crash recovery had four defects, and the previously recorded diagnosis was wrong.**
`docs/features/workspace/project-version-history.md` blamed `has_restore_data()`'s `catch (...)`.
Probing Win32 directly disproved that: `OpenProcess` on a free pid returns **`NULL`** (error 87),
not `INVALID_HANDLE_VALUE`, so for a dead pid the name comes back empty, the comparison does not
match, and the `catch` is never reached. What was actually wrong:

- the sentinel guard tested the wrong value, so a null handle reached `GetModuleFileNameEx` and
  then `CloseHandle`;
- Windows **reuses freed pids**, so relaunching after a crash could hand the new instance the
  crashed one's pid — the app then compared itself against itself, concluded another instance
  held the backup, and silently offered nothing. This is the likeliest explanation for the
  2026-07-27 observation;
- `load_string_file()` sat outside the `try`, so an unreadable lock threw out of
  `has_restore_data()` into the startup handler;
- **worst:** `Plater` discarded `preserve_unsaved_backup_in_history()`'s bool, so when preserving
  failed its "stays restorable" snackbar never fired, the user read the ordinary prompt, clicked
  Cancel, and `remove_all()` ate the only copy.

> [!IMPORTANT]
> **A severity claim was withdrawn.** The sentinel bug was first written up as crashing the app
> under strict handle checking. That was reasoned, not measured — and measuring it did not support
> it: a probe ran the old and the fixed guard under `ProcessStrictHandleCheckPolicy` and **both
> survived**, as did a control that closed a garbage non-null handle, proving the policy was never
> armed. The regression test built on that probe could not fail either way and was **removed**
> rather than left green. The sentinel fix is correctness and hygiene, not a crash fix.
> `tests/libslic3r/test_crash_restore.cpp` now records which of its cases actually discriminate.

**Fourteen more native defects** were confirmed by adversarial verification and fixed: two
`FilamentScanner` use-after-frees (a stack-allocated modal dialog with a detached 180-second
thread posting `CallAfter` on a raw `this`), an invisible keyboard focus ring on every dialog's
default action (`Primary` on a `Primary` fill), ~1.33:1 snackbar contrast, `StaticBox` flooding its
own rounded corners so every pill drew as a rectangle, `CheckBox` glyphs baked at construction,
`StateColor` missing a dark pair for `Surface`, a colour picker `Fit()` before its label had text,
a resizable dialog with **no visible close control**, a non-wrapping label truncating the real
libgit2 cause, and a command palette scrolling 52px against a 53px row pitch until the selection
left the viewport entirely.

**MD3 stock-UI purge.** The parity register said all 128 gaps were done. A fresh six-lens audit
found **33 more across 26 files, three contradicting rows marked `done`**. 34 were closed across
19 files. `FanControl` was the worst: 1161 lines with **zero** `MD3::Role` references, and its fan
toggles were PNGs in a `wxStaticBitmap` — not controls — so that popup was **mouse-only** with no
role, name or state. Row `gizmo-rail-svg-icons` is now correctly marked **partial**.

**Two conversions were reverted on principle**, and both reverts matter more than the conversions:

- the Smart Home volume control kept its native `wxSlider`, because the MD3 `Slider` could not be
  reached by Tab and had no `wxAccessible`. `Slider` has since been fixed (§7 item 2);
- `2DBed`'s X/Y axis arrows went back to pure red/green. Axis colours are **exempt data**, and the
  3D gizmo still draws pure RGB, so the conversion would have desynced the 2D preview from the 3D
  scene it mirrors.

Also: the release codename roster grew from 97 to 217 Hong Kong dishes (styles 40 → 71), append-only
and enforced by `scripts/ci/Test-ReleaseCodenames.ps1` — codenames are assigned **by index**, so an
insertion renames every later release and contradicts published immutable ones. And chocolatey's
third-party downloads now retry, after a SourceForge timeout failed a whole Windows build with zero
compile errors.

### 5.5 Earlier session — how the two features above were built

- **Crash-backup preservation** (`Plater::priv::preserve_unsaved_backup_in_history`, in
  `src/slic3r/GUI/Plater.cpp`): when the app starts and finds an unsaved crash backup, it commits
  that backup to the local Git-backed project history **before** showing the "restore your last
  unsaved project?" prompt, because declining the prompt runs
  `boost::filesystem::remove_all` on the backup directory. **Now verified live — see 5.3 item 3.**
  - The snapshot is staged under a real `.3mf` filename because the backup file is literally
    named `.3mf`, which has *no extension* by path rules, and the engine validates extensions on
    both the identity path and the snapshot path.
  - The commit future is `.get()`-ed because **that future carries the only error report** —
    dropping it hides failures completely.
- **ProjectHistoryDialog dark mode** (`src/slic3r/GUI/ProjectHistoryDialog.cpp`): `apply_theme()`
  re-seeds label backgrounds as well as foregrounds, because `Label`'s constructor caches its
  parent's background colour and the dialog builds its layout before any theme is applied. That
  fix is correct and still needed — but it was **not** what caused the remaining light plates.
  Those were the `StaticBox` bugs in 5.3 item 2. The previously suspected `WM_CTLCOLORSTATIC`
  explanation was wrong; do not go looking for it.

---

## 6. Current state of the world

```
branch:          fix/gui-accessibility-wave; 13 feature commits plus the current native repair are
                 still branch-only. origin/master remains the integration baseline until final push.
local build:     the post-key-pair focused Release GUI library compile and full
                 BambuStudio_app_gui link exit 0 with only the existing C4099/LNK4098 warnings.
                 DLL 150,811,136 bytes, 2026-07-30 00:44:51 -04:00, SHA-256
                 1EECBBFFBB5AB87AF2A90050220E3B4A93E816291F5C29DF4276078CABF22530.
runtime smoke:   exact-final-binary Lowlevel MCP verification is pending. Older intermediate captures
                 are not proof; one file named as sliced still visibly says "Not sliced".
local tests:     all three native accessibility contracts pass; DeviceWeb accessibility/behavior,
                 changed-file lint, TypeScript, Vite, owned-web/MD3, and 726 native / 184 DeviceWeb /
                 168 legacy localization checks passed earlier in this delivery branch.
open issues:     #16 (HA handover evidence pending). #15 was refused and closed as not planned because
                 it explicitly requested retaining secret material in Git history.
open PRs:        none
```

### 6.1 GUI accessibility delivery evidence (2026-07-30)

- `SwitchBoard` exposes one grouping object with two radio-button children, reports selected and
  enabled states through `wxAccessible`, and preserves the existing `1 = left` / `0 = right`
  asynchronous command contract with the real control ID and event object.
- Its minimum size is measured from both translated labels; representative Safety/Print/AMS/Status
  callers can grow instead of clipping against legacy maximum widths.
- Arrow keys and Home/End select endpoints immediately. Space/Enter/Numpad Enter arm once and commit
  only on the matching key-up; focus loss clears the armed key. This prevents OS key repeat and a
  mismatched key release from alternating the choice or emitting duplicate commands.
- The Ink Dispenser settings gear uses the shared focusable Button command event; the stale mouse
  overload that caused the full Release unresolved external has been removed.
- The maintained contracts are `native_shared_controls_accessibility_contract`,
  `native_gui_accessibility_contract`, and `native_accessibility_contract`; all three pass. The
  repaired `libslic3r_gui` project compiles and the full Release app links through MSBuild 18.7.8
  with `/m:2`.
- The exact post-key-pair DLL is **150,811,136 bytes**, timestamped
  `2026-07-30 00:44:51 -04:00`, SHA-256
  `1EECBBFFBB5AB87AF2A90050220E3B4A93E816291F5C29DF4276078CABF22530`. Lowlevel MCP captures,
  default-branch integration, remote ancestry proof, and hosted workflow/release state remain in the
  current-state block above. Do not reuse an intermediate hash or a capture whose visible state
  contradicts its filename.

### 6.2 Two machine limits that will bite you

- **`MSBuild /m` (unbounded) runs this box out of memory.** A parallel GUI build died with
  `C3859: Failed to create virtual memory for PCH` and `C1076: compiler limit: internal heap
  limit reached` — 220 of them — while agent processes were also running. `/m:2` completes.
  Neither error is a code error; do not go looking for one.
- **A full GUI build takes ~2.5 hours, so do not use it as a syntax check.** Compile a single
  file with the real settings instead:

  ```
  MSBuild build\src\slic3r\libslic3r_gui.vcxproj /t:ClCompile /p:Configuration=Release
    /p:Platform=x64 /p:SelectedFiles="<abs path>.cpp" /p:DebugInformationFormat=None
  ```

  `DebugInformationFormat=None` matters: without it two `cl.exe` racing on the shared
  `libslic3r_gui.pdb` fail with `C1041`, which looks exactly like a real error and is not.

This handoff records local implementation evidence; exact pushed revisions, hosted runs, and
release verdicts are maintained in
[issue #16](https://github.com/Ding-Ding-Projects/BambuStudio/issues/16). The repository convention
remains that completed work lands on `master` and every push builds and publishes a release. A remote
`codex/windows-reinstall-backup-20260726-174428` branch contains an explicit WIP snapshot with a
unique commit; retain it unless its work is reviewed and safely integrated—do not delete it merely
to make the branch list look tidy.

---

## 6.9 Session of 2026-07-30 — master did not compile, and the sidebar ate the process settings

**`master` had not compiled since the accessibility merge.** `SwitchButton.cpp` defined
`SwitchBoard::Accessible`, `on_key_down()` and `activateSegment()` that the header never declared —
16 errors, all in that one file. A concurrent agent pushed a fuller fix (also adding
`AcceptsFocus`/`AcceptsFocusFromKeyboard`, `MSWWindowProc`, `DoGetBestSize`) while this session was
working, so the redundant local commit was dropped and the tree reset onto theirs. Verified by a
clean local build and by CI publishing **`md3-v80`** from the identical tree.

CI was *also* red for a second, unrelated reason that never reached the compiler:
`scripts/i18n/Test-LanguageModes.ps1` pinned DeviceWeb English resources at **178** while the tree
ships **184**. The six new keys are present and translated in both locales with matching keys and
placeholders — a stale tripwire, not a resource defect. Already fixed upstream too.

> [!WARNING]
> Two `Windows build and release` runs failed at **`Test Windows release inputs`**, *before*
> `Build slicer Win`. So CI never reached the compile break at all, and a green pre-build gate is
> not evidence that the tree compiles. Check which step failed before concluding anything.

**The Prepare sidebar was cutting the process settings off at the right edge.** The full process
tree is the settings-tab layout reparented into a 344 dip sidebar; its option rows are label + value
field and neither half reflows. Measured live: the `Layer height` row lays out **1234 px wide inside
a 348 px sidebar**. The body was created `wxSHOW_SB_NEVER` for the horizontal bar with an x-scroll
rate of `0`, so the clipped values were not merely off-screen — **nothing could scroll to them**.

The header row above it had failed the same way the Print button did (§3.3): over-subscribed, so
`wxBoxSizer` paid the fixed items in full and handed **zero** to what straddled the boundary. The
`Process` title and the Compare-presets button were *absent*, not clipped. This is now the second
time that failure mode has cost this project a visible control — when a row looks cramped, measure
the children's widths before assuming everything is merely narrow.

Fixed in `Plater.cpp` / `Plater.hpp`, all verified live at 846 px on the real Release build
(`2421f9268`, plus the grow-only follow-up):

- `update_sidebar_scroll_body()` grows the **virtual width** when content genuinely cannot compress,
  and the body has a real horizontal scrollbar to grow into. Anything that *can* reflow still gets
  the client width, so the compact cards are unchanged. A re-entrancy guard was added because
  `SetVirtualSize()` can add/remove a scrollbar, resizing the client area and re-entering the helper
  through the sidebar's own `EVT_SIZE`.
- `Plater::request_sidebar_width()` widens the dock to 480 dip in Advanced mode — **weakly**: capped
  at 55% of the frame, never below the density default, `grow_only` so a sidebar you dragged wider is
  left alone, and the sash stays draggable with the dragged width persisted by the existing idle
  handler. It shrinks back only on the explicit flip to Simple.
- The width is re-asserted on the first **laid-out** size event. The `priv` ctor runs before the
  frame has a width, so a request made there clamps to the compact default; the function returns
  `false` while the frame is too small to size against, and the caller retries instead of latching.
- Advanced mode gained its own settings-search pill on the **Simple settings** bar (same
  `OptionsSearcher` and regex builder as the compact card's, whose field is hidden with the card).
- **Object manipulation now starts hidden** and appears on selection. With nothing selected it was
  twelve en dashes under a header, costing a screenful of sidebar height.

Evidence: `docs/screenshots/sidebar-process/` (before/after pairs), documented in
`docs/features/prepare/process-settings-sidebar.md`. The 3D canvas starts at **x=348** before and
**x=461** after; `Object manipulation` is absent from `press.py controls` until something is selected.

> [!IMPORTANT]
> **Two of the reported symptoms are NOT fixed and were not reproduced.** See §7 item 0d.

**Watch out — two shadowing traps in `Plater.cpp` cost two build cycles here.** `Plater::priv::priv`
takes a parameter named **`q`** that shadows the member `Plater *q`, and the AUI block declares a
local `auto &sidebar` that shadows the member `Sidebar *sidebar`. A lambda in that scope must reach
both through `this->`, or you get `C3493: cannot be implicitly captured`.

**Also worth knowing:** `press.py controls` only enumerates *labelled* children, so custom-drawn
controls (the `Global`/`Objects` `SwitchButton`, search-field placeholders) never appear — their
absence from that list is not evidence they are missing. Crop the capture instead.

---

## 6.95 READ FIRST IF YOU ARE ON THE MACHINE WITH THE PRINTER

The 2026-07-30 session could not reproduce the reported crash **because this build host has no
printer bound and no network plugin**, and every strong suspect it found lives in code that only
runs when those exist. If you are the agent on the user's other machine, you can settle in ten
minutes what cost that session a day.

**Collect these, in this order:**

1. **`crash_*.log`.** The crash reporter was disabled three ways and is now merged and CI-green
   (`e445d1a19`). After the next crash, get
   `%APPDATA%\BambuStudio\log\crash_*.log` (release build) or
   `%APPDATA%\BambuStudioInternal\log\crash_*.log` (internal build). It carries the exception code,
   registers, loaded modules and a **call stack**. This single file replaces all the guesswork below.
2. **Which binary.** Installed release (which `md3-v*` tag?) or a local build? A release installer
   uses data dir `BambuStudio`; an internal build uses `BambuStudioInternal`. The 07-30 session
   found **no logs at all** on the days the user reported crashing, which is why it suspects the
   crashes happen on a different machine or build than the one it could test.
3. **Network plugin + sign-in state.** Help ▸ check the plugin, and whether the user is signed in.
   This matters more than it sounds — see below.

**The leading theory, and what makes it testable there:**

`GUI_App::getAgent()` returns `m_agent`, which is **only ever constructed under
`if (create_network_agent)`**. If the network plugin fails to load, it is null for the *entire
session*. A sweep of all 155 `getAgent()` call sites found **8 that dereferenced it unchecked**, all
in device/media/model-mall code — the live-view camera URL, the go-live camera URL (on the HTTP
server thread), LAN bind detect, and the model-rating flow. Two run on non-UI threads, where a null
dereference is an access violation with no handler and nothing in the log. Fixed in `4e31b2e6d`
and `7e1ebbf28`; the audit now reports zero unguarded.

Every one of those needs a **bound printer** to reach, which is exactly why a host with none sails
past them. The user's config has a `Bambu Lab X1 Carbon`.

The same null agent is also why the Ink/Device tabs read **"No Data"** — that string is the Filament
Manager's empty state in the DeviceWeb locales (`en.json:59`, `SpoolTable.tsx:269`), rendered when
there is no agent. **One root cause would explain both reported symptoms.** Confirming the plugin
state is therefore the highest-value single check on that machine.

> [!WARNING]
> **Do not report any of this as "the crash is fixed" without a stack trace or a reproduction.**
> This session already had to withdraw one reachability claim (§7 item 0d) for being reasoned
> rather than measured, and §5.3 records an earlier severity claim withdrawn for the same reason.
> The fixes are real; their connection to the user's specific crash is not established.

> [!NOTE]
> **Build state:** commits from `95fd064c0` through `4e31b2e6d` were pushed with local build
> verification **incomplete** — the local rebuild was stopped in favour of CI at the user's
> instruction, after a CMake change forced a full libslic3r rebuild. Confirm the CI runs on master
> from 2026-07-31 are green before building on top of them. Touched: `Plater.cpp`, `GUI_App.cpp`,
> `MediaPlayCtrl.cpp`, `HttpServer.cpp`, `ReleaseNote.cpp`, `StatusPanel.cpp`.

---

## 7. What to do next

0d. **THE CRASH ITSELF IS STILL OPEN — start here.** It was not reproduced, so it is not fixed.
   Two of the three things reported around it *are* addressed: the app no longer refuses to reopen
   afterwards (`bbcf1630b`, below), and a crash will finally leave a stack trace once
   the crash reporter is merged and green. The crash itself has not been found. The user
   reported, in their words: *"it keeps crashing … when opening model or changing a lot of settings
   at the same time"*, *"when it crashes it refuses to open until i open it a few times"*, and
   *"switching tabs do not work and say model has no data"*.

   **Why there was never any evidence — this is the actionable finding.** The crash reporter exists
   in this tree and was switched off in *three independent ways*:
   - `SET_DEFULTER_HANDLER()` commented out in `bambustu_main()` (`src/BambuStudio.cpp`), for both
     release and internal builds;
   - `CBaseException::set_log_folder(data_dir())` commented out (`GUI_App.cpp`), so the filter had
     nowhere to write even if installed;
   - `src/BaseException.cpp` and `src/StackWalker.cpp` were **in the tree but compiled by no
     target**, so uncommenting either line alone only earns a link error.

   That is why a crash left no dump, no stack and no marker: the process simply stops mid-line,
   which is indistinguishable from being killed.

   > [!IMPORTANT]
   > **All three are now enabled and MERGED** (`e445d1a19`). The work went to a branch first
   > precisely because those legacy files had never been compiled here; branch CI run
   > `30589807507` came back **green** (built, linked, release published), so the merge rests on
   > evidence. **A crash now writes `<data_dir>/log/crash_<when>_<n>.log`** with the exception
   > code, registers, loaded modules and a call stack. It does **not** stop the crash — it makes
   > the next one diagnosable. **Ask the user for that file.**

   **What was already ruled out here (do not redo):**
   - Opening `cube.stl`, slicing, and Preview all work. **32 tab switches** across
     Prepare/Preview/Device: clean. **10 rounds** of advanced/simple flips plus every segment
     (Quality/Strength/Support/Others): clean. **12 modal open/close cycles** (`Version history`)
     with a model loaded, to fire the sidebar's 250 ms timer inside nested modal event loops:
     clean. `procdump -e -ma -w` attached throughout produced **no dump**, and both app instances
     stayed alive every time.
   - Racing the **background slicing worker** against config changes (8 rounds of Slice-plate
     followed immediately by category switches, no wait): clean.
   - Loading a **dual-filament 3MF** (`resources/calib/pressure_advance/auto_pa_line_dual.3mf`) —
     chosen because it forces a filament-count change *and* a whole-config apply at once, the
     closest thing to "changing a lot of settings at the same time": clean.
   - Testing constraint worth knowing: **this box supports only two concurrent app instances.** A
     third dies pre-log at the GL gate (`bs-out.txt` empty, no studio log, no process) because two
     llvmpipe contexts already exhaust software GL here. `driver.py open` spawns an instance, so
     with two already up it silently fails. That is a local resource limit, **not** an app defect —
     do not chase it. `single_instance` is `false` in this config, so it is not the instance check
     either.
   - **A real defect was found here by inspection and fixed** (`e897d6b3b`), though it is not
     proven to be *the* crash. `refresh_process_card()` runs off the 250 ms `m_manip_timer`, and
     every `ShowModal()` spins a nested event loop in which that timer keeps firing — so the
     function re-enters. Its `process_card_refreshing` flag (which tells the field handlers "this
     value came from the config, not the user") was set true on entry and cleared
     **unconditionally** on exit with no re-entrancy check. A nested tick therefore cleared the
     flag while the outer pass was still assigning values, so every remaining
     `SetValue()`/`SetSelection()` in that outer pass was treated as a **user edit** →
     `tab->load_config()` wrote settings nobody touched → that raised another config change → which
     scheduled another refresh. Phantom writes plus a self-feeding loop, and the window it needs is
     "a modal is open while settings are being applied" — i.e. both reported triggers. Now it bails
     out when a refresh is already in flight and restores the flag via RAII; the timer body takes
     one tick at a time.
   - **A genuine out-of-bounds crash WAS found and fixed on the model-load path** (`95fd064c0`).
     `Sidebar::on_filament_count_change()` did `choices[0]->GetDropDown().Invalidate()` whenever
     `num_physical == 1`, without checking `choices` was non-empty. With mixed filaments that
     matters: `physical_indices` collects only non-mixed slots, so `num_physical` is **0** when
     every slot is mixed; the tail of the same function then calls
     `remove_unused_filament_combos(num_physical)`, which pops `combos_filament` with **no floor of
     one** and at 0 empties it outright. The next call in with a single physical filament clears
     the `num_physical == choices.size()` early-out (0 != 1), reaches that line, and reads `[0]` of
     an empty vector — a garbage pointer dereferenced immediately by `->GetDropDown()`. In Release
     that is an access violation **on project load**, which is exactly when filament counts change.
     Now guarded with `!choices.empty()`.
     > [!WARNING]
     > **A reachability claim was withdrawn — read this before citing the fix.** It was first
     > written up as reachable through ordinary filament editing, via
     > `on_filaments_delete()` → `remove_unused_filament_combos(size - 1)` emptying the vector when
     > the last filament is deleted. **That is wrong.** `Sidebar::delete_filament()` returns early
     > on `combos_filament.size() <= 1` (Plater.cpp:5470), so the physical filaments cannot be
     > deleted down to zero. And `add_custom_filament()` appends a mixed slot at
     > `new_idx == total`, so adding mixed filaments never converts the existing physical ones —
     > `num_physical >= 1` always holds through the UI. Reasoned, not measured, and measuring it
     > did not support it. Same failure mode as the withdrawn sentinel claim in §5.3.
     >
     > What survives: this is a **latent** out-of-bounds worth guarding, not a demonstrated
     > user-facing crash. The one route not closed off is a project whose `filament_is_mixed` marks
     > every slot mixed — `check_mixed_filament_integrity()` only *flags* such slots as broken, it
     > does not refuse them, so a hand-edited or corrupt 3MF still reaches
     > `on_filament_count_change()` with `num_physical == 0`. Unverified.

     Pinned by `tests/sidebar_filament_combos/` (`a81a00fe0`), which asserts the guard, the call-site
     count, and that `remove_unused_filament_combos()` still has no floor of one. **Mutation-checked
     for real:** removing the guard fails the contract, restoring it passes.
   - Audited and clean in the same area: `update_filament_row_badges()`,
     `update_mixed_filament_list()` (all parallel-vector reads are size-checked), and the
     `combos_filament[0]` in the ctor (a `push_back` precedes it).
   - None of the above is *proven* to be the user's crash — it was found by auditing, not by
     reproducing. Do not close the crash on it; do ask for a `crash_*.log` now that one gets written.
   - No stale `wxSingleInstanceChecker` lock in `<data_dir>\cache\` and no zombie `bambu-studio.exe`
     after a run, so the "refuses to open" symptom did not reproduce either.
   - Log truncation is **not** proof of a crash: `driver.py stop` kills the process and truncates
     the buffered log identically. Six of eight older logs end mid-line for that reason. The
     2026-07-28 20:12 log that ends inside `_save_model_to_file` is a **27-second** session, which
     fits a kill far better than a crash.

   **Code paths audited and cleared (do not re-audit these):**
   - `blend_color_multi()` (`FilamentMixer.cpp:115`) and `blend_mixed_color()` (`Plater.cpp`) —
     the parallel colour/ratio vectors are bounds-guarded on both sides.
   - `has_restore_data()` (`bbs_3mf.cpp:9674`) — already hardened by the earlier session (§5.3):
     `load_string_file()` is inside the `try`, empty process names never compare equal, and pid
     reuse is handled. Not a candidate any more.
   - `Sidebar::on_filament_count_change()` / `update_mixed_filament_list()` — `physical_indices[i]`
     is bounded by `num_physical`, and the mixed-filament option reads are all size-checked.

   **The strongest untested lead:** `%APPDATA%\BambuStudioInternal\log\` holds **no logs at all from
   2026-07-29 or 07-30** despite the user hitting crashes on those days. Either they are running a
   *different* build, or it dies before the log opens (`instance_check()` runs before `wxEntry()`
   and before boost log is initialised — an early exit there produces exactly "won't open, no
   log"). **Establish which binary they actually run before anything else.** Note a release
   installer uses data dir `BambuStudio`, not `BambuStudioInternal` — and no plain `BambuStudio`
   dir exists on this host, so the reported crashes probably did not happen on this machine.

   **The "refuses to open" half IS fixed** (`bbcf1630b`, on master, CI running at session end).
   `instance_check()` discarded `send_message()`'s return value and returned `true` — terminate —
   regardless. So when the single-instance mutex is held by something that cannot answer (a process
   wedged mid-crash, one still starting, one already tearing its windows down), the launch found no
   window, handed off to nobody, and **exited anyway**. Every attempt did that until the stale
   holder released the mutex: exactly *"try it a few times and eventually it opens"*. And because
   this runs before `wxEntry()` and before boost log exists, it left **no log entry at all**, which
   also explains the missing logs above. Now the hand-off decides: if nothing took it, the instance
   starts normally and logs why. The bare blocking `SendMessage(WM_COPYDATA)` — which hangs startup
   forever against a wedged instance, same silent non-start by a different route — is now
   `SendMessageTimeout` (`SMTO_ABORTIFHUNG`, 5 s), and `l_bambu_studio_hwnd` is cleared before each
   scan so a handle from a previous enumeration can never be messaged.

   > [!NOTE]
   > That fix was pushed with **local build verification incomplete** (the branch switch invalidated
   > the CMake cache and forced a full libslic3r rebuild, which was stopped in favour of CI). It is
   > a single self-contained `.cpp` change using Win32 calls already present in that file. Confirm
   > run `30593749021` is green.

   The re-entrancy guard added to `update_sidebar_scroll_body()` is a **defensive** fix for a
   plausible recursion (`SetVirtualSize` → scrollbar → `EVT_SIZE` → repeat, which both reported
   triggers would cross). It is **not** a confirmed crash fix and must not be written up as one.

   **"Model has no data" — FOUND, and it is not a crash.** The earlier "no such string exists"
   note was wrong because it only searched C++ and the native `.po` catalogs. The tabs the user
   says "do not work" (Ink / Device / Project) are **WebView2 surfaces**, so the string lives in
   the DeviceWeb locales:
   - `"No Data"` — `device_page/locales/en.json:59`, rendered by
     `src/features/filament-manager/SpoolTable.tsx:269`
   - `"Not signed in — no data available"` — `en.json:164`, rendered by
     `FilamentManagerPage.tsx:566`

   Both are the **empty state of the Filament Manager**, shown when there is no signed-in account
   or no network agent. This host's log shows exactly why:
   `NetworkAgent::initialize_network_module ... can not Load Library` → `unload_network_module` →
   `WebViewPanel::ShowNetpluginTip: bValid=0` → `no plugins currently`. So "switching tabs doesn't
   work and says no data" is **the network plugin not being installed / not signed in**, a separate
   issue from the crash. Confirm with the user whether they are signed in and whether the network
   plugin installed, before treating it as a defect.

   **A null-deref found while reading that path and fixed** (`7e1ebbf28`): `sLocalBindFunc()` did
   `wxGetApp().getAgent()->bind_detect(...)` with no null check. Its caller `InnerLoad()` validates
   the agent, then spawns this onto a `boost::thread` — so the check and the use are on different
   threads at different times. `m_agent` is deleted and nulled during teardown, and is only ever
   constructed under `if (create_network_agent)`, so it stays **null for the whole session whenever
   the network plugin fails to load** — the state this host runs in. A null deref on a background
   thread is an access violation with no handler and nothing useful in the log. Note this path only
   runs for users with a **stored `user_access_dev_ip` + `user_access_code`** (i.e. a previously
   LAN-bound printer), which is a plausible reason it never fires on this box and might on the
   user's. Still unproven as their crash.

0e. **The Pages dim-sum surprise is implemented; native-app and release coverage still need a fresh
   inventory.** The static site now performs one fresh 10% draw on eligible repeat visits, names the
   dish in English and Traditional Chinese, and loads only a published `catalog-v1` release photo
   from `Ding-Ding-Projects/dim-sum-photos` with no referrer. It never appears on first run, never
   blocks startup or steals focus, auto-dismisses, has no opt-out, and removes the retired stored
   opt-out during migration. Do not restore repository-bundled or generated consumer images.



**Pages/site work owed (see §5.0 and §5.0.1):**

0. ~~Confirm the `release` → dispatch path~~ — **done**, proven by `md3-v62` (§5.0.1). Nothing in
   the Pages workflow is unproven now. Red release runs from tags older than `5340bd466` are the
   pre-fix queue draining, not regressions; check the tag's commit before reacting.
0b. **`README.md` and `ROADMAP.md` still describe the renamed native screens by their old labels**
   (`Filament` cards, `AMS` dialogs, the wizard's filament page). The native UI, DeviceWeb, the
   prototype, the site and the design system have all moved to ink / Ink Dispenser; those two files
   have not, so they document a UI that no longer exists. Judge each line — `FilamentPicker` is a
   class name and stays, and completed ROADMAP history should not be retroactively rewritten.
0c. The published UI kit loads React and Babel from **unpkg.com**, so it makes third-party requests
   and renders nothing if that CDN is blocked — unlike every other page on the site. A separate
   session was started for this.

Items 1 and 2 of the previous list are **done** (see §5.4). What remains, in priority order:

1. **Native captures are still owed for everything changed on 2026-07-28.** The GUI build is
   clean and the app runs, but almost none of the reskinned surfaces have been photographed.
   Highest value first, all through `.claude/skills/run-bambustudio/`:
   the **fan control popup** (the biggest single reskin, and its toggles are now real controls —
   verify they take focus), the **Slice/Print dropdowns** (`SideButton` defaults + `SideMenuPopup`
   surface), the **measurement gizmo chips in dark mode**, and the **2D bed preview in dark mode**
   — that last one has an explicit open question recorded in `2DBed.cpp:88-100`: the slab sits at
   1.05:1 against its backdrop by arithmetic, and the fix that raises it costs grid contrast. A
   capture is the only thing that settles it.
2. **Re-do the Smart Home volume slider conversion.** It was reverted because `Slider` could not
   be reached by Tab and exposed no screen-reader role. Both are fixed now (`2283f5dc8`), so the
   swap is safe — but `tests/home_assistant/home_assistant_ui_performance_contract.cmake:86`
   anchors on the literal string `m_volume->Bind(wxEVT_SLIDER`, so that contract must be updated
   in the same change or CI goes red. Keep what it is really asserting: that volume dispatch
   happens inside the debounce callback, not before the slider hook.
3. **Finish `gizmo-rail-svg-icons`** (register row remains **partial** until native evidence).
   The conservative closure overlay is now present in the working tree: 34 existing SVG resource
   keys have MD3-token artwork, with no C++ or behavior change. Static inventory/XML/palette/
   geometry/source-link checks pass via `scripts/validate_md3_gizmo_assets.py`; Windows build,
   light/dark/DPI runtime captures, operation checks, and the two bounded investigations remain
   outstanding. Do not mark the row done from static review alone; use
   `CODEX_HANDOFF_BAMBUSTUDIO_MD3.md` and `ACCEPTANCE_MATRIX_BAMBUSTUDIO_MD3.md`.
4. **Issue #24 — 8 verified `ui-md3` defects** (4 accessibility, 1 clipping, 3 search/regex).
   Left unfixed on purpose: a concurrent session owned that tree. Check whether it still does.
5. **Verify and deliver "Add my printers to Home Assistant"** (issue #16) — see §7.1.
6. **Issue #15 is waiting on the user**, not on you: whether app-data secrets are redacted,
   committed with disclosure, or encrypted. Do not start it by guessing.

### 7.0 Two traps this session paid for — do not repeat them

- **Do not edit source files while an audit or review agent is reading them.** Three verifiers
  reported findings as "refuted — this code does not exist" when what had actually happened was
  that the fix landed mid-audit. The verdicts were worthless and the time was wasted.
- **A green patch is not a correct patch.** Of eleven fixes, four passed compilation and failed
  adversarial review — one of them *introducing* a dark-mode regression while fixing a contrast
  bug (a white error link brightened to `y=1.1`, which `IM_COL32` packed with no clamp so the
  carry landed in blue and painted the underline magenta). Wave B repeated the pattern: a
  conversion added `overflow: hidden` to a compressible flex item, i.e. introduced a clipping
  defect inside a task whose entire purpose was removing them. **Review every patch, including
  the ones that compile.**

### 7.1 Item 3 in detail — Home Assistant printer handover (IMPLEMENTED; VERIFICATION PENDING)

**Current boundary:** the code, focused tests, cross-host probe, documentation, localization source,
and Windows workflow wiring are complete. The focused Release targets are built and green; the full
Release GUI build and English native 720×760/520×480 clipping review are also complete. Native
bilingual capture, live Home Assistant paths, and physical-printer success remain acceptance
conditions. Remote publication and hosted Pages are verified; Windows CI/release evidence is tracked
separately in issue #16 because this record must not predict a running job. Do not call the feature fully
runtime-verified, shipped, or issue-complete until every applicable boundary has observed evidence.

The companion
[`Ding-Ding-Projects/ha-bambulab`](https://github.com/Ding-Ding-Projects/ha-bambulab) now pins
Home Assistant 2025.1.4 plus its matching fixture package. Its canonical Ubuntu workflow passes
**93/93** in 3.36 seconds at
[run 30359258358](https://github.com/Ding-Ding-Projects/ha-bambulab/actions/runs/30359258358),
and published the tested root-content
[`v3.0.7` HACS package](https://github.com/Ding-Ding-Projects/ha-bambulab/releases/tag/v3.0.7).
Hassfest is green. The separate HACS repository validator remains red because neither fork nor
upstream declares a license; choosing terms requires owner authority and is tracked in
[companion issue #1](https://github.com/Ding-Ding-Projects/ha-bambulab/issues/1). Live Home
Assistant/physical-printer verification remains pending.

**Implemented Path B — explicit service call with a Home Assistant long-lived token:**

- `SmartHomeDialog` collects accessible printers from `get_local_machinelist()` first and
  `get_user_machinelist()` second, deduped by serial. Inclusion requires access rights, serial, LAN
  address, and access code.
- The visible **Add my printers to Home Assistant** action discloses the exact fields being copied
  and states that access codes are credentials. The decision dialog uses **No** as its default.
- `HomeAssistant::add_printers()` posts each printer to
  `POST /api/services/bambu_lab/add_printer` with
  `{serial, host, access_code[, name]}` and reports processed/failed request counts on the UI thread
  (a 2xx may be an idempotent already-configured result). One import remains single-flight, but its
  requests run in four-wide waves; 32 dead endpoints therefore consume at most eight 30-second
  timeout waves instead of about 16 minutes of serial waiting.
- Progress and results use non-blocking notifications with dialog-status fallback. Failures expose
  the serial and HTTP status but never echo a response body that may contain credentials.
- Every Home Assistant bearer request now goes through `HomeAssistantTransportPolicy`: HTTPS is
  accepted; HTTP is accepted only for localhost or an explicit IPv4 loopback. Clear-text LAN HTTP,
  malformed URLs, unsupported schemes, and URL user information are rejected before networking.
  These requests also disable redirects and libcurl verbose tracing so credentials cannot be
  replayed by a redirect or printed in a protocol trace.

**Implemented Path A — temporary local discovery without a Home Assistant long-lived token:**

- **Share for discovery for 5 minutes (no Home Assistant token)** is off by default, never
  persisted, starts only through the user's toggle, and automatically turns itself off after five
  minutes.
- Every sharing window gets a fresh URL-safe capability with more than 240 random bits.
- `HomeAssistantSharingService` binds `GET /bambustudio/printers` to the RFC1918/shared IPv4 address
  on the ordinary default route (not a multicast-preferred host-only virtual adapter) and an
  operating-system-selected port, then advertises
  `_bambu-slicer._tcp.local.` PTR/SRV/TXT/A records. TXT contains `pairing` and `name`.
- The service resolves the real prefix length for that interface and answers mDNS only for usable
  senders on the exact advertised link. Network, broadcast, public, loopback, and link-local
  senders are rejected. Query admission is burst eight/refill one per second before allocation;
  replies are deduplicated, queued to eight, and paced 50 ms apart.
- The HTTP endpoint requires exactly one matching Bearer header before it asks for printer data.
  It caps header bytes, target length, timeout, concurrent sessions, response size, printer count,
  and field lengths; strips unknown fields; uses no-store/nosniff/close headers; and returns generic
  errors without reflecting sensitive values.
- Turning the toggle off, closing the dialog, reaching the five-minute expiry, or destroying the
  service closes sessions, stops serving, sends a zero-TTL mDNS goodbye, and discards the pairing
  capability.
- This path is clear-text LAN HTTP. The pairing capability is visible in mDNS to the broadcast
  domain, so the UI/docs instruct users to use a trusted LAN and keep the window brief. It is not a
  claim of encrypted transfer.

**Local build, native UI, and focused verification completed on 2026-07-28:**

- The Windows SDK 10.0.26100.0 Release build completed `home_assistant_tests` and
  `home_assistant_sharing_probe`. The test binary passed **30 cases / 267 assertions**, and all five
  `home_assistant_*` CTest entries passed.
- The full `BambuStudio_app_gui` Release build exited 0 after **3,387 seconds**; its first no-change
  rebuild exited 0 in **8.3 seconds**. After the clipping build described below, the final nonvisual
  import-scheduling and cancellation-cleanup changes compiled and linked in **214.808 seconds**; the
  final no-change rebuild exited 0 in **8.544 seconds**. The resulting DLL is 151,299,584 bytes,
  timestamped `2026-07-28 08:15:46 -04:00`, with SHA-256
  `41BB1BFC754E3184C5908E2145A93E3640D3866E59380F32EEFF7A76F418E972`.
- Lowlevel MCP headless review at 720×760 and the declared 520×480 minimum found a real clipping
  defect: `make_responsive_action` forced text actions such as **Close** and the media controls
  into 44-DIP widths. After removing that shrink, `SmartHomeDialog.cpp` rebuilt and the DLL linked
  successfully in **141 seconds**; a no-change build exited 0 in **8.0 seconds**. The two primary
  corrected captures were recaptured from the final `41BB1B…` DLL. The media-action close-up uses
  the preceding `EBF646…` DLL; the later build changes only printer-import scheduling and
  alert-light cancellation cleanup, not `SmartHomeDialog` or `MsgDialog` layout.
- Genuine before/after evidence lives under `docs/screenshots/smart-home/`:
  `dialog-720x760-before-text-action-fix.png`,
  `dialog-720x760-after-text-action-fix.png`,
  `dialog-520x480-before-text-action-fix.png`,
  `dialog-520x480-after-text-action-fix.png`, and
  `dialog-520x480-media-actions-after-fix.png`. The corrected **Close** and media actions remain
  readable at both reviewed sizes. These captures are English; bilingual native capture remains
  pending.
- On the GPU-less Mesa llvmpipe headless host, cold first-run launch took **37.427 seconds** and a
  subsequent launch took **32.339 seconds**. These are environment observations, not production
  benchmarks. The app remained responsive with no hang at about 523 MB after 13.74 minutes and
  about 549 MB after 6.26 minutes; the latter observation included three incidental Version
  History windows.
- `home_assistant_sharing_probe` runs the production service with a synthetic TEST-NET printer for
  second-host mDNS/HTTP verification without printing its token or payload. A second LAN host
  observed PTR/SRV/TXT/A, completed one authenticated bounded fetch, and observed the zero-TTL
  goodbye.
- That pass caught a real false-LAN selection: the multicast route preferred a host-only WSL
  adapter. Auto-detection now follows the ordinary default route, while the multicast flood test
  correctly sends real multicast rather than nondeterministic unicast into a shared Windows port.
- The Cantonese catalog check passed 718 entries, the static Pages/i18n/clipping suite passed 21/21,
  and the browser Pages matrix passed all 156 combinations of 13 physical widths, four zoom levels,
  and three language modes. Template assembly is synchronized. Native bilingual Smart Home capture
  remains pending.
- Performance bounds now include a 4 MiB entity-state body, at most four query domains, 512 parsed
  backend entities, 256 rendered matches, and persisted-list inspection capped at 256 segments,
  64 KiB, and 256 bytes per value with at most 32 active unique entries. Path B imports at most four
  printers concurrently, so the 32-printer cap takes no more than eight timeout waves. A failed
  two-attempt light restore retains its generation-specific recovery scene rather than deleting it;
  cancellation after scene creation but before a flash deletes the unused scene, and the focused
  regression records the exact create/delete sequence and shutdown timeout.
- `.github/workflows/build_bambu.yml` builds both targets and includes `home_assistant_tests` in the
  maintained CTest gate.
- `docs/features/windows/smart-home.md` is the user/maintainer behavior and security guide.
  `docs/features/api/home-assistant-printer-discovery.md` plus focused and master Postman
  collections document the transient endpoint.

**Do this next, in order:**

1. Capture the native bilingual Smart home dialog with `.claude/skills/run-bambustudio/` through
   Lowlevel MCP headless mode. Check the wrapped “don't show again” footer, stacked actions,
   credential disclosure, No-default confirmation, transport rejection, sharing status, keyboard
   reachability, focus, and clipping.
2. Verify Path B end-to-end against a real Home Assistant with `bambu_lab:` configured. Never place
   a real token or access code in a command line, log, screenshot, issue, Discussion, or Git.
3. Verify Home Assistant's real Path A confirmation card. Cross-host PTR/SRV/TXT/A discovery,
   authenticated fetch, and goodbye are already observed; the synthetic probe is not a real-printer
   import.
4. Verify a physical-printer success path when hardware is available; do not substitute the
   synthetic TEST-NET transport probe for that result.
5. Update `ha-bambulab/docs/verification.md` only with observed evidence. Ensure both repositories'
   remote default branches contain the exact verified commits, post the native screenshot and
   evidence to issue #16, record the observed hosted CI/release verdict there, and close #16 only
   after every acceptance condition is proven.

**Definition of done for issue #16:** the Path B button works end-to-end against a real Home
Assistant and physical printer; Path A's production service is observed across hosts and produces
the companion integration's real discovery card; native UI screenshots are posted to the issue;
both repositories are pushed; hosted checks have an honest recorded state; and no credential
appears in any retained artifact.

---

## 8. Rules this repo is run by (do not skip)

- **An auto-commit daemon runs on this machine.** It periodically commits *all* uncommitted
  changes in this repo and pushes `master`. Never leave half-finished work in the tree expecting
  to commit it later with a clean message. Commit deliberately and promptly.
- **Never claim a build or CI run succeeded before it reports.** Check the log, check the job
  list, and say "running" when it is running.
- **Screenshot evidence must be genuine**: from the real built binary, through the project's own
  capture harness. Never a mockup, never a different surface passed off as the fixed one.
- Commit messages are **bilingual**: concise English subject, playful Hong Kong Cantonese in the
  body.
- Every user-facing surface must follow Material Design 3, provide the three language modes
  (English / Cantonese / bilingual), use non-blocking toasts for anything informational, and
  route every search bar through the shared regex builder. See `docs/features/` for per-feature
  documentation and `.claude/skills/` for tooling.

---

## 9. Where documentation lives

- `docs/features/README.md` — index of categorized feature docs.
- `docs/features/windows/windows-only-platform.md` — the Windows-only policy in detail.
- `docs/features/workspace/project-version-history.md` — the Git-backed history feature,
  including the crash-backup preservation behaviour described in §5.3.
- `docs/features/pages/README.md` — the published GitHub Pages site: tabs, language modes and
  funny levels, the regex builder, the changelog viewer, and the current 447-case layout gate.
- `docs/screenshots/README.md` — the screenshot matrix index.
- `docs/screenshots/pages/README.md` — captures of the published site, and how to retake them.
- `.claude/skills/run-bambustudio/SKILL.md` — how to run and drive the app. **Read this before
  trying to test anything in the UI.**

---

## 10. One-click local Windows installer build

`OneClickBuildInstaller.cmd` now provides the local bootstrap → dependencies → Release app →
payload → verified Mesa fallback → SBOM → NSIS → SHA-256 path. Its implementation is
`scripts/windows/Invoke-OneClickBuild.ps1`, its static contract test is
`scripts/ci/Test-OneClickBuild.ps1`, and its operator guide is
`docs/features/releases/windows-one-click-build.md`. The installer remains unsigned and is launched
only when the caller explicitly supplies `-Install`.

---

## 11. `wgtMsgPanel` LinkLabel build repair (2026-09-02)

### Scope and root cause

The `c2d7d36c7d455b7ca5088b617708e0294ac61721` control-conversion commit changed
`wgtMsgPanelItem::m_wiki_link` from `wxHyperlinkCtrl*` to `LinkLabel*`, but
`src/slic3r/GUI/DeviceTab/wgtMsgPanel.h` did not include or declare `LinkLabel`. MSVC first stopped
at line 41 with `error C2143: syntax error: missing ';' before '*'`; the later undeclared-member
diagnostics were a cascade.

### Landed repair

- `1a855ea0d1adbb04d324a31ff9ba9c402e7d098b` replaces the obsolete wx hyperlink include with
  `slic3r/GUI/Widgets/LinkLabel.hpp` and adds a focused source contract.
- `48e9378cd071c34302c8b4333de392a3ca8c5c15` makes that contract require the owning include as
  the first nonblank directive after `#pragma once`, before the converted member. Read-only break
  probes verified that a late include and an include inside `#if 0` both fail the contract.
- `node --test ui-md3/tests/md3-conversion-contracts.test.mjs` passed 12 tests with 0 failures.
  Before the source repair, the new focused test failed as intended (11 passed, 1 failed).

### Hosted verification and release

- Run [`33695532981`](https://github.com/Ding-Ding-Projects/BambuStudio/actions/runs/33695532981)
  succeeded for the source repair and published immutable release
  [`md3-v100`](https://github.com/Ding-Ding-Projects/BambuStudio/releases/tag/md3-v100).
- Final-tip run [`33696115604`](https://github.com/Ding-Ding-Projects/BambuStudio/actions/runs/33696115604)
  succeeded at `48e9378cd071c34302c8b4333de392a3ca8c5c15` and published immutable, non-draft,
  non-prerelease release
  [`md3-v101`](https://github.com/Ding-Ding-Projects/BambuStudio/releases/tag/md3-v101),
  **Bambu Studio MD3 v101, Beancurd Skin Roll 腐皮卷**.
- The final workflow recorded `2026-09-02T23:39:52Z` to `2026-09-03T01:38:56Z`, duration
  `02:59:04`. All five release-asset endpoints returned HTTP 200 with their expected filenames and
  content lengths. `Setup.exe` is intentionally unsigned.

### Public records and remaining work

- Issue [#28](https://github.com/Ding-Ding-Projects/BambuStudio/issues/28) contains the start,
  diagnosis, red-to-green proof, commits, workflow evidence, release inventory, and finished
  handoff; it is closed as completed.
- Discussion [#29](https://github.com/Ding-Ding-Projects/BambuStudio/discussions/29) is the rolling
  progress record. Announcement [#30](https://github.com/Ding-Ding-Projects/BambuStudio/discussions/30)
  records the v101 release.
- This was a compiler-only repair with no visible application state, so no screenshot applied.
  The separate light and dark capture matrix for the converted controls remains open in
  `ROADMAP.md` and the parity register.
- The pre-existing Home Assistant issue #16 remains separate and unchanged.

## 12. Every element Material Design 3, every clipping defect fixed (2026-09-05/06)

### What landed

- Own CMake pipeline behind `build.bat` (`scripts/windows/Invoke-OneClickBuild.ps1`); the upstream
  `build_win.bat` is retired. Thirteen attempts on this host: 1 to 5 were bootstrap defects in the
  script (all fixed in-tree), 6 and 7 built the unmodified baseline, 8 and 10 to 13 built the
  converted tree, 9 went red on one missing `wx/glcanvas.h` include in `LayoutProbe.cpp`.
  Configure runs only when the CMake cache is missing or `BAMBU_RECONFIGURE=1`, because
  `libslic3r/CMakeLists.txt` stamps the build time into a generated header and any configure is a
  full recompile.
- Stock control conversions: 548 `wxStaticText` to kit `Label`, 7 `wxRadioButton` to
  `LabeledRadioButton` + `RadioGroup`, multi-line `wxTextCtrl` to `TextArea`, `wxListBox` to
  `ListBox`, 11 `wxBitmapButton` to icon `Button` / `RadioBox`, static-bitmap triage closed,
  `Button` Material by default, 207 legacy palette runs to variants. Eight compile defects from the
  merge fixed in `68b0107da`, `df80dba36`, `cc6affdc1`, `bbc9db5cf`.
- Runtime layout probe (`src/slic3r/GUI/LayoutProbe.{hpp,cpp}`, gated on `BAMBU_LAYOUT_PROBE`,
  triggered by `WM_COPYDATA` dwData 2 from `scripts/md3/send-layout-probe.py`, which must itself run
  on the hidden desktop). Records carry `on_screen` and the canvas emits `gl_item` rectangles for
  the scene toolbar and gizmo rail (`GLCanvas3D::get_toolbar_item_rects()`). Reader:
  `ui-md3/tests/layout-probe-report.mjs`.
- Clipping inventory `docs/features/design-system/clipping-inventory.md`, rows CJ-001..CJ-010,
  guarded by `ui-md3/tests/clipping-inventory.test.mjs`. Verified from real captures: CJ-005
  (starvation class), CJ-006 (Device placeholder heading, CSS), CJ-007 (split-button chevrons),
  CJ-008 (ink swatch badge regression from the sweep itself), CJ-009 (Home caption bar stuck at
  787 px: `wxAuiToolBar::Realize` resizes to content unless `wxAUI_TB_NO_AUTORESIZE`; both
  `BBLTopbar` constructors now pass it), CJ-010 (the preset combo's stock cog on the Printer card,
  shown at zero width outside every sizer; hidden). CJ-001..004 are the high-DPI rows: fixed and
  guarded, unverifiable on this 96 dpi host.
- CJ-011 (`baabd4e17`): every kit `SearchField` painted its 44 px regex and builder buttons over the
  44 px pill's outline and reserved an empty clear slot; buttons are 40 px and the clear slot is
  reserved only while shown. CJ-012 (`3f4d8ffeb`, verified on attempt 23): with Advanced settings
  open every sidebar row was laid out 1271 px wide inside a 479 px scroller behind a horizontal
  scrollbar. `wxBoxSizer::CalcMin` scales the largest proportional minimum by the total
  proportion, and the ParamsPanel header had its title at proportion 1 (56 px) beside stretch
  spacers of 2, 1 and 12 (56 x 16 + fixed = 1271); `update_sidebar_scroll_body` honours the content
  minimum as the virtual width. The header now has a fixed title and one stretch spacer. Rule for
  the next reader: a proportional item with a minimum next to stretch spacers inflates the sizer
  minimum; keep stretch spacers to one, or give the proportional item no minimum.
- Upstream v02.08.02.61 merged (`5a08b703d` on `md3-upstream-merge`, integrated `3886fd482`):
  34 conflicted files, resolved keeping every Material Design 3 surface; Preferences.cpp taken
  whole from this fork (upstream rewrote its layout; its WebView devtools toggle is a follow-up).
  The merge reintroduced four stock controls and one legacy-palette dialog, all converted in
  `5d52a8e53`. Rule: after any upstream merge run `node --test ui-md3/tests/` first; the four
  contracts that went red name the exact sites.
- Update check (`GUI_App::check_new_version`) reads this fork's GitHub releases and compares
  `published_at` with `SLIC3R_BUILD_TIME` (three-hour margin); the Bambu Lab feed and the beta
  channel are no longer consulted. See docs/features/windows/app-updates.md.
- Every process setting is shown by default (`get_mode()` always advanced; header switches and
  the Simple/Advanced flip removed, `67328e94f`).
- Build script: the device_page pnpm steps run under CI=1 (`c127ab158`); without it pnpm waited
  on a terminal for over fifteen minutes in the console-less one-click build.
- GitHub Actions answered HTTP 422 for a dispatch on 2026-09-06; by 2026-09-07 03:38Z it was
  publishing again on every push (correction recorded below). At that time the last hosted run
  (`34006586101`) failed on a `GetLogicalHeight` call removed in `68b0107da`. Releases from here
  are manual: build.bat /s, build-installer.bat /s, then gh release create with the line-count
  table from scripts/ci/Measure-LineCount.ps1 (fixed for cp1252 consoles in `60ed72397`).
- Local hook: .git/hooks/pre-commit piped unquoted filenames into grep; fixed locally with
  `-z | xargs -0` (the hook is not tracked).
- Attempt 27 (`07335a340`, DLL `a0392d4affee9d77`) is the first green build of the merged tree;
  attempts 24 to 26 found the pnpm wedge and eight merge compile slips, each fixed in its own
  commit. Evidence from it: `prepare-full-tree-default--...--after.png` (fresh profile, full tree,
  no Simple/Advanced control), `gizmo-rotate-panel--...--after.png` and `gizmo-move-panel--...--
  after.png` (software-GL route; only the first rail item clicked per launch activates its gizmo,
  so capture one panel per launch). The update dialog cannot be captured until a release newer
  than the running build exists.
- CJ-013 (`92cd7bce7`, verified on attempt 28): the category pill strip overflowed and the settings
  tree lived in a 144 px inner scroller at the default sidebar width. TabCtrl pill mode now flows its
  items onto further rows (relayoutPills) and ParamsPanel::fit_page_to_content sizes the page to
  its content and tells the sidebar, which hosts the tree at proportion 0, so the sidebar body is
  the only scroller. Rule: a scroller inside a scroller is a defect here; measure with the probe
  (page scroller rect vs best) rather than by eye.
- Capture matrix `docs/screenshots/md3-everything/`: 132 before (baseline payload `9e8d005b0`) and
  132 after (attempt 13, `80734696c`) across 12 tuples (en / yue_HK / bilingual x light / dark x
  comfortable / compact), 24 probe dumps per build under `probe/`. Runner
  `scripts/md3/capture-tuple.py`; datadirs from `scripts/md3/prepare-capture-datadirs.py`.
- Full recapture of the 294 tracked screenshots: manifest `docs/screenshots/recapture-manifest.json`
  (recipes from `scripts/md3/fill-recapture-recipes.py`), orchestrator `scripts/md3/recapture.py`,
  report `artifacts/recapture-report.json`. See the manifest's `provenance` for the payload.

### Machine facts learned (corrects §2)

- The host has an NVIDIA RTX 3050; the app runs on the real driver on a hidden desktop. GL canvases
  capture blank through `PrintWindow` on that route; canvas crops need the Mesa pair staged in
  `install-dir/mesa`.
- Only a 96 dpi display exists; 125/150/200% tuples cannot be produced here.
- Cross-desktop rule: `IsWindow`, `SendMessage`, `press_keys` and `resize_window` by handle all fail
  from the visible desktop against a window on a hidden one. Launch the sender or use a background
  `mouse_click`, which does cross.

### Open

- Gizmo rail, scene toolbar and preview overlay register rows still need runtime evidence from the
  Mesa route (crop-gl rows in the recapture manifest).
- Issues #32, #33 and #34 were deleted by an owner (2026-09-05/06); the current handoff record is
  issue #35 (opened 2026-09-07).
- Hosted release from the final tip: md3-v106 "Char Siu Cheung Fun" published 2026-09-07 by hand
  from `a3b121673` (build attempt 30, `BambuStudioMD3-2.8.2106-full.nupkg`, unsigned Setup.exe
  SHA-256 `3b5fe0424ca602deed99c0e39f8f3d77ddab73d4197347a8d14d32222f71db9b`, six assets, download
  verified HTTP 200). Correction: GitHub Actions was enabled again by then and the per-push
  workflow independently published md3-v107 (`a467e28ef`), md3-v108 (`a55930919`), md3-v109
  (`a3b121673`, the same commit as the manual md3-v106) and md3-v110 (`a0902033a`), each with
  package version 2.8.2-build61 because the workflow did not yet pass the release number; that is
  fixed in `5ee2219bc` (the build job derives the highest existing md3-v tag plus one and passes
  `-ReleaseNumber`). That first attempt failed on the hosted runner (`-match` in the new loop
  rewrote `$matches`, so packaging received a tag number); `7a036286c` captures the product
  version first. Hosted run 34154014720 on `7a036286c` is green (02:39:56) and published
  md3-v111 "Big Chicken Bun" with `BambuStudioMD3-2.8.2111-full.nupkg`, the first hosted release
  whose package version carries the release number. The after-capture matrix and line count in
  the v106 notes were measured at `a3b121673`; the v106 notes carry the same correction.
- Done 2026-09-06: the Squirrel package version now carries the release number
  (`Invoke-SquirrelPackage.ps1 -ReleaseNumber`, resolved by `Invoke-OneClickBuild.ps1` from the
  parameter, `BAMBU_RELEASE_NUMBER`, or `gh release list`), fixing the identical `2.8.2-build61`
  package version of md3-v104 and md3-v105.
- Open (found 2026-09-07 during the md3-v106 pass): the software-OpenGL copy of the payload
  (`install-dir-mesa`, Mesa 26.1.3 `opengl32.dll` + `libgallium_wgl.dll` beside the exe) exits
  within ten seconds of launch on the build host, before any frame, on a fresh and on a reused
  `--datadir`; the Mesa DLL hashes are identical to the build's hash-pinned set and the real-GPU
  payload (`install-dir`) runs for minutes. No Application event-log crash record was written.
  Root cause (2026-09-07, build attempts 31/32): Mesa reads GALLIUM_DRIVER / LIBGL_ALWAYS_SOFTWARE
  through its C runtime environment copy; without them Mesa 26 picks another gallium driver and
  the process exits within seconds. The self-heal relaunch passed those variables only to its
  child, so every later plain start with the copied pair died. Fix: `GUI_App::OnInit` now calls
  `OpenGLManager::apply_bundled_softgl_environment()`, which relaunches once with the llvmpipe
  environment when the pair sits beside the exe and no driver was chosen (setting the variables
  in-process was proven insufficient on attempt 31). Verified on attempt 32: the launcher hands
  over to a child that stayed alive with the main frame rendered
  (`docs/screenshots/md3-everything/evidence/softgl-relaunch-main-frame--build32.png`).
- Fixed 2026-09-07 (build attempt 35): Setup.exe reported "Installation has failed" on a machine
  with an earlier install. Squirrel's log: it could not delete
  `app-2.8.1-build55\resources\fonts\Roboto-Regular.ttf` because the Windows Font Cache Service,
  fontdrvhost and Chrome held it. The app registers its bundled faces session-wide on purpose
  (GDI+ crashes on FR_PRIVATE faces), and a session font inside the versioned install folder
  stays mapped by other processes after the app exits, so no installer or updater can replace
  that folder. `Label.cpp` and `MaterialIcon.cpp` now copy each face to `<data_dir>\fonts` and
  register the copy (Restart Manager proof: zero holders on the payload's fonts while the app
  runs, holders only on the staged copies). Machines already locked need a sign-out or reboot
  (or a Font Cache Service restart) once before the next Setup.exe succeeds.

---

## 2026-09-18 repository closeout handoff

- Inventory and fetch completed in the primary checkout. `main` was fast-forwarded from
  `69de7d38dc829646eea0b847ca9f7bae0bd96802` to `5014d08562b1de7f2198a2e1134eb4d378cb670f`,
  incorporating the `AGENTS.md` vocabulary-discipline update from `origin/main`.
- Completed export work from `worktree-agent-a83c1d2b35b074967` was integrated into `main` in
  merge commit `7df4f534c4c80274355b9d3fe7ebae9a715756e5`. The merge preserved the export
  dialog, dataset, format, test, documentation, and build registrations.
- Conflict decisions: `GUI_Factories.cpp` and `Tab.cpp` retain both the appearance and export
  includes; `tests/CMakeLists.txt` retains `super_confirm`, `changelog`, and `export_everything`;
  `Tab.cpp` keeps the newer `SuperConfirmGate` delete flow from `main` and adds the export control
  beside it. The older modal delete path was not reintroduced because it would weaken the newer
  destructive-action contract. All unmerged index entries and conflict markers were removed.
- The primary checkout has no staged or unstaged source changes. The untracked
  `.claude/worktrees/` directory is generated linked-checkout storage and is intentionally not
  staged as source. It contains the linked checkouts listed below.
- Four checkpoint branches remain intentionally preserved and pushed because they are not ancestors of
  `origin/main`: `worktree-agent-a24267613001f90e0` at `1498551fea628cef31a1a0f0acad041a5ce03c27`,
  `worktree-agent-a28cd0c64451603b9` at `dcc5c09265f3f49399c3efff7e69a889426f4c73`,
  `worktree-agent-a470984c061728c07` at `29209a60bca2125ce6bd9b7d16b1270ae39a535a`, and
  `worktree-agent-a83c1d2b35b074967` at `df60f7d1d2a01bd63e5af2a405d6ad3f9acdad4b`.
  The export branch is now integrated, but its source ref remains preserved until the archive and
  ancestry checks authorize removal.
- `worktree-agent-a2c028be23ca8b092` is locked by an active process and is retained even though its
  tip is already an ancestor of `origin/main`. The remaining completed, clean, ancestor branches and
  their linked checkouts are candidates only after the external archive is verified and ownership
  is confirmed by the closeout pass.
- No Git stashes were present. No local conflict entries remain. No build, installer, release, or
  unrelated product work was run during this closeout.

### Archive and cleanup result

- The first archive attempt, `BambuStudio-20260918T170234Z.7z`, returned exit code `1` because four
  tracked paths were absent on disk. It was not used as deletion evidence.
- The verified archive was stored in an owner-managed backup folder outside this checkout.
  It is 4,838,688,295 bytes and passed `7z t`; the read-back listed 191,804 files and 11,467
  folders, including 611 `.git` administrative entries. The four absent tracked files are listed
  in the closeout log and were excluded only because they did not exist on disk.
- After archive verification and ancestry proofs against pushed `origin/main` at
  `2b058b95ce138f0fe04152535ff848adb1842b8e`, these 11 clean redundant linked checkouts and local
  branches were removed: `a0105905c86c50ce9`, `a0f70928641ad0755`, `a1ca11d4e1a10f6a4`,
  `a4a18b5a32117547f`, `a67df19625f3bae6a`, `a7fadd855e01844a`, `a83c1d2b35b074967`,
  `a89c4bc7767da0773`, `ac6d63cb45f27b994`, `ae23d1c6043a8e08e`, and `afd570dc82b037965`.
  The pushed `worktree-agent-a83c1d2b35b074967` ref was deleted after its merge proof.
- Retained by design: checkpoint branches `worktree-agent-a24267613001f90e0` at
  `1498551fea628cef31a1a0f0acad041a5ce03c27`, `worktree-agent-a28cd0c64451603b9` at
  `dcc5c09265f3f49399c3efff7e69a889426f4c73`, and `worktree-agent-a470984c061728c07` at
  `29209a60bca2125ce6bd9b7d16b1270ae39a535a`, plus the locked active
  `worktree-agent-a2c028be23ca8b092` at `5d61f48d40bc15c66593b8c151152c70fa72d72f`.
  Their linked checkouts are clean, but the three checkpoint branches are unmerged and the locked branch
  is active, so none is removable in this pass.
- The final documentation commit is `7281bd15da7cda27e8fff73bab253521a6027954`; after this
  handoff refresh, `git ls-remote origin refs/heads/main` was verified at the same SHA.

</details>
