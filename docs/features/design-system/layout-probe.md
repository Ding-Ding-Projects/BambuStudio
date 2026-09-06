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
- one `window` record per window in the tree: class (the wx class name, so `Label`, `Button`,
  `TextArea` rather than the Win32 class), name, label, rect, screen rect, client, min, best, shown,
  enabled, parent handle, the sizer item that owns it (proportion, flag, border, `CalcMin`,
  allocation) and the owning box sizer's verdict (orientation, available, required, oversubscribed).

Flags on each window record:

| Flag | Meaning |
| --- | --- |
| `starved` | a shown sizer child allocated less than its own minimum |
| `zero_sized` | a shown window with zero width or height |
| `oversubscribed` (in `sizer.row`) | the box sizer's children need more than it has |
| `text_clipped` | a label's text extent is wider than its client width and it has no ellipsize style |
| `ellipsized` | the label carries an ellipsize style (reported for review, not a finding) |
| `clipped_by_parent` | a shown window's rect leaves its parent's client area |

One `gl_item` record per visible item of the scene toolbar (`"toolbar":"main"`) and the gizmo rail
(`"gizmo"`): name, host canvas handle, rectangle in canvas pixels and on screen, derived from the
item's world-space render rectangle and the camera zoom. These are not wx windows, so no flag
applies; they exist so a capture can be cropped to a toolbar or rail item by name.

## Activation

| Setting | Effect |
| --- | --- |
| `BAMBU_LAYOUT_PROBE=1` | write `<data_dir>/log/layout-probe-<pid>-<n>.jsonl` |
| `BAMBU_LAYOUT_PROBE=<dir>` | write into that directory |
| `BAMBU_LAYOUT_PROBE_TAG=<text>` | copied into the header (name the tuple: scale, language, theme) |

A dump runs once after the main frame is first shown and idle, and again whenever the process
receives `WM_COPYDATA` with `dwData == 2` and payload `L"layout-probe [<path>]"`, which is how a
headless driver asks for a dump after opening a dialog. Unset, the cost is one environment read.

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
  captures are recorded in `docs/features/design-system/cheap-jor-inventory.md`.

## Suggested articles

- [Kit widgets added in the every-element sweep](kit-widgets-2026-09.md)
- [MD3 parity register](md3-parity-register.md)
- [Process settings sidebar](../prepare/process-settings-sidebar.md)
