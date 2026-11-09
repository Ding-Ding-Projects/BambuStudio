# Surface restyle handoff: Preview statistics, Slice and Print bar, Multi-slice dialog

Date: 2026-11-09. Status: design handoff only. Nothing in this file is built; no source, build or
capture was changed while writing it.

Three surfaces were rebuilt recently and read as generic: the statistics rows and TIME ESTIMATION
card in the Preview dock, the Slice and Print action group in the bottom bar, and the Multi-slice
dialog. This handoff restates them in the visual language of the November 2026 design boards
(`boards/kit.css`, `Main`, `FeatureFinder`, `VirtualDisks`, `InfoIcons`) so an implementer can
apply it in wxWidgets and ImGui without making taste decisions.

Sources read: `boards/kit.css`, the four boards above, `Widgets/MD3Tokens.hpp`,
`Widgets/Button.hpp` and `.cpp`, `Widgets/SideButton.cpp`, `Widgets/StaticBox.hpp`,
`Widgets/SearchField.cpp`, `Widgets/MD3Dialog.hpp`, `GCodeRenderer/BaseRenderer.cpp`
(`imgui_stat_row`, the legend dock and the estimation card), `MainFrame.cpp`
(`create_side_tools`, `place_side_actions`, `update_side_button_style`),
`MultiSlice/MultiSliceDialog.cpp`, `ui-md3/README.md`,
`docs/features/design-system/md3-design-system.md`, `md3-parity-register.md` and
`clipping-inventory.md`. Captures: `v19-preview.png`, `v19-prepare.png`, `v19-ms-fresh2.png`
(all light theme, bilingual language mode).

---

## 1. What the boards say, in eight rules

These are the traits that make the boards look like this product rather than a Material sample.
Every specification below is one of them applied.

1. **Soft rectangles, not stadiums.** Anything you press is a rounded rectangle whose radius is
   about 0.3 of its height: 12 px at 40 px (`.btn`, `.ib`), 10 px at 32 px (filter chips on Main),
   9 px at 30 px (`.chip`), 11 px at 38 px (`.in`). Full pills are kept for search fields
   (Main 40/20, FeatureFinder 44/22), switches and the navigation indicator only.
2. **Layered tonal containers without borders.** Page `Surface` (#0e1311) holds a card
   `SurfaceContainer` (#18201c, radius 20, padding 16, gap 10), which holds wells
   `SurfaceContainerLow` (#141a17, radius 14, padding 12, gap 8). Containers never carry an
   outline. Outlines belong to outlined buttons, chips and inputs, and are `OutlineVariant`
   (#34413a).
3. **Muted uppercase labels head every group.** `.lbl` is 11 px, weight 500, +0.08 em tracking,
   uppercase, `OnSurfaceVariant`, never with an icon. A count or note sits right-aligned on the
   same line in `.note` (12 px, `OnSurfaceVariant`).
4. **Muted labels, bright values.** In key/value tables (`.kv` on VirtualDisks) the label is
   `OnSurfaceVariant` and the value is `OnSurface`. Numbers and paths are Roboto Mono with tabular
   figures.
5. **One Filled action per row, at the end of the row.** VirtualDisks header: History and Attach
   (outlined), Quick pack (tonal), Create (filled). Utility actions such as Copy are text buttons
   (`.btn.q`) in `Primary`.
6. **Status is a small tag.** `.tag` is 11 px with 3/8 px padding and an 8 px radius, coloured by
   meaning (neutral, green, amber, red, blue).
7. **Selection is a fill.** A selected table row (`.tr.sel`) is a `SecondaryContainer` (#24342b)
   rounded rectangle (radius 12) with `OnSecondaryContainer` text. A selected chip or segment is
   the same tonal fill.
8. **Dense and even.** 40 px controls, 30 px chips, 38 px inputs, gaps of 8, 10, 12 and 14 px,
   13 px body text.

The board colours map one to one onto the dark roles in `MD3Tokens.hpp`
(`Dark::sc` #18201c, `Dark::scLow` #141a17, `Dark::scHigh` #1f2823, `Dark::scHighest` #28322c,
`Dark::outlineVariant` #34413a, `Dark::secondaryContainer` #24342b, `Dark::primary` #6fdc98,
`Dark::primaryContainer` #0f5130). The boards are dark only; light values in this handoff come
from resolving the same roles, which has not been reviewed by the owner (see open question Q10).

---

## 2. Diagnosis

### 2.1 Preview dock statistics and TIME ESTIMATION card (`v19-preview.png`)

- The dock is a stack of default widgets at equal weight: section headers carry leading glyphs
  (`COLOR SCHEME`, `OPTIONS`, `TIME ESTIMATION`) at the same visual weight as the content, so no
  level of the hierarchy recedes.
- The colour-scheme and option chips are full stadium outlines, exactly the Material sample chip.
  The one selected chip (Seams) is a saturated `Primary` fill and is the loudest thing in the dock.
- The ink table header puts `MODEL` left-aligned over a cell that holds two right-weighted values
  ("1.31 m 3.96 g"), and the values are in the proportional face while the statistics below are in
  Roboto Mono, so the table and the statistics look like two different components.
- In every statistic row the label is as bright as the value, and the Cantonese caption under the
  label gives each row a different height. Row pitch in the capture varies between about 26 and
  49 px, so there is no rhythm to scan.
- The TIME ESTIMATION block is a white rounded rectangle with a 1 px `Outline` ring: the default
  "card" from any toolkit. The boards never outline a container.

### 2.2 Slice and Print bar (`v19-prepare.png`, `v19-preview.png`)

- Four 44 px stadium buttons (Slice plate, Slice and Print, Slice and Send, Print plate) of nearly
  the same width and weight form a toolbar of generic pills. Nothing says which action is next.
- Each split button is two shapes: a pill-ended main half and a pill-ended options half meeting at
  a 2 px gap with 6 px inner corners. At 48 px wide the options half reads as a circle bolted to a
  pill.
- Outlined Slice uses an `Outline` ring on `SurfaceContainerHigh`; disabled Print is a grey filled
  pill. Both read as system defaults.
- The plate chip (radius 12, 40 px) is the one element already close to the boards, but its 2 px
  `Primary` ring is not board language (`.chip.on` has no ring).

### 2.3 Multi-slice dialog (`v19-ms-fresh2.png`)

- The native caption repeats the title ("Multi-slice" in the caption and "Multi-slice · 多重切片"
  as the heading), and the body sits on plain `Surface` with no container hierarchy.
- The ink list is a square-cornered rectangle with no row hover, no selected state other than the
  check box, and a large empty tail. The ink type on a second line doubles each row's height.
- Summary, destination and actions are three loose lines of text. The destination path is printed
  twice, once per language.
- Change folder and Close are outlined stadiums and Slice and save is a greyed filled stadium: the
  stock Material dialog.

---

## 3. Decision: pill radius versus board radius

**Conflict.** `MD3Tokens.hpp` (around line 511 and `pill_radius()` at line 528) says standalone
buttons and chips use a pill radius of height / 2. The boards use 12 px on 40 px buttons and 9 px
on 30 px chips.

**Decision for these three surfaces: the board radius wins.** Every pressable control on the three
surfaces uses a new `MD3::Metrics::soft_radius(height)`; `pill_radius()` stays for search fields,
switches and navigation indicators, and is not used anywhere on these three surfaces.

**Why.**

1. The boards are the owner's most recent design record (November 2026), and the owner's complaint
   is specifically about "stock Material pills". A pill on these surfaces would keep the exact
   trait that was rejected.
2. The token comment already does not describe the code. `Button::applyMD3Style()` sets
   `SetCornerRadius(std::min(FromDIP(height) / 2.0, FromDIP(active().radius)))`, which is 16 px on a
   42 px Medium button, not 21. `md3-design-system.md` (lines 72 to 76) still says
   `SetCornerRadius(FromDIP(height) / 2.0)`. Only `update_side_button_style()` and the dock chips
   (`FrameRounding 0.5f * chip_height`) actually draw full pills, and those are two of the three
   surfaces in question.
3. Applying the rule per surface, through a new token, does not change any other screen. The
   app-wide question is left open (Q5).

**New token.** In `namespace MD3::Metrics`:

| Token | Value | Notes |
| --- | --- | --- |
| `soft_radius(int height)` | `(3 * height + 5) / 10` | 40 → 12, 38 → 11, 36 → 11, 32 → 10, 30 → 9, 24 → 7. Pass the current DPI-scaled height, exactly like `pill_radius()`. |
| `action_height` | 40 | Board `.btn` and `.ib`. |
| `chip_height` | 30 | Board `.chip`. |
| `radius_card` | 20 | Alias of `radius_home`, for readability at call sites. |
| `radius_well` | 14 | Same value as `radius_icon_tile`; a separate name because a well is not an icon tile. |
| `radius_chip` | 9 | `soft_radius(chip_height)`, named for ImGui call sites that cannot call a constexpr helper with a scaled value conveniently. |
| `radius_tag` | 8 | Same value as `radius_tiny`. |
| `seam_inset` | 8 | Vertical inset of a split button's seam line. |

Update the comment at line 511 to say: standalone buttons and chips use `soft_radius(height)` on
surfaces that follow the November 2026 boards (currently the Preview statistics, the Slice and
Print bar and the Multi-slice dialog), and `pill_radius(height)` elsewhere until Q5 is settled.

**New button size.** `Button::Size::Action`: height 40, horizontal padding 16, label 13 px, glyph
20 px, glyph-to-label gap 8, radius `soft_radius(40)` = 12. `applyMD3Style()` must take the radius
from `soft_radius()` for this size only, so `Rescale()` keeps it. Label weight per variant is
given in 3.3.

---

## 4. Surface A: Preview dock statistics and the TIME ESTIMATION card

All values below are in design pixels and are multiplied by `m_scale` in ImGui. The dock shell
(position, width `sidebar_width`, `SurfaceContainerLow` background, `small_radius` corners, the
status header and the collapse control) does not change.

### 4.1 Order of the dock content

For the ColorPrint (Ink) view and the FeatureType (Line Type) view:

1. Status header ("Sliced · 100 layers"), unchanged.
2. `COLOR SCHEME` label and the scheme chips.
3. The view's own legend body (FeatureType: the line-type list; ColorPrint: nothing here).
4. `OPTIONS` label and the option chips (`render_options_chips()`).
5. **The summary well**, one container holding every statistic, described in 4.3.

In the ColorPrint view the per-ink table, Ink change times and Cost move out of the `switch`
body and into the summary well, after the option chips. Implementation note: collect their rows
while the `switch` runs and emit them inside the well afterwards. Other views keep their legend
bodies where they are and get the well with the time group only.

### 4.2 Group labels and chips in the dock

**Group label** (`COLOR SCHEME`, `OPTIONS`, and the labels inside the well): the existing
`imgui_section_header()` with **no glyph** (pass 0), bold face, `ImGui::SetWindowFontScale(0.80f)`
while drawing, colour `OnSurfaceVariant`, uppercase. 12 px of space above, 8 px below. Letter
spacing is not available in the ImGui atlas; that is accepted (see 7).

**Chips** (scheme and option chips, the code near `chip_height` around line 2230):

| Property | Value |
| --- | --- |
| Height | `MD3::Metrics::chip_height` (30) |
| Radius | `MD3::Metrics::radius_chip` (9); replace `0.5f * chip_height` |
| Horizontal padding | 12; with a glyph: 10 before the glyph, glyph 16, gap 6 |
| Gap between chips | 6 horizontal, 6 vertical |
| Text | regular face, font scale 0.92 |
| Rest | fill none (dock background shows), border 1 px `OutlineVariant`, text `OnSurface`, glyph `OnSurfaceVariant` |
| Hover | fill `SurfaceContainerHigh`, border `Outline` |
| Pressed | fill `SurfaceContainerHighest` |
| Selected | fill `SecondaryContainer` (Preview scheme), border same colour as the fill, text and glyph `OnSecondaryContainer` (Preview scheme); no `Primary` fill |
| Selected and hovered | `SecondaryContainer` with an 8 % `OnSecondaryContainer` state layer |
| Disabled | text and glyph `Outline`, border `SurfaceContainerHighest` |

The selected chip keeps the regular face so its width does not change when toggled.

### 4.3 The summary well

One container, drawn on draw-list channel 0 behind the content exactly as the estimation card is
drawn today.

| Property | Value |
| --- | --- |
| Horizontal extent | from `dock_x0 + 8` to `dock_x0 + dock_span − 8`; it must stay inside the clip rect (see CJ-034) |
| Fill | `SurfaceContainerLowest` in both themes (light #ffffff on the #eef3ef dock, dark #0a0e0c on the #141a17 dock): one step down the container ladder from the dock, the board's card-to-well relationship |
| Border | none. Remove the current 1 px `Outline` ring |
| Radius | `MD3::Metrics::radius_well` (14) |
| Padding | 12 left and right, 10 top, 12 bottom |
| Label column | starts at well left + 12 |
| Value edge | every value is right-aligned to well right − 12 (`right_edge`) |
| Row gap | 6 (the current `ItemSpacing.y`) |
| Group gap | 10, then a 1 px `OutlineVariant` hairline from well left + 12 to well right − 12, then 10 |

The well has two groups. In the ColorPrint view both appear; in other views only the time group
appears.

**Group 1, INK** (ColorPrint view only):

- Header row (`.tr.h` equivalent): group label `INK` at the label column. Each numeric column
  header (MODEL, SUPPORT, FLUSHED, TOWER, TOTAL) is right-aligned to its own column's right edge,
  in the group label style. Today `MODEL` is left-aligned over its values; that is the defect to
  fix.
- Column geometry: the last numeric column's right edge is the value edge. Each earlier column's
  right edge is the next column's left edge minus 16. A column's width is the larger of its header
  and its widest value line.
- Ink row: swatch 14 × 14, radius 4, the ink colour, with a 1 px `OutlineVariant` ring so white and
  near-background inks stay visible; 8 px gap; ink index in Roboto Mono, `OnSurface`. Values in
  Roboto Mono (`push_mono_font()`), `OnSurface`, each line right-aligned to its column. Keep the
  existing value strings: one line with length and weight when only the Model column shows, two
  lines (length over weight) when several columns show.
- Ink row hover (rows that have a click callback): a `SurfaceContainerHigh` rounded rectangle,
  radius 8, from well left + 4 to well right − 4, behind the row. Hidden ink: text
  `OnSurfaceVariant`, swatch at 40 % alpha. No hover highlight in `Primary`.
- After the last ink row, 8 px, then the two statistic rows Ink change times and Cost (4.4).

**Group 2, TIME** (every view that shows the estimate):

- Header row: the existing title (`Time Estimation` or `Total Estimation`) as a group label at the
  label column. When a mode switch exists ("Switch to silent mode" / "Switch to normal mode"), it
  sits on the same row, right-aligned to the value edge, as a text link: regular face, font scale
  0.86, `Primary` (Preview scheme), underline drawn only on hover. If title and link do not fit on
  one line (measure both; long languages and bilingual mode), the link moves to its own row after
  the last statistic, right-aligned. Replace the current `imgui.button()` for this switch.
- Rows, in the existing order: Total Filament, Model Filament and Cost (FeatureType view), Prepare
  time or Prepare and timelapse time, Model printing time, then the total row.
- Total row: 6 px extra space above it and a 1 px `OutlineVariant` hairline in that space (label
  column to value edge). Label in the bold face, `OnSurface`. Value in Roboto Mono, `Primary`
  (Preview scheme). This is the one accent in the well and marks the answer.

### 4.4 Statistic row (`imgui_stat_row`)

| Part | Specification |
| --- | --- |
| Label | regular face, `OnSurfaceVariant` (today `OnSurface`) |
| Value | Roboto Mono, `OnSurface`, right-aligned to `right_edge`, on the English line |
| Minimum label-to-value gap | 2 × `ItemSpacing.x`, as today |
| Cantonese caption (bilingual mode) | a second line under the label: regular face, font scale 0.86, `OnSurfaceVariant` at 80 % alpha so it sits below the English label in emphasis |
| Row pitch | uniform within a group. In bilingual mode, every row in the group reserves the caption line even when one label has no Cantonese, so rows never change height inside a group |
| Total row | see 4.3 |

English-only and Cantonese-only modes have one line per row. In Cantonese-only mode the label is
the Cantonese string at full scale.

### 4.5 Accessibility and fit

- `OnSurfaceVariant` on `SurfaceContainerLowest`: light #3f4c44 on #ffffff and dark #a9b6ad on
  #0a0e0c both exceed 7:1. `Primary` (Preview scheme) on the well must be checked for 4.5:1 in
  both themes before it is used for the total value; if it fails, use `OnSurface` in the bold face
  and keep the hairline as the only marker.
- At a narrow canvas the dock is clamped below 344 px. The label column takes the remaining width
  and long labels wrap before the value; the value never moves left of
  `label_end + 2 × ItemSpacing.x`. Keep the CJ-034 rule: nothing draws left of `dock_x0`.

---

## 5. Surface B: Slice and Print bar

All values are design pixels through `FromDIP()`. The bar keeps its height
(`prepare_actions_height`, 66), its `Surface` ground (`prepare_bar_surface()`) and the two-row
behaviour of `place_side_actions()`.

### 5.1 Composition and emphasis

| Control | Variant | Why |
| --- | --- | --- |
| Slice plate (split: main and options) | **Tonal** | Secondary emphasis: always the next step before a print, never the final commit |
| Slice and Print | **Outlined** | Combined shortcut; one of several routes |
| Slice and Send | **Outlined** | Same |
| Print plate (split: main and options) | **Filled** | The one committing action, last in the row (board rule 5) |

The emphasis does not swap with state. When Print is disabled (nothing sliced yet), Slice stays
Tonal; a promoted Slice would move weight around the bar every time the plate changes and break
the habit of finding Print at the end.

Order and spacing in `m_side_actions`: divider rule (1 × 22, `OutlineVariant`) with 12 px on each
side, then Slice split, 8, Slice and Print, 8, Slice and Send, 8, Print split. Replace the current
4/8 spacers and the `wxRIGHT, FromDIP(8)` borders with these values.

### 5.2 Common geometry

| Property | Value |
| --- | --- |
| Height | `MD3::Metrics::action_height` (40); today 44 |
| Outer radius | `MD3::Metrics::soft_radius(40)` = 12; today `pill_radius(44)` = 22 |
| Leading glyph | 20 px; Slice `DeployedCode`, Slice and Print `Print`, Slice and Send `Send`, Print `Print` (unchanged) |
| Padding | 14 before the glyph, gap 8, 16 after the label |
| Label | 13 px. Filled: weight 700 (`Label::Head_13` with `SetNumericWeight(700)`). Tonal: weight 500. Outlined: weight 400 (`Label::Body_13`) |
| Elevation | none. The `elev2` follow-up noted in `update_side_button_style()` is dropped; board buttons are flat |
| Focus | 2 px ring in `focus_ring_colour()`, inset 2 px, following the control's outer shape; on a split button the ring outlines only the focused half, with square corners on the seam side |

### 5.3 The split button

The pair is drawn as **one continuous shape with a hairline seam**, not two shapes with a gap.

| Part | Geometry |
| --- | --- |
| Main half | 40 high; outer (left) corners 12; seam-side (right) corners 0; width = 14 + 20 + 8 + label + 16 |
| Options half | 40 high, **36 wide**; outer (right) corners 12; seam-side (left) corners 0; glyph `ExpandMore` 20 px centred; `ExpandLess` while its menu is open |
| Gap between halves | 0. Replace `wxLEFT, FromDIP(2)` in `slice_sizer` and `print_sizer` |
| Seam | a 1 px vertical line on the main half's right edge, from `seam_inset` (8) below the top to 8 above the bottom |
| Outline (outlined state only) | drawn around the outer silhouette only; the seam side of each half has no outline stroke, the seam line replaces it |

`draw_split_segment()` already draws a shape with different leading and trailing radii; call it
with `inner = 0` instead of `FromDIP(6)`, skip the outline on the seam side, and draw the seam line
after the fill. Layout styles 2 and 3 stay the selectors for main and options halves.

Colours by state (Brand scheme on both the Prepare and Preview tabs; the bar is window chrome, not
canvas):

| State | Tonal Slice | Filled Print |
| --- | --- | --- |
| Rest fill | `SecondaryContainer` | `Primary` |
| Rest label and glyph | `OnSecondaryContainer` | `OnPrimary` |
| Seam | `OnSecondaryContainer` blended 20 % over the fill | `OnPrimary` blended 28 % over the fill |
| Hover (per half) | 8 % `OnSecondaryContainer` state layer | 8 % `OnPrimary` state layer; replaces the local `brighten(1.06)` so both bars use the same rule as `Button` |
| Pressed (per half) | 12 % state layer | 12 % state layer |
| Options menu open | the options half holds the pressed fill until the menu closes | same |
| Disabled (both halves) | see below | see below |

Outlined Slice and Print and Slice and Send (single, not split):

| State | Fill | Border | Label and glyph |
| --- | --- | --- | --- |
| Rest | none (bar `Surface`) | 1 px `OutlineVariant` | `OnSurface` |
| Hover | `SurfaceContainerHigh` | 1 px `Outline` | `OnSurface` |
| Pressed | `SurfaceContainerHighest` | 1 px `Outline` | `OnSurface` |
| Disabled | see below | | |

**Disabled appearance, for every control in the group including both split pairs:** fill none (the
bar `Surface` shows), border 1 px `SurfaceContainerHighest`, label and glyph `Outline`, seam
`SurfaceContainerHighest`. A disabled Print looks exactly like a disabled Slice: the filled colour
disappears entirely instead of turning grey. This matches the board's disabled button
(InfoIcons, "Already the built-in Mint": `Outline` text, `SurfaceContainerHighest` border).
Contrast of the disabled label: light #66766c on #f5f8f5 and dark #7c8a81 on #0e1311 are both
above 4.5:1, so the readability fix that introduced the 45 % blend is kept without the blend. The
local `blend()` lambda in `update_side_button_style()` is no longer needed.

### 5.4 The rest of the bar

| Control | Specification |
| --- | --- |
| Plate chip | 40 high, radius 12 (unchanged), fill `SecondaryContainer`, **no border** (today 2 px `Primary`), label `Label::Body_13` weight 500 `OnSecondaryContainer`, glyph `GridView` 20 `OnSecondaryContainer`, padding 12 / gap 8 / 14. Hover: 8 % `OnSecondaryContainer` state layer (today it swaps to `PrimaryContainer`). Disabled: as the disabled rule above |
| Add plate | 40 × 40, radius 12, 1 px dashed `Outline`, no fill, glyph `Add` 20 `OnSurfaceVariant`; hover `SurfaceContainerHigh`. Unchanged apart from confirming the values |
| Estimate, line 1 | duration in Roboto Mono 15/500 `OnSurface` (unchanged); "Not sliced" in `Label::Body_13` `OnSurfaceVariant` (today `Body_14`) |
| Estimate, line 2 | weight and length in `Label::Mono_12` `OnSurfaceVariant` (today `Body_11`): it is a number line, so it takes the number face |
| Estimate block | right-aligned, 12 px before the expand control, unchanged |
| Expand control | 34 px circular icon button, unchanged |

### 5.5 Bilingual labels

The captures show English-only labels on all four actions while the language mode is bilingual.
The rule for this bar: labels follow the app's compact bilingual pattern, English then " · " then
the Cantonese short label, inline, in the same face and colour. If that makes the group wider than
the bar, `place_side_actions()` already moves the whole group to the second row, right-aligned;
that is the accepted behaviour. Whether the bilingual decorator reaches `SideButton` today is not
established from the sources (Q2).

---

## 6. Surface C: Multi-slice dialog

### 6.1 Shell

Rebuild on `Slic3r::GUI::MD3Dialog` with `Options{ resizable = true }` (the dialog is resizable
today and must stay so). This removes the native caption and the duplicated title.

| Part | Specification |
| --- | --- |
| Surface | `SurfaceContainer` (the shell default) |
| Header | 44 × 44 icon tile, `PrimaryContainer`, radius 14, glyph `MaterialIcon::Stack` 24 `OnPrimaryContainer`; title "Multi-slice" 18/600 `OnSurface`; no subtitle (the intro carries the explanation); 36 px circular close |
| Body padding | 24 left and right (the shell default) |
| Body gap | 14 between every block (board `.dlg` gap) |
| Footer | the shell footer: 1 px `OutlineVariant` top border, padding 14 / 24 |
| Size | minimum 560 × 520, default 640 × 680 (unchanged) |
| Shadow | none can be drawn by the shell today (Q3) |

Closing with the circular close while a run is active keeps today's behaviour (veto, stop after
the current ink, restore inks).

### 6.2 Body, top to bottom

**1. Intro.** The existing text, `Label::Body_13`, `OnSurfaceVariant`, wrapping. In bilingual mode
the Cantonese paragraph follows with 4 px between, `Label::Body_12`, `OnSurfaceVariant`.

**2. Search.** The shared `SearchField`, full width, directly on the dialog surface (not inside the
well), so its `SurfaceContainerLow` fill reads as an input on a card, as `.in` does on the boards.
Its geometry (44 high, radius 16, `Outline` border, `Primary` focus ring) is not changed here
(Q1).

**3. Ink list well.**

| Property | Value |
| --- | --- |
| Container | `SurfaceContainerLow`, radius `radius_well` (14), padding 6 on all sides, no border. The scrolled window (`m_list`) is inside the well and shares its colour; the well's rounded shape is drawn by a `StaticBox` parent with `SetCornerRadius(FromDIP(14))` |
| Height | grows with the dialog (proportion 1); minimum 3 rows plus the header |
| Header row | 28 high, padding 0 / 12: `INK` at the name column, `STATUS` right-aligned to the status column; `Label::Head_11`, `OnSurfaceVariant`, uppercase |
| Row | 44 high (two-line failure rows grow), radius 12, padding 10 / 12, 2 px between rows |
| Row columns | check box 20 · gap 12 · name (grows) · gap 8 · type tag · gap 12 · status column (fixed 132, right-aligned) |
| Name | `Label::Body_13`, `OnSurface`, single line, ellipsised at the end, full name in the tooltip |
| Type | a neutral tag (6.3) on the same line as the name, replacing today's second line; omitted when the preset has no type |
| Rest | no fill |
| Hover | `SurfaceContainerHigh` |
| Checked | `SecondaryContainer`, name `OnSecondaryContainer`; hover over a checked row adds an 8 % `OnSecondaryContainer` state layer |
| Disabled (during a run) | no hover; check box disabled; checked rows keep their fill |
| Failed row | a second line under the name, `Label::Body_12`, `Error`, carrying the failure detail ("the plate did not slice with this ink"); the status tag shows only "Not saved" |
| Empty list | the existing message, `Label::Body_13` `OnSurfaceVariant`, centred in the well with 24 px padding, preceded by `MaterialIcon::Info` 20 `OnSurfaceVariant` |
| No search matches | same treatment, text: the app's existing no-results string if one exists (Q4) |

A click anywhere on the row toggles it, as today. The checked fill is the selection signal; the
check box stays for keyboard and assistive technology.

**4. Status column.** Each ink's status is a tag (6.3), with a 16 px glyph before the text:

| State | Tag colours | Glyph |
| --- | --- | --- |
| Idle | no tag | none |
| Waiting | neutral | `Schedule` |
| Slicing | tonal: `SecondaryContainer` / `OnSecondaryContainer` | `Sync` (static; no spin, so reduced motion needs no special case) |
| File saved | green: `PrimaryContainer` / `OnPrimaryContainer` | `CheckCircle` |
| Not saved | red: `ErrorContainer` / `OnErrorContainer` | `Error` |
| Stopped | neutral | `Stop` |

In bilingual mode the tag carries "English · Cantonese" as `show_status()` already builds it; the
status column widens to the longest tag, up to 180, and the name ellipsises first.

**5. Summary line.** A row: glyph 20 then `m_summary` text, `Label::Body_13`.

| State | Glyph | Text colour |
| --- | --- | --- |
| Fewer than two chosen | `Info`, `OnSurfaceVariant` | `OnSurfaceVariant` |
| Ready ("%d inks chosen …") | `TaskAlt`, `Primary` | `OnSurface` |
| Running ("Slicing %d of %d …") | `Sync`, `Primary` | `OnSurface`, plus the existing `ProgressBar` widget (8 high, radius 6, `SurfaceContainerHighest` track, `Primary` fill) under the text showing finished / queued |
| Stopping | `Stop`, `OnSurfaceVariant` | `OnSurfaceVariant` |

**6. Destination well.**

| Property | Value |
| --- | --- |
| Container | `SurfaceContainerLow`, radius 14, padding 12, no border |
| Line 1 | label `SAVE TO` (`Label::Head_11`, `OnSurfaceVariant`, uppercase); when the destination is the USB drive, a green tag "USB drive" after it, 8 px gap |
| Line 2 | the folder path once, in `Label::Mono_12`, `OnSurface`, single line, middle-ellipsised, full path in the tooltip; "No folder chosen yet." in `Label::Body_13` `OnSurfaceVariant` when empty |
| Action | "Change folder", Outlined, `Size::Action`, glyph `FolderOpen`, vertically centred on the right of the well, 12 px from the text |

This replaces the "Saving to %s" sentence and prints the path once in every language mode. In
bilingual mode only the label is paired ("SAVE TO · 存去").

### 6.3 Tag

| Property | Value |
| --- | --- |
| Height | 22 |
| Padding | 3 vertical, 8 horizontal; with a glyph, 6 before the glyph, glyph 16, gap 4 |
| Radius | `radius_tag` (8) |
| Text | `Label::Body_11`, weight 500 |
| Neutral | `SurfaceContainerHighest` / `OnSurfaceVariant` |
| Green | `PrimaryContainer` / `OnPrimaryContainer` |
| Tonal | `SecondaryContainer` / `OnSecondaryContainer` |
| Red | `ErrorContainer` / `OnErrorContainer` |

A tag is not interactive and never takes focus. Implement it as a `StaticBox` with a `Label`
child, or as an owner-drawn panel; the geometry above is what matters.

### 6.4 Footer and button variants

All dialog buttons use `Button::Size::Action` (40, radius 12).

| Button | Variant | Position | Why |
| --- | --- | --- | --- |
| Show files | **Text**, glyph `FolderOpen` | footer, left; shown only after at least one file is saved | A utility follow-up (board `.btn.q`); it must not compete with the committing action |
| Close | **Outlined** | footer, right, before the primary | Board secondary action in a dialog footer (InfoIcons: Restart now outlined, Done filled) |
| Stop (Close relabelled during a run) | **Danger** | same place | It abandons the remaining inks, so it takes the board's `.btn.d` treatment; switch the variant together with the label in `set_running()` |
| Slice and save | **Filled** | footer, far right | The one committing action, last in the row |
| Change folder | **Outlined** | in the destination well | A secondary choice next to the value it changes, as Browse in the VirtualDisks create dialog |

No Tonal button: the dialog has exactly one primary action and no second-tier action that needs
more weight than an outline. Slice and save disabled follows the `Button` Filled disabled state
(`SurfaceContainerHigh` fill, `Outline` label), which is the shared kit rule and is not changed
here.

Bilingual button labels stay inline ("Close · 關閉"), as in the capture.

---

## 7. Intentional deviations from stock Material Design 3

| Deviation | Stock Material Design 3 | Here | Reason |
| --- | --- | --- | --- |
| Button and chip shape | Common buttons fully rounded; chips 8 dp at 32 dp | `soft_radius(h)`: 12 at 40, 9 at 30 | The owner's boards; the owner rejected the stadium look |
| Outlined border at rest | `Outline` | `OutlineVariant`, `Outline` on hover | Boards; the label identifies the control, so the border is decoration. Input fields keep `Outline` for 3:1 boundary contrast |
| Disabled buttons | 12 % `OnSurface` container, 38 % label | No fill, `SurfaceContainerHighest` border, `Outline` label | Boards; keeps disabled labels readable (an earlier capture measured 1.7:1) |
| Filled label weight | 500 | 700 | Boards (`.btn.p`); separates the committing action by weight as well as colour |
| Split button | Two shapes, 2 dp gap, small inner corners | One shape with a 1 px inset seam | The two-shape version read as a pill plus a circle |
| Elevation on the primary action | `elev2` in the earlier kit note | None | Boards draw flat buttons |
| Section labels | Sentence-case title styles | Uppercase 11 px labels without icons | Boards (`.lbl`); the kit's leading glyphs on headers (`insights`) are dropped in the dock |
| Containers | Cards may be outlined | Tonal steps only, no outlines | Boards |
| Status | Assist chips or plain text | Small tags | Boards (`.tag`) |
| Selected chip | `SecondaryContainer` in M3 already; the app used `Primary` | `SecondaryContainer` | Brings the dock back to both M3 and the boards |
| Group label tracking | n/a | Not reproduced in ImGui | The ImGui atlas cannot express letter spacing; weight, size, colour and case carry the style |
| Dialog shadow | `elev5` | None | `MD3Dialog` cannot draw a shadow on a shaped or resizable borderless window (Q3) |

---

## 8. State inventory for the capture matrix

Every row is captured at 100 %, 125 %, 150 % and 200 % display scale unless noted. "All" for
language means English, Cantonese and bilingual. "Both" for theme means light and dark. Minimum
width is the narrowest width at which the row must be captured in addition to a 1920 × 1080 frame.

### 8.1 Preview dock

| Screen | State | Language | Theme | Minimum width |
| --- | --- | --- | --- | --- |
| Preview, Ink view | One ink, sliced, Model column only | All | Both | dock 344 |
| Preview, Ink view | Four inks with Support, Flushed, Tower and Total columns (two-line cells) | All | Both | dock 344 |
| Preview, Ink view | One ink hidden (muted row) and pointer over another ink row | English | Both | dock 344 |
| Preview, Ink view | Imperial units | English | Light | dock 344 |
| Preview, Line Type view | Total Estimation with Total Filament, Model Filament, Cost | All | Both | dock 344 |
| Preview, any view | Prepare and timelapse time present | English, bilingual | Both | dock 344 |
| Preview, any view | Mode switch link present (normal and silent differ) | All | Both | dock 344, and the width where the link moves to its own row |
| Preview, any view | Narrow canvas: dock clamped below 344, status in the dock header | All | Both | the narrowest frame the app allows |
| Preview, chips | Each chip at rest, hover, selected, selected and hovered, disabled | English | Both | dock 344 |
| Preview | Dock collapsed (no well drawn) | English | Both | dock 344 |

### 8.2 Slice and Print bar

| Screen | State | Language | Theme | Minimum width |
| --- | --- | --- | --- | --- |
| Prepare | Not sliced: Slice enabled, Print disabled | All | Both | 1366, and the width just below the two-row threshold |
| Prepare | Empty plate: every action disabled | All | Both | 1366 |
| Prepare | Sliced, back on Prepare: all enabled | All | Both | 1366 |
| Preview | Sliced and valid: Slice disabled, Print enabled | All | Both | 1366 |
| Prepare | Pointer over the Slice main half; then over the options half | English | Both | 1366 |
| Prepare | Pressed on the Print main half | English | Both | 1366 |
| Prepare | Slice options menu open (options half held pressed, `ExpandLess`) | English | Both | 1366 |
| Prepare | Keyboard focus on each half in turn (ring on one half only) | English | Both | 1366 |
| Prepare | Slice set to all plates and Print set to send (alternative labels) | All | Both | 1366 |
| Prepare | Action group on the second row | All | Both | the narrowest frame the app allows |
| Prepare | Plate chip at rest, hover, disabled; Add plate at rest and hover | English | Both | 1366 |

### 8.3 Multi-slice dialog

| Screen | State | Language | Theme | Minimum width |
| --- | --- | --- | --- | --- |
| Multi-slice | Opened, none chosen, Slice and save disabled | All | Both | 560 × 520 and 640 × 680 |
| Multi-slice | One ink chosen | English | Both | 560 |
| Multi-slice | Two or more chosen, ready | All | Both | 560 |
| Multi-slice | Pointer over a row; over a checked row | English | Both | 640 |
| Multi-slice | Keyboard focus on a check box and on each footer button | English | Both | 640 |
| Multi-slice | Search with matches; with regular expression mode on; with no matches | All | Both | 560 |
| Multi-slice | No inks fit the printer (empty well) | All | Both | 560 |
| Multi-slice | Running: Waiting, Slicing and File saved tags at once, progress bar, Stop button | All | Both | 560 |
| Multi-slice | Stopping | All | Both | 560 |
| Multi-slice | Finished, all saved, Show files visible | All | Both | 560 |
| Multi-slice | Finished with one Not saved row and its detail line | All | Both | 560 |
| Multi-slice | Destination is the USB drive (green tag) | All | Both | 560 |
| Multi-slice | No folder chosen; very long folder path (middle ellipsis, tooltip) | English | Both | 560 |
| Multi-slice | Ten or more inks (scrolling inside the well) | English | Both | 560 × 520 |

---

## 9. Open questions

- **Q1. Search field geometry.** The boards draw search fields as full pills (40/20 or 44/22). The
  shared `SearchField` is 44 high with radius 16, and its trailing padding was tuned to that shape
  (CJ-015). This handoff leaves it unchanged; a change belongs to an app-wide pass.
- **Q2. Bilingual labels on the bar.** The bilingual captures show English-only labels on
  `SideButton`. Is that a decorator gap or a deliberate choice? Section 5.5 specifies inline
  bilingual labels; the owner should confirm before the implementer widens the bar.
- **Q3. Dialog shadow.** The boards give dialogs a deep shadow (0 12 32, 45 % black).
  `MD3Dialog` cannot draw one on a shaped or resizable borderless window. A DWM shadow would need
  a separate decision.
- **Q4. New interface strings.** `INK`, `STATUS`, `SAVE TO`, `USB drive` and a no-matches message
  may not exist as translated strings. Each needs a Cantonese translation before it ships; reuse
  existing strings where the catalogue has them.
- **Q5. App-wide shape.** Should `soft_radius()` replace `pill_radius()` for every button and chip
  in the app? `md3-design-system.md` (lines 72 to 76) currently describes code that no longer
  exists (`Button` caps the radius at the density radius). That document needs correcting either
  way.
- **Q6. Light-theme well boundary.** A white well on the #eef3ef dock with no border has a faint
  edge. It is decorative grouping, not a control, so no contrast requirement applies, but the
  owner has only reviewed dark boards.
- **Q7. Total value colour.** `Primary` in the Preview scheme must pass 4.5:1 on the well in both
  themes; the Preview accent values were not checked here. The fallback is given in 4.5.
- **Q8. Bar scheme on Preview.** The bar uses the Brand scheme on both tabs (green Print on the
  Preview tab in the capture). The dock uses the Preview scheme. Confirm that window chrome should
  stay Brand.
- **Q9. Tag colour depth.** The boards' red tag (#3a1512 / #ffb4ab) is darker than the
  `ErrorContainer` role (#93000a in dark). This handoff uses the role. A dedicated tag palette
  would need new tokens for red, amber and blue tags.
- **Q10. Light theme overall.** All three boards are dark. Every light value here is derived from
  roles and has not been seen by the owner; the light rows of the capture matrix are the first
  review of it.
