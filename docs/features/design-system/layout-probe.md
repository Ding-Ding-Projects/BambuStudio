# Runtime layout probe

An off-by-default measurement mode that finds layout clipping mechanically instead of by eye.

## Why it exists

An over-subscribed `wxBoxSizer` row does not overflow. It pays proportion-0 items in full and hands
the items after them zero width, so the starved control simply vanishes while every child still
reports a rectangle inside its parent. Two primary controls (the Print pill and the Process title)
were lost that way before and both were found late, by looking at screenshots. The probe sums each
row's minimum sizes against its allocation and says so.

## Behaviour

When `BAMBU_LAYOUT_PROBE` is set at launch, the app writes one NDJSON file per dump:

- a `header` record: reason, free-form `tag` (from `BAMBU_LAYOUT_PROBE_TAG`), pid, DPI scale,
  language, dark mode, density, number of top-level windows;
- one `toplevel` record per top-level window;
- one `window` record per window in the tree: class (the wx class-info name; a kit widget that
  declares none, such as `Button` or `Label`, reports its nearest wx base, usually `wxWindow`), type
  (the C++ type, so `Button`, `Label`, `TextInput`), name, label, rect, screen rect, client, min,
  best, shown, enabled, parent handle, the sizer item that owns it (proportion, flag, border,
  `CalcMin`, allocation) and the owning box sizer's verdict (orientation, available, required,
  oversubscribed).

Flags on each window record:

Every flag applies only to a window a user can see: shown, with every parent shown. A control inside a
hidden panel is never flagged, whatever its own shown flag says.

| Flag | Meaning |
| --- | --- |
| `starved` | a shown sizer child allocated less than its own minimum |
| `zero_sized` | a shown window with zero width or height |
| `oversubscribed` (in `sizer.row`) | the box sizer's children need more than it has |
| `text_clipped` | a label is cut or shortened although nothing asked for it: its text extent is wider than its client width and it has no ellipsize style, or a kit `Button` that may not shrink drew its label shortened |
| `truncated` | a label was drawn shortened with an ellipsis, asked for or not: an ellipsizing static text wider than its client width, or any kit `Button` whose last paint shortened its label. A shortened action in a dialog is a defect even where shrinking is allowed |
| `ellipsized` | the label carries an ellipsize style, or the kit `Button` is allowed to shrink, as the notebook tabs are (reported for review, not a finding) |
| `hint_clipped` | an empty single-line text entry shows a placeholder hint wider than its client width, so the edit control cuts it; `hint` and `hint_width` carry the hint and its extent (What's new cut "YYYY-MM-DD / DD/MM/YYYY" to "YYYY-MM-DD / D", clipping inventory CJ-027, before hints were measured) |
| `clipped_by_parent` | a shown child window's rect leaves its parent's client area along an axis the parent does not scroll (a row below the fold of a scrolling panel is scrolled away, not clipped; a dialog or other top-level window is never flagged: it is its own window) |

One `gl_item` record per visible item of the scene toolbar (`"toolbar":"main"`) and the gizmo rail
(`"gizmo"`): name, host canvas handle, rectangle in canvas pixels and on screen, derived from the
item's world-space render rectangle and the camera zoom. These are not wx windows, so no flag
applies; they exist so a capture can be cropped to a toolbar or rail item by name.

One `tool` record per labelled item of every `wxAuiToolBar` (the caption bar's brand tile, menu
tools, history chip, palette and window controls), carrying the item id, its label or help text,
and its rectangle in toolbar and screen coordinates. Tools are not windows, so without these the
caption bar was unaddressable by the recapture driver and invisible to any label-based check.

Every dump ends with `{"kind":"end"}`. A reader that polls the file while the application is
still streaming it must wait for that record: a partial file made of whole lines parses cleanly and
silently lacks whatever had not been walked yet (the first recapture runs lost the tab strip that way).

The command channel (`WM_COPYDATA`, `dwData` 2) also accepts two driver hooks while the probe is
armed: `menu-popup <Title>` pops one of the caption bar's menus (File, Edit, View, Objects,
Calibration, Help) and `invoke <label>` fires the first menu item whose label contains the text.
Both defer through `CallAfter`, so the sender's `SendMessage` returns before a popup loop or a
modal dialog blocks. Keystrokes and menu clicks do not reach a window on another desktop; these
hooks are how the headless driver reaches anything behind a menu.

`canvas-png <path>` saves the 3D canvas's next frame, ImGui panels included (gizmo panels,
notifications, the Daily Tips panel), as a PNG at the given path. On the real graphics driver,
PrintWindow gets nothing from the OpenGL surface, so a capture of an unmodified package shows the
canvas as a blank area (staging Mesa's software OpenGL beside the executable also works, but
changes the package); instead the canvas reads its frame back after the ImGui pass and before the
buffer swap, writes
`<path>.part` and renames it, so a driver waiting for the file never reads half of it. One
request saves one frame, and the canvas only draws while it is shown: switch to the Prepare or
Preview tab first. `scripts/md3/sweep-dialogs.py` uses it for its `canvas:` entries.

## Language audit

The command channel also accepts `language-audit`. When the app is in bilingual mode, it walks
every shown `wxStaticText` (including the kit `Label`), `wxButton` and the kit `Button`, `wxCheckBox`,
`wxRadioButton` and `wxStaticBox`, and classifies each one's label against `BilingualRegistry`:

- `bilingual`: the label already carries the Cantonese for its English part;
- `tooltip`: the label is English only, but the tooltip carries the Cantonese;
- `english_only`: a Cantonese translation exists but neither the label nor the tooltip shows it
  (this is the defect list);
- `no_translation`: the label is not a catalogue string at all (a number, a name); counted, not listed.

Text entry controls, combo boxes and list controls are never classified: they hold the user's own
data or a chosen value, not catalogue text, exactly as the bilingual decorator itself leaves them
untouched.

It writes `language-audit.json` beside the dumps: the language mode (`bilingual`, `cantonese` or
`english`), the registry size, totals per class, one entry per shown top-level window with its
class, title and counts, and the full `english_only` list (top-level class, control class, handle,
label capped at 200 characters, and whether a tooltip exists). Outside bilingual mode it writes
`{"mode": ..., "skipped": "not bilingual"}` instead and still reports success; the command only
fails when the JSON file itself cannot be written.

The bilingual decorator (`BilingualDecorator.cpp`) applies its own labels on a 250 ms timer and only
sweeps every shown window fully once every twelve ticks, about 3 seconds. A driver should wait at
least 4 seconds after a surface first shows before sending `language-audit`, or the report describes
a window the decorator has not reached yet.

What it cannot see: text an ImGui panel or another self-drawn widget paints itself, and anything
drawn straight onto the 3D canvas, carry no `wxWindow` label for this command to read at all.
For the canvas, `canvas-png` gives the pixels to check instead.

## Activation

| Setting | Effect |
| --- | --- |
| `BAMBU_LAYOUT_PROBE=1` | write `<data_dir>/log/layout-probe-<pid>-<n>.jsonl` |
| `BAMBU_LAYOUT_PROBE=<dir>` | write into that directory |
| `BAMBU_LAYOUT_PROBE_TAG=<text>` | copied into the header (name the tuple: scale, language, theme) |

A dump runs once after the main frame is first shown and idle, and again whenever the process
receives `WM_COPYDATA` with `dwData == 2` and payload `L"layout-probe [<path>]"`, which is how a
headless driver asks for a dump after opening a dialog. Unset, the cost is one environment read.

While the probe is on, the splash screen also saves the exact bitmap it shows as `splash.png` in the
same folder, once at startup. The splash lives for well under a second, so a screenshot of its window
comes back before it paints; the file is how a capture run checks the splash
([splash-release-date.md](../windows/splash-release-date.md)).

`scripts/md3/send-layout-probe.py <hwnd> <out.jsonl>` sends that message from the standard library
alone, given the main window handle a headless window list reports, and exits non-zero when no dump
appears within its timeout.

## Reading a dump

```bash
node ui-md3/tests/layout-probe-report.mjs <dump.jsonl> [more.jsonl] [--json]
```

Prints a findings table ordered by severity and exits non-zero when any finding is present, so a
capture run can gate on it. Hidden windows and hidden top-levels never count.

## Security and privacy

The dump contains window labels, which are user-visible strings, and window geometry. It never
contains file contents, credentials or project data beyond what a label shows. It is written only
when the environment variable is set.

## Verification

- Source contract: `ui-md3/tests/md3-conversion-contracts.test.mjs` asserts the gate, the
  `WM_COPYDATA` dispatch, the install call after the main frame shows, the CMake registration and
  every flag name.
- Runtime: the tuple matrix (100 / 125 / 150 / 200 percent, English / Cantonese / bilingual, light /
  dark, comfortable / compact) is run on the built artifact and its findings, fixes and before/after
  captures are recorded in `docs/features/design-system/clipping-inventory.md`.

## Suggested articles

- [Kit widgets added in the every-element sweep](kit-widgets-2026-09.md)
- [MD3 parity register](md3-parity-register.md)
- [Process settings sidebar](../prepare/process-settings-sidebar.md)
