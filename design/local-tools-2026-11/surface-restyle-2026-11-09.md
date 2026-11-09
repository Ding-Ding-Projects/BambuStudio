# Surface restyle handoff: Preview statistics, Slice and Print bar, Multi-slice dialog

Date: 2026-11-09. Status: design handoff only, revised the same day after an independent review
and three owner decisions (see the decision log). Nothing in this file is built; no source, build
or capture was changed while writing it.

Three surfaces were rebuilt recently and read as generic: the statistics rows and TIME ESTIMATION
card in the Preview dock, the Slice and Print action group in the bottom bar, and the Multi-slice
dialog. This handoff restates them in the visual language of the November 2026 design boards
(`boards/kit.css`, `Main`, `FeatureFinder`, `VirtualDisks`, `InfoIcons`) so an implementer can
apply it in wxWidgets and ImGui without making taste decisions. One rule from the boards, the
board radius, is applied to the whole application rather than only to the three surfaces; section
3 carries that rule and its full site inventory.

Sources read: `boards/kit.css`, the four boards above, `Widgets/MD3Tokens.hpp`,
`Widgets/Button.hpp` and `.cpp`, `Widgets/SideButton.cpp`, `Widgets/SideMenuPopup.cpp`,
`Widgets/StaticBox.hpp`, `Widgets/SearchField.cpp`, `Widgets/SwitchButton.cpp`,
`Widgets/PillView.hpp`, `Widgets/TabStrip.cpp`, `Widgets/MD3Dialog.hpp` and `.cpp`,
`BilingualDecorator.hpp`, `PersonalVocabulary.hpp`, `GCodeRenderer/BaseRenderer.cpp`
(`imgui_section_header`, `imgui_stat_row`, the legend dock, the chips, the estimation card and
`render_all_plates_stats`), `MainFrame.cpp` (`create_side_tools`, `place_side_actions`,
`update_prepare_action_bar_style`, `update_side_button_style`, the side popups),
`MultiSlice/MultiSliceDialog.cpp`, every pill site listed in 3.2 and 3.3, `ui-md3/README.md`,
`ui-md3/design-system/readme.md` and its `Button.jsx` and `Chip.jsx`,
`docs/features/design-system/md3-design-system.md`, `md3-parity-register.md`,
`studio-atlas-shared-controls.md` and `clipping-inventory.md`,
`docs/features/prepare/multi-slice.md`, `bbl/i18n/yue_HK/BambuStudio_yue_HK.po`, and the tests
named in section 10. Captures: `v19-preview.png`, `v19-prepare.png`, `v19-ms-fresh2.png` (all light
theme, bilingual language mode).

---

## Decision log

Decisions taken after the first draft of this handoff. Where this log and a later section seem to
disagree, this log wins.

| Date | Decision | Effect on this document |
| --- | --- | --- |
| 2026-11-09 | **D1. The board radius applies to the whole application** (owner). `MD3::Metrics::soft_radius(height)` is the shape of every pressable control in the application: buttons, chips, icon buttons, steppers and segment-like toggles. `pill_radius(height)` is kept only for the exemptions in 3.3. | Rule 1 and section 3 are rewritten as an app-wide rule; 3.2 lists every site that changes and 3.3 every site that stays a pill. Q5 is closed. Q1 is closed: the search field is in the exemption list, so the shared `SearchField` becomes a true pill (`kRadius` 16 becomes `pill_radius(44)` = 22); its trailing padding of 13 was already tuned for a 22 px end arc (CJ-015), so no padding change follows. |
| 2026-11-09 | **D2. The Slice and Print action bar stays English-only** (owner). The bar's labels never take the inline bilingual form; the Cantonese counterpart is carried by each button's tooltip and by the Every feature page. | The former section 5.5 (inline bilingual labels) and Q2 are removed. The new 5.5 specifies how the bar stays English-only. The 8.2 language column now proves the label never changes. |
| 2026-11-09 | **D3. Pressable icon buttons are soft squares** (owner). Every pressable icon button is `Button::IconShape::Square` with `soft_radius(container)`, including every `MD3Dialog` close button and the bar's expand control. Circles remain only for non-pressable round marks (swatches, status dots, radio and switch marks, avatars, progress rings). | Rule 1, 3.1, 3.2, 5.4 (expand control) and 6.1 (dialog close) change. Several tests that pin `IconShape::Circle` call sites turn red and are listed in section 10. |

Conflicts found while applying the review, and how they were settled (owner decision first, then
the blocking amendment):

- **Amendments 3 and 10 both replace the old 5.5.** Amendment 3 proposed a flag that skips both
  the inline bilingual form and the tooltip note; amendment 10 keeps the tooltip note. D2 says the
  tooltip carries the Cantonese, so the flag skips only the inline form and always fills the
  tooltip note (5.5).
- **Amendment 11 offered circular icon buttons as an alternative.** D3 chose soft squares; the
  alternative is not taken.
- **Amendment 2 asked whether the `Preferences.cpp:3529` pill, which it called the settings
  nav-item pill, stays a pill.** Checking the source shows it is the "Sample" Primary button in
  the Appearance density preview card, not a navigation item. It previews a button, so under D1 it
  migrates to `soft_radius` (3.2).
- **Amendments 2 and 9 both supply a pill inventory.** They are merged into the tables in 3.2 and
  3.3; amendment 9's list is the larger one. Its `Button.cpp:182` reference is line 186 in the
  current file.
- **Amendments 8 and 17 both cover the dialog shell surface.** Both allow either changing the
  surface or recording a deviation. The shell is shared by every `MD3Dialog`, so the surface and
  padding stay at the shell defaults and the difference from the board is recorded in section 7.
- **Amendment 6 asks to unify the disabled Filled look or list it as a deviation.** Unifying would
  change the shared `Button` disabled state for every dialog in the application, which no decision
  covers, so it is listed in section 7 and raised as Q11.

---

## 1. What the boards say, in eight rules

These are the traits that make the boards look like this product rather than a Material sample.
Every specification below is one of them applied.

1. **Soft rectangles, not stadiums, across the whole application.** Anything you press is a
   rounded rectangle whose radius is about 0.3 of its height, given by
   `MD3::Metrics::soft_radius(height)`: 12 px at 40 px (`.btn`, `.ib`), 10 px at 32 px (filter
   chips on Main), 9 px at 30 px (`.chip`), 11 px at 38 px (`.in`). This includes pressable icon
   buttons, which are soft squares (`IconShape::Square`, `soft_radius(container)`: 10 at 34, 11 at
   36, 12 at 40), the dialog close button among them. Full pills are kept only for the exemptions
   in 3.3: search fields (Main 40/20, FeatureFinder 44/22), switches, the navigation indicator and
   non-interactive count and status badges. Circles are kept only for marks you cannot press.
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

## 3. Decision: pill radius versus board radius, application-wide

**Conflict.** `MD3Tokens.hpp` (the comment at line 511 and `pill_radius()` at line 528) says
standalone buttons and chips use a pill radius of height / 2. The boards use 12 px on 40 px buttons
and 9 px on 30 px chips.

**Decision (D1).** `soft_radius(height)` is the shape rule for every pressable control in the
application: buttons, chips, icon buttons and steppers. `pill_radius(height)` is retained only for
the exemptions listed in 3.3.

**Why.**

1. The boards are the owner's most recent design record (November 2026), and the owner's complaint
   is specifically about "stock Material pills". A pill anywhere in the application keeps the exact
   trait that was rejected.
2. The token comment already does not describe the code. Variant buttons are already capped at the
   density radius, not drawn as pills: `Button::applyMD3Style()` (`Button.cpp:662`) sets
   `SetCornerRadius(std::min(FromDIP(height) / 2.0, FromDIP(active().radius)))`, which is 16 px on
   a 42 px Medium button in the comfortable density and 12 px in the compact density, not 21.
   `md3-design-system.md` (lines 72 to 76) still says `SetCornerRadius(FromDIP(height) / 2.0)`.
   Full pill radii are drawn by the sites listed in 3.2 and 3.3: the legacy `Button` default,
   circular icon buttons, the bar's split buttons, the dock chips and some 30 other feature-level
   sites. The kit still says pill as well (`ui-md3/design-system/readme.md` line 51, `Chip.jsx`,
   `Button.jsx`).

### 3.1 Tokens and the shared `Button`

**New tokens.** In `namespace MD3::Metrics`:

| Token | Value | Notes |
| --- | --- | --- |
| `soft_radius(int height)` | `(3 * height + 5) / 10` | 40 → 12, 38 → 11, 36 → 11, 34 → 10, 32 → 10, 30 → 9, 24 → 7. Pass the current DPI-scaled height, exactly like `pill_radius()`. At 30 px this gives 9 where the board's own segment buttons (`.seg button`, kit.css line 20) use 8; that one-pixel difference is accepted. |
| `action_height` | 40 | Board `.btn` and `.ib`. |
| `chip_height` | 30 | Board `.chip`. |
| `seam_inset` | 8 | Vertical inset of a split button's seam line. |
| `radius_card` | alias: `= radius_home` (20) | A second name for readability at call sites, declared as an alias of the existing constant, not a second literal. |
| `radius_well` | alias: `= radius_icon_tile` (14) | Same: a well is not an icon tile, but the value is one constant. |
| `radius_tag` | alias: `= radius_tiny` (8) | Same. |
| `radius_chip` | 9 | `soft_radius(chip_height)`, named for ImGui call sites that multiply by `m_scale` instead of calling a scaled helper. |

Rewrite the comment at `MD3Tokens.hpp` line 511 and the `pill_radius()` comment at lines 520 to
527 to say: standalone buttons and chips use `soft_radius(height)`; `pill_radius(height)` remains
for switches, search fields, navigation indicators and count badges.

**`Button`.**

- `Button::applyMD3Style()` uses `soft_radius(height)` for all four size tiers in place of
  `min(height / 2, active().radius)`: Small 36 gives 11, Medium 42 gives 13, Large 44 gives 13,
  Action 40 gives 12. `Rescale()` already re-runs `applyMD3Style()`, so the radius follows DPI.
- **New size `Button::Size::Action`**: height 40, horizontal padding 16 on both sides, label
  13 px, glyph 20 px, glyph-to-label gap 8, radius `soft_radius(40)` = 12. Label weight per variant
  is given in 5.2.
- The legacy non-variant default at `Button.cpp:186`
  (`SetDefaultCornerRadius(active().row_height / 2)`) becomes `soft_radius(row_height)`.
- **Icon buttons (D3).** The icon-button branch of `applyMD3Style()` (`Button.cpp:524` to `531`)
  sets `SetCornerRadius(FromDIP(MD3::Metrics::soft_radius(container)))` for `IconShape::Square`,
  and the Square width becomes `container`, a true square like `.ib` (today it adds 4 px of width).
  The default argument of `SetIconButton()` changes from `Circle` to `Square`, and every explicit
  `IconShape::Circle` caller (3.2) changes to `Square`. `IconShape::Circle` stays in the enum only
  until no pressable caller uses it; non-pressable round marks do not go through `Button`.

### 3.2 Sites that change

Every row moves from a pill or circle radius to `soft_radius` of its own height (or `container`
for icon buttons). Line numbers are from the current tree.

| Site | Control | Change |
| --- | --- | --- |
| `Widgets/Button.cpp:662` | every variant `Button` | `soft_radius(height)` (3.1) |
| `Widgets/Button.cpp:186` | legacy default `Button` radius | `soft_radius(row_height)` |
| `Widgets/Button.cpp:524-531` | icon buttons | Square, `soft_radius(container)`, width = container (D3) |
| every `SetIconButton(IconShape::Circle, ...)` and every `SetIconButton()` that relies on the default, about 60 call sites across `Plater.cpp`, `SearchField.cpp`, `ParamsPanel.cpp`, `ChangelogDialog.cpp`, `TabStrip.cpp`, `Preferences.cpp`, `MD3Dialog.cpp:306` and others | pressable icon buttons, including every dialog close | `IconShape::Square` (D3) |
| `Widgets/TabStrip.cpp:292`, `:887` | tab close and tab-strip action buttons (explicit `container / 2`) | `soft_radius(container)` |
| `Widgets/MD3Dialog.cpp:309-314` | fallback close button when glyphs are unavailable | 36 × 36, `soft_radius(36)` = 11 |
| `MainFrame.cpp:4313` (`update_side_button_style`) | Slice and Print split buttons | section 5 |
| `GCodeRenderer/BaseRenderer.cpp:2230` | dock scheme and option chips (`FrameRounding 0.5f * chip_height`) | section 4.2 |
| `CapsuleButton.cpp:183` | capsule button | `soft_radius(rect.height - 2)` |
| `HelioReleaseNote.cpp:1859`, `2141`, `3095`, `3330`, `3351`, `3682`, `3736` | Helio buttons (`pill_radius` of 28, 56, 24, 24, 24, 28, 40) | `soft_radius` of the same heights; the Helio brand palette is unchanged |
| `ReleaseNote.cpp:2784`, `2795` | Helio activate and uninstall buttons (fixed 18 and 14, commented "kit pill") | `soft_radius(36)` and `soft_radius(28)`; correct the comments |
| `ParamsPanel.cpp:175` | tips dialog button | `soft_radius(TIPS_DIALOG_BUTTON_SIZE.GetHeight())` |
| `MultiNozzleTpuTipsDialog.cpp:160`, `172` | guide and confirm buttons | `soft_radius(BUTTON_HEIGHT_DIP)` |
| `VersionPolicyDialog.cpp:364` | dialog button | `soft_radius(BUTTON_HEIGHT)` |
| `Plater.cpp:4594`, `5049`, `7671` | sync button and add-filament-row buttons (`row_height / 2`) | `soft_radius(row_height)` |
| `Widgets/SwitchButton.cpp:978-1009` (`CustomToggleButton::doRender`) | filter chip | `soft_radius(rect.GetHeight())` |
| `Widgets/CameraHUD.cpp:728` | zoom-percent chip button | `soft_radius(kChipDIP)` |
| `IMSlider.cpp:755` | speed chip hover fill | `soft_radius(chip_h)` |
| `GLCanvas3D.cpp:11458` | ImGui image-text buttons (`FrameRounding 0.5f * real_size.y`) | `soft_radius(real_size.y)` |
| `BBLTopbar.cpp:390` | update chip (pressable download chip) | `soft_radius(H)` |
| `ExtruderPanelGroup.cpp:420` | nozzle mode pill (a `ComboBox`, so pressable) | `soft_radius(MODE_PILL_H)` |
| `Widgets/MD3Menu.cpp:281` | stepper parts in menus | `soft_radius(part height)` |
| `Preferences.cpp:3529` | the "Sample" Primary button in the Appearance density preview card | `soft_radius(pill_h)`, so the preview shows the real rule |
| `Overview/AssemblyStepsUtilsImgui.cpp:6026` | footer buttons (`render_footer_button`) | `soft_radius(draw_size.y)` |
| `Overview/AssemblyStepsUtilsImgui.cpp:6355` | part-number toggle | `soft_radius(pill_h)` |
| `Overview/AssemblyStepsUtilsImgui.cpp:7620-7621` | re-record keyframe button | `soft_radius(pill_h)` |
| about 19 other `SetCornerRadius(x / 2)` sites found by `grep -rn "SetCornerRadius" src/slic3r/GUI \| grep "/ 2"` | buttons and chips | migrate each pressable one; any that is not pressable goes to 3.3 with its reason |

### 3.3 Exempt sites

These stay `pill_radius(height)` (or a circle) because the boards draw them that way or because
they cannot be pressed.

| Site | Why it stays |
| --- | --- |
| `Widgets/SearchField.cpp` (`kRadius`) | Search fields are pills on the boards (Main 40/20, FeatureFinder 44/22). Under D1 the field moves from 16 to `pill_radius(kHeight)` = 22; `kPadRight` 13 was tuned for that arc (CJ-015). |
| `SwitchButton` track and thumb | Switches are pills in Material and on the boards. |
| Navigation indicators (rail and tab active indicators) | The board's navigation indicator is a pill. |
| `IMSlider.cpp:1006` (error label chip on the layer slider) | A non-interactive label badge, not a control. |
| Count and status badges | Non-interactive; a tag (6.3) uses `radius_tag`, a numeric badge stays round. |
| Ink swatches, status dots, radio marks, avatars, progress rings | Round marks that are not pressed (D3). |
| Segmented controls (`SwitchBoard`, `MultiSwitchButton`) | Already fixed-radius tracks, not pills; unchanged. |

### 3.4 New widget capabilities

The specifications in sections 4 to 6 assume capabilities that do not exist yet. Each is new work:

| # | Capability | Owner file | Needed by |
| --- | --- | --- | --- |
| 1 | `Button::Size::Action`, per-variant label weight in `applyMD3Style()`, `soft_radius(height)` for every size, Square icon buttons at `soft_radius(container)` | `Widgets/Button.hpp`, `.cpp` | 3.1, 5, 6.4 |
| 2 | A shared state-layer function outside `Button.cpp`'s anonymous namespace, for example `MD3::stateLayer(surface, foreground, opacity)`, moved from `buttonStateLayer` (`Button.cpp:28-159`) and used by `Button`, the bar's `SideButton` styling and the plate chip. `MainFrame` cannot call the anonymous helper today. | new `Widgets/MD3StateLayer.hpp` (or `MD3Tokens.hpp`) | 5.3, 5.4 |
| 3 | `SideButton`: `SetSplitSeam(colourStates, inset)`; per-side stroke for split halves (fill the path, stroke only the open outline, since `draw_split_segment` at `SideButton.cpp:395-424` strokes all four sides with `dc.GetPen()`); a configurable glyph gap (`SideButton.cpp:511` is a literal `FromDIP(6)`, 5.2 needs 8); a configurable focus-ring inset (`SideButton.cpp:595` is `max(FromDIP(3), 1)`, 5.2 needs 2) and a split-aware focus path with square seam-side corners (`SideButton.cpp:602` draws a full rounded rectangle); a held pressed state; a radius-0 branch in the path builder (emit `AddLineToPoint` instead of `AddArcToPoint` when the radius is below 1e-5, because a zero-radius arc is degenerate in the wx graphics path); and the English-only flag in 5.5 | `Widgets/SideButton.hpp`, `.cpp` | 5.2, 5.3, 5.5 |
| 4 | A dismiss notification from `SidePopup` (a callback or a wx event fired from `SidePopup::OnDismiss`, `SideMenuPopup.cpp:43`) so `MainFrame` can release the held options half and restore `ExpandMore`; the popups are created and deleted in `MainFrame.cpp:3890-3938` | `Widgets/SideMenuPopup.hpp`, `.cpp` | 5.3 |
| 5 | A tag widget: extend `PillView` (`Widgets/PillView.hpp`, today text-only with a fixed radius and no glyph) with an optional leading glyph and a soft shape at `radius_tag`, or add an owner-drawn `Tag` | `Widgets/PillView.*` or new `Widgets/Tag.*` | 6.2, 6.3 |
| 6 | A row widget derived from `StaticBox` with rounded hover and checked fills and child hover forwarding: today rows are plain `wxPanel` (`MultiSliceDialog.cpp:157`), and entering a child label or tag fires a leave event on the row. The existing code already binds `LEFT_UP` per child (lines 187-190); enter and leave need the same forwarding. | `MultiSlice/MultiSliceDialog.cpp` or a shared widget | 6.2 |
| 7 | ImGui helpers: a draw-list chip (glyph, padding and label drawn as separate items so 10/16/6 spacing holds; one `ImGui::Button` holding "glyph, two spaces, label" cannot do that, `BaseRenderer.cpp:2221-2240`); a right-aligned multi-line cell (today `append_item` draws left-aligned cells at `SameLine` offsets and relies on `\n` inside one string); a wrapped-label stat row (today `imgui_stat_row`, lines 95-122, does not wrap); and a text link with an underline drawn only on hover (reuse the "View Summary" link pattern near `BaseRenderer.cpp:2807-2814`) | `GCodeRenderer/BaseRenderer.cpp` (helpers in its anonymous namespace) | 4.2, 4.3, 4.4, 4.5 |

### 3.5 Kit, documentation and inventory updates in the same change

- `ui-md3/design-system/readme.md` line 51 ("full pills for standalone buttons/chips"):
  soft radius for pressable controls, pills for the 3.3 exemptions.
- `ui-md3/design-system/components/selection/Chip.jsx` (`borderRadius: h / 2`), `Chip.prompt.md`
  and `components/actions/Button.jsx` (`borderRadius: h / 2`): soft radius.
- `docs/features/design-system/md3-design-system.md` lines 72 to 76: describe `soft_radius`, the
  exemptions and the soft-square icon buttons, not `SetCornerRadius(FromDIP(height) / 2.0)`.
- `md3-parity-register.md`: every row that describes a "pill" button or chip, including the rows at
  lines 62 (`generic-buttons`) and 134 (`customtogglebutton-chip-anatomy`), and the plate-chip row
  at line 216 (its 2 px `Primary` border is removed in 5.4).
- `studio-atlas-shared-controls.md` line 12 ("explicit circles remain circles"): pressable icon
  buttons are soft squares.
- The `.yue_HK.md` twin of every article above, with its `source-sha256` updated (section 10).

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

In the ColorPrint view the per-ink table, the ink change count and Cost move out of the `switch`
body and into the summary well, after the option chips. Other views keep their legend bodies where
they are and get the well with the time group only.

**How the ink table moves.** Today the ink rows are produced inside the ColorPrint case of the
legend `switch` through `append_item` with absolute `SameLine` offsets, optionally inside
`ImGui::BeginChild("color_prints")` when the list scrolls (`BaseRenderer.cpp:2681`), and each row
is a `BBLMenuItem` for the click; the estimation card is drawn after `render_options_chips()`
(line 2840) with `ChannelsSplit(2)` around a group (lines 3127 to 3245). The move is:

1. During the `switch`, ink rows are collected into a vector of `{index, colour, visible,
   cells[], callback}` instead of being drawn. One function emits them inside the well.
2. Column widths are measured in a first pass over every row and every header, then the rows are
   drawn in a second pass.
3. When the ink list is taller than the current `child_height`, the INK group scrolls in a child
   window whose clip rectangle stays inside the well (well left + 4 to well right − 4); the well
   itself does not grow past the dock.
4. The value edge and the label column are window-local x positions, while the well is drawn in
   screen space from `dock_x0`. Convert with
   `right_edge = (well_right − 12) − ImGui::GetWindowPos().x + ImGui::GetScrollX()`, because the
   dock window is horizontally scrollable (`ImGuiWindowFlags_HorizontalScrollbar`, line 1750).
5. The well replaces the card block at lines 3123 to 3245, and the source contract that pins a
   dock-spanning card is updated to the new inset (section 10).

The 8 px inset is intentional: the well sits inside the dock as a board well sits inside a card,
so it no longer shares its left and right edges with the header plate, which keeps running from
`dock_x0` to `dock_x0 + dock_span`.

**All-plates statistics.** `render_all_plates_stats` (`BaseRenderer.cpp:957-1358`) draws a second
ink table, "Total cost" and a per-plate "Time Estimation" block with `imgui.title` (line 1332) and
the old `append_item(false, ...)` rows. It uses the same summary well, group labels and stat rows:
an INK group built from the all-plates totals and a TIME group with one row per plate and a total
row. Without this, the same table would exist in two styles.

The only `imgui_stat_row` call sites that move are lines 2788 and 2791 (ink change count and Cost,
into the INK group) and line 3177 (the time rows, into the TIME group); the "Time Estimation"
`imgui.title` at line 1332 becomes a group label.

### 4.2 Group labels and chips in the dock

**Group label** (`COLOR SCHEME`, `OPTIONS`, and the labels inside the well): the existing
`imgui_section_header()` with **no glyph** (pass 0), bold face, `ImGui::SetWindowFontScale(0.80f)`
while drawing, colour `OnSurfaceVariant`, uppercase. 12 px of space above, 8 px below. Letter
spacing is not available in the ImGui atlas; that is accepted (see 7). `imgui_section_header()`
already uppercases ASCII after the catalogue lookup, so the existing mixed-case msgids are kept
(see 6.5). In bilingual mode the group labels stay English only, as they do today; in
Cantonese-only mode they show the Cantonese string (no case change applies).

**Chips** (scheme and option chips, `BaseRenderer.cpp:2214-2280`):

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

No chip is ever disabled in this code, so there is no disabled chip state. The selected chip keeps
the regular face so its width does not change when toggled (a deviation from the board's
`.chip.on` weight 500, listed in section 7).

**Chip geometry and wrapping.** The wrap predictor and the drawn chip must agree. `chip_width()`
(line 2226) uses the same padding (12, or 10 + 16 + 6 with a glyph), the same 1 px border and the
same font scale as the drawing code, measured with `ImGui::SetWindowFontScale(0.92f)` in force.
The glyph is drawn as a separate draw-list item, or the whole chip is one custom draw-list item
(3.4, capability 7), so the 10/16/6 spacing holds; the current "glyph, two spaces, label" string
inside one `ImGui::Button` (`chip_content`, line 2221) is replaced.

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
| Value edge | every value is right-aligned to well right − 12 (`right_edge`, converted as in 4.1) |
| Row gap | 6 (the current `ItemSpacing.y`) |
| Group gap | 10, then a 1 px `OutlineVariant` hairline from well left + 12 to well right − 12, then 10 |

The well has two groups. In the ColorPrint view both appear; in other views only the time group
appears.

**Group 1, INK** (ColorPrint view and all-plates statistics):

- Header row (`.tr.h` equivalent): group label `INK` at the label column. Each numeric column
  header (MODEL, SUPPORT, FLUSHED, TOWER, TOTAL) is right-aligned to its own column's right edge,
  in the group label style. Today `MODEL` is left-aligned over its values; that is the defect to
  fix.
- Column geometry: the last numeric column's right edge is the value edge. Each earlier column's
  right edge is the next column's left edge minus 16. A column's width is the larger of its header
  and its widest value line (measured in the first pass, 4.1).
- Ink row: swatch 14 × 14, radius 4, the ink colour, with a 1 px `OutlineVariant` ring so white and
  near-background inks stay visible; 8 px gap; ink index in Roboto Mono, `OnSurface`. Values in
  Roboto Mono (`push_mono_font()`), `OnSurface`, each line right-aligned to its column (3.4,
  capability 7). Keep the existing value strings: one line with length and weight when only the
  Model column shows, two lines (length over weight) when several columns show.
- Ink row hover (rows that have a click callback): a `SurfaceContainerHigh` rounded rectangle,
  radius 8, from well left + 4 to well right − 4, behind the row. Hidden ink: text
  `OnSurfaceVariant`, swatch at 40 % alpha. No hover highlight in `Primary`.
- After the last ink row, 8 px, then the two statistic rows for the ink change count (msgid
  "Filament change times", shown through `PersonalVocabulary::display`) and Cost (4.4).

**Group 2, TIME** (every view that shows the estimate):

- Header row: the existing title (`Time Estimation` or `Total Estimation`) as a group label at the
  label column. When a mode switch exists ("Switch to silent mode" / "Switch to normal mode"), it
  sits on the same row, right-aligned to the value edge, as a text link: regular face, font scale
  0.86, `Primary` (Preview scheme), underline drawn only on hover. If title and link do not fit on
  one line (measure both; long languages and bilingual mode), the link moves to its own row after
  the last statistic, right-aligned. Replace the current `imgui.button()` for this switch.
- Rows, in the existing order: Total Filament, Model Filament and Cost (FeatureType view), Prepare
  time or Prepare and timelapse time, Model printing time, then the total row. In all-plates
  statistics: one row per plate, then the total row.
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
  `label_end + 2 × ItemSpacing.x`. Wrapping needs a measure pass before the row is drawn
  (3.4, capability 7). Keep the CJ-034 rule: nothing draws left of `dock_x0`.

---

## 5. Surface B: Slice and Print bar

All values are design pixels through `FromDIP()`. The bar keeps its height
(`prepare_actions_height`, 66), its `Surface` ground (`prepare_bar_surface()`) and the two-row
behaviour of `place_side_actions()`.

**Where the styling lives.** The bar is restyled in three places, and all three must change:

- `update_side_button_style()` (`MainFrame.cpp:4303`) styles the four action buttons. It is
  re-run on DPI change (line 4523) and theme change (line 4618), so the blended seam and
  state-layer colours are recomputed there and need no `gDarkColors` keys.
- The plate chip and the add-plate button are restyled in two places: `create_side_tools`
  (`MainFrame.cpp:3498-3534`) and `update_prepare_action_bar_style` (`MainFrame.cpp:2530-2559`).
  The second re-applies the 2 px `Primary` border, the `PrimaryContainer` hover and the radius on
  every style pass and DPI change, so editing only `create_side_tools` is undone by the first
  style pass. Change both, or reduce the first to a call of the second.

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
| Padding | 16 before the glyph, gap 8, 16 after the label (the `Size::Action` padding and the board `.btn` `0 16px`) |
| Label | 13 px. Filled: weight 700 (`Label::Head_13` with `SetNumericWeight(700)`). Tonal: weight 500. Outlined: weight 400 (`Label::Body_13`) |
| Elevation | none. The `elev2` follow-up noted in `update_side_button_style()` is dropped; board buttons are flat |
| Focus | 2 px ring in `focus_ring_colour()`, inset 2 px, following the control's outer shape; on a split button the ring outlines only the focused half, with square corners on the seam side (3.4, capability 3) |

### 5.3 The split button

The pair is drawn as **one continuous shape with a hairline seam**, not two shapes with a gap.

| Part | Geometry |
| --- | --- |
| Main half | 40 high; outer (left) corners 12; seam-side (right) corners 0; width = 16 + 20 + 8 + label + 16 |
| Options half | 40 high, **36 wide**; outer (right) corners 12; seam-side (left) corners 0; glyph `ExpandMore` 20 px centred; `ExpandLess` while its menu is open |
| Gap between halves | 0. Replace `wxLEFT, FromDIP(2)` in `slice_sizer` and `print_sizer` |
| Seam | a 1 px vertical line on the main half's right edge, from `seam_inset` (8) below the top to 8 above the bottom |
| Outline (outlined state only) | drawn around the outer silhouette only; the seam side of each half has no outline stroke, the seam line replaces it |

`draw_split_segment()` already draws a shape with different leading and trailing radii; call it
with `inner = 0` instead of `FromDIP(6)`, add the radius-0 branch so the seam-side corners are
straight lines rather than degenerate arcs, skip the outline on the seam side, and draw the seam
line after the fill (3.4, capability 3). Layout styles 2 and 3 stay the selectors for main and
options halves.

Colours by state (Brand scheme on both the Prepare and Preview tabs; the bar is window chrome, not
canvas):

| State | Tonal Slice | Filled Print |
| --- | --- | --- |
| Rest fill | `SecondaryContainer` | `Primary` |
| Rest label and glyph | `OnSecondaryContainer` | `OnPrimary` |
| Seam | `OnSecondaryContainer` blended 20 % over the fill | `OnPrimary` blended 28 % over the fill |
| Hover (per half) | 8 % `OnSecondaryContainer` state layer | 8 % `OnPrimary` state layer; replaces the local `brighten(1.06)` so both bars use the same rule as `Button`, through the shared state-layer function (3.4, capability 2) |
| Pressed (per half) | 12 % state layer | 12 % state layer |
| Options menu open | the options half holds the pressed fill and shows `ExpandLess` until the menu closes; the close is reported by the `SidePopup` dismiss notification (3.4, capability 4) | same |
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

Contrast of the disabled label, measured from `MD3Tokens.hpp`: `Outline` on the bar `Surface` is
4.49:1 in light (#66766c on #f5f8f5) and 5.19:1 in dark (#7c8a81 on #0e1311). WCAG 2.2 success
criterion 1.4.3 does not apply to inactive controls, so 4.5:1 is not a requirement here; the target
for disabled labels in this handoff is at least 3:1, which every disabled label below meets. The
readability fix that introduced the 45 % blend is kept without the blend, and the local `blend()`
lambda in `update_side_button_style()` is no longer needed.

### 5.4 The rest of the bar

| Control | Specification |
| --- | --- |
| Plate chip | 40 high, radius 12 (unchanged), fill `SecondaryContainer`, **no border** (today 2 px `Primary`), label `Label::Body_13` weight 500 `OnSecondaryContainer`, glyph `GridView` 20 `OnSecondaryContainer`, padding 12 / gap 8 / 14 (chip padding, board `.chip`, not the Action button padding). Hover: 8 % `OnSecondaryContainer` state layer through the shared state-layer function (today it swaps to `PrimaryContainer`). Disabled: as the disabled rule above. Change both restyle sites named at the top of section 5 |
| Add plate | 40 × 40, radius 12, 1 px dashed `Outline`, no fill, glyph `Add` 20 `OnSurfaceVariant`; hover `SurfaceContainerHigh`. Unchanged apart from confirming the values |
| Estimate, line 1 | duration in Roboto Mono 15/500 `OnSurface` (unchanged); "Not sliced" in `Label::Body_13` `OnSurfaceVariant` (today `Body_14`) |
| Estimate, line 2 | weight and length in `Label::Mono_12` `OnSurfaceVariant` (today `Body_11`): it is a number line, so it takes the number face |
| Estimate block | right-aligned, 12 px before the expand control, unchanged |
| Expand control | 34 × 34 **soft square** icon button (D3): `IconShape::Square`, `soft_radius(34)` = 10; today a 34 px circle |

### 5.5 Language: the action labels stay English (D2)

The four action labels stay English in English, Cantonese and bilingual language modes as far as
the bilingual decoration is concerned: the bar never shows the inline "English · Cantonese" form.

- Today `SideButton::dorender` already runs `I18N::fit_bilingual(text_dc, text, avail, &note)` at
  paint time (`SideButton.cpp:579-591`). `messureSize()` sizes on the English alone, so the compact
  bilingual form has never fitted and the Cantonese has gone to the tooltip note. The new padding
  (16 after the label) could leave enough room for the inline form to appear, which D2 forbids, so
  the behaviour is made explicit rather than left to the measured room.
- Add a `SideButton` flag, for example `SetEnglishOnly(true)`, set on the four action buttons
  (main halves and the single buttons; options halves carry no label). With the flag set,
  `dorender` draws the English label without calling `fit_bilingual` for the inline form, and in
  bilingual mode always merges the Cantonese counterpart into the tooltip note, using the existing
  `bilingual_note` and `bilingual_base_tooltip` path at lines 586 to 590.
- Do not change `messureSize()` to measure the bilingual form and do not widen the group;
  `place_side_actions()` needs no bilingual case.
- The Every feature page carries the Cantonese name of each action alongside the English, so the
  pairing remains discoverable without the tooltip.
- In Cantonese-only mode the labels are the catalogue's Cantonese strings, as for every other
  control in that mode; the English-only rule governs the bilingual decoration. This reading of D2
  is raised for confirmation as Q12.
- The Slice and Print dropdown rows (`SideButton` children of `SidePopup`) are unaffected and keep
  today's bilingual handling.

---

## 6. Surface C: Multi-slice dialog

### 6.1 Shell

Rebuild on `Slic3r::GUI::MD3Dialog` with `Options{ resizable = true }` (the dialog is resizable
today and must stay so). This removes the native caption and the duplicated title.

| Part | Specification |
| --- | --- |
| Surface | `SurfaceContainer` (the shell default, `MD3Dialog.cpp:162`). The board `.dlg` is `SurfaceContainerHigh` (#1f2823); the shell default is kept because the shell is shared by every dialog (section 7) |
| Header | 44 × 44 icon tile, `PrimaryContainer`, radius 14, glyph `MaterialIcon::Stack` 24 `OnPrimaryContainer`; title "Multi-slice" 18/600 `OnSurface`; no subtitle (the intro carries the explanation); 36 × 36 **soft square** close (D3: `IconShape::Square`, `soft_radius(36)` = 11, set once in `MD3Dialog.cpp:306` for every dialog) |
| Body padding | 24 left and right (the shell default; the board `.dlg` uses 22, section 7) |
| Body gap | 14 between every block (board `.dlg` gap) |
| Footer | the shell footer: 1 px `OutlineVariant` top border, padding 14 / 24 |
| Size | minimum 560 × 520, default 640 × 680 (unchanged) |
| Window corners | square. The resizable shell skips the `wxFRAME_SHAPED` rounded silhouette (`MD3Dialog.hpp` lines 53 to 59), so there are no 28 px outer corners (section 7, Q3) |
| Shadow | none can be drawn by the shell today (Q3) |

**Closing during a run.** `MD3Dialog::OnHeaderClose()` defaults to `EndModal(wxID_CANCEL)`
(`MD3Dialog.cpp:437`), which does not pass through `wxEVT_CLOSE_WINDOW`, while
`MultiSliceDialog` vetoes a close during a run only in its `wxEVT_CLOSE_WINDOW` handler
(`MultiSliceDialog.cpp:89-99`). The dialog is shown as a stack temporary
(`MultiSliceDialog(this).ShowModal()`, `MainFrame.cpp:5131`), so `EndModal` mid-run would destroy
it while slicing and leave the inks changed, breaking the promise in
`docs/features/prepare/multi-slice.md` line 60 ("Stop, or closing the window, finishes the ink
being sliced"). Therefore `MultiSliceDialog` overrides `OnHeaderClose()` to call `Close()`, so the
existing veto runs: stop after the current ink, restore the inks, then close. Escape goes through
the same path.

### 6.2 Body, top to bottom

**1. Intro.** The existing text, `Label::Body_13`, `OnSurfaceVariant`, wrapping. In bilingual mode
the Cantonese paragraph follows with 4 px between, `Label::Body_12`, `OnSurfaceVariant`.

**2. Search.** The shared `SearchField`, full width, directly on the dialog surface (not inside the
well), so its `SurfaceContainerLow` fill reads as an input on a card, as `.in` does on the boards.
Its geometry is the shared one: 44 high, `Outline` border, `Primary` focus ring, and after D1 a
full pill (radius 22, 3.3).

**3. Ink list well.**

| Property | Value |
| --- | --- |
| Container | `SurfaceContainerLow`, radius `radius_well` (14), padding 6 on all sides, no border. The scrolled window (`m_list`) is inside the well and shares its colour; the well's rounded shape is drawn by a `StaticBox` parent with `SetCornerRadius(FromDIP(14))` |
| Height | grows with the dialog (proportion 1); minimum 3 rows plus the header |
| Header row | 28 high, padding 0 / 12: `INK` at the name column, `STATUS` right-aligned to the status column; `Label::Head_11`, `OnSurfaceVariant`, uppercase (strings in 6.5) |
| Row | 44 high (two-line failure rows grow), radius 12, padding 10 / 12, 2 px between rows; a `StaticBox`-derived row with child hover forwarding (3.4, capability 6) |
| Row columns | check box 20 · gap 12 · name (grows) · gap 8 · type tag · gap 12 · status column (fixed 132, right-aligned) |
| Name | `Label::Body_13`, `OnSurface`, single line, ellipsised at the end, full name in the tooltip |
| Type | a neutral tag (6.3) on the same line as the name, replacing today's second line; omitted when the preset has no type |
| Rest | no fill |
| Hover | `SurfaceContainerHigh` |
| Checked | `SecondaryContainer`, name `OnSecondaryContainer`; hover over a checked row adds an 8 % `OnSecondaryContainer` state layer |
| Disabled (during a run) | no hover; check box disabled; checked rows keep their fill. Names keep their rest colours (`OnSurface` unchecked, `OnSecondaryContainer` checked) so the rows stay readable next to their live status tags; only the check box shows the disabled state |
| Failed row | a second line under the name, `Label::Body_12`, `Error`, carrying the failure detail ("the plate did not slice with this ink"); the status tag shows only "Not saved" |
| Empty list | the existing message, `Label::Body_13` `OnSurfaceVariant`, centred in the well with 24 px padding, preceded by `MaterialIcon::Info` 20 `OnSurfaceVariant` |
| No search matches | same treatment, with the existing msgid "No matches." (yue_HK "一個匹配都冇。") |

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
| Line 1 | label `SAVE TO` (`Label::Head_11`, `OnSurfaceVariant`, uppercase); when the destination is the USB drive, a green tag "USB drive" after it, 8 px gap (strings in 6.5) |
| Line 2 | the folder path once, in `Label::Mono_12`, `OnSurface`, single line, middle-ellipsised, full path in the tooltip; "No folder chosen yet." in `Label::Body_13` `OnSurfaceVariant` when empty |
| Action | "Change folder", Outlined, `Size::Action`, glyph `FolderOpen`, vertically centred on the right of the well, 12 px from the text |

This replaces the "Saving to %s" sentence and prints the path once in every language mode.

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

A tag is not interactive and never takes focus. Implement it by extending `PillView` with an
optional leading glyph and the soft `radius_tag` shape, or as a new owner-drawn `Tag` (3.4,
capability 5); the geometry above is what matters.

### 6.4 Footer and button variants

All dialog buttons use `Button::Size::Action` (40, radius 12).

| Button | Variant | Position | Why |
| --- | --- | --- | --- |
| Show files | **Text**, glyph `FolderOpen` | footer, left; shown only after at least one file is saved | A utility follow-up (board `.btn.q`); it must not compete with the committing action |
| Close | **Outlined** | footer, right, before the primary | Board secondary action in a dialog footer (InfoIcons: Restart now outlined, Done filled) |
| Stop (Close relabelled during a run) | **Danger** | same place | It abandons the remaining inks, so it takes the board's `.btn.d` treatment; switch the variant together with the label in `set_running()` |
| Slice and save | **Filled** | footer, far right | The one committing action, last in the row |
| Change folder | **Outlined** | in the destination well | A secondary choice next to the value it changes, as Browse in the VirtualDisks create dialog |

**Footer placement.** The shell footer is right-aligned by a leading stretch spacer
(`MD3Dialog.cpp:345-349`), and `AddFooterButton()` appends with a left border. Show files is
therefore inserted at index 0 of `GetFooterSizer()`, before the stretch spacer and without the
`AddFooterButton()` left border; Close and Slice and save are added with `AddFooterButton()`.

No Tonal button: the dialog has exactly one primary action and no second-tier action that needs
more weight than an outline. Slice and save disabled follows the shared `Button` Filled disabled
state (`SurfaceContainerHigh` fill, `Outline` label), which is the shared kit rule and is not
changed here. Its label contrast is 3.84:1 in light (#66766c on #dfe8e2) and 4.19:1 in dark
(#7c8a81 on #1f2823), above the 3:1 disabled target in 5.3. The bar's disabled Filled has no fill
(5.3), so one Filled role has two disabled looks; this is listed in section 7 and raised as Q11.

Bilingual button labels stay inline ("Close · 關閉"), as in the capture. D2 applies only to the
action bar.

### 6.5 Strings

| String | Source | Cantonese | Bilingual form |
| --- | --- | --- | --- |
| `INK` (list header) | existing msgid "Ink" | 墨水 | "INK · 墨水" |
| `STATUS` (list header) | existing msgid "Status" | 狀態 | "STATUS · 狀態" |
| `SAVE TO` (destination label) | new msgid "Save to" | 存去 | "SAVE TO · 存去" |
| `USB drive` (tag) | new msgid "USB drive" | USB 手指 | "USB drive · USB 手指" |
| No search matches | existing msgid "No matches." | 一個匹配都冇。 | the shared label's usual pairing |

- **Uppercase after the lookup.** `I18N::bilingual_secondary` and the bilingual decoration look up
  the original English msgid, so uppercasing before the lookup (for example
  `_L("Ink").Upper()`) loses the Cantonese pair. Look up the mixed-case msgid first, then
  uppercase the English part only for display, and build the bilingual header text as the
  uppercased English, " · ", then `bilingual_secondary` of the original. This is the same order
  `imgui_section_header()` already uses in the dock.
- The Cantonese for "Save to" and "USB drive" reuses the wording already in the catalogue
  ("Saving to %s" is "存去 %s"; "Saving to the USB drive %s" is "存去 USB 手指 %s").
- The two new msgids are added to the `.pot` and to `bbl/i18n/yue_HK/BambuStudio_yue_HK.po`.
- In bilingual mode the header cells and the tag are wider than in English; the status column
  widening rule in 6.2 (up to 180) covers the `STATUS` header as well as the tags.
- Every new string is displayed through `PersonalVocabulary::display`, as the existing labels are.
- Dock group labels (`COLOR SCHEME`, `OPTIONS`, `INK`, `TIME ESTIMATION`) stay English-only in
  bilingual mode (4.2).

---

## 7. Intentional deviations from stock Material Design 3 and from the boards

| Deviation | Stock Material Design 3 or board | Here | Reason |
| --- | --- | --- | --- |
| Button and chip shape | Common buttons fully rounded; chips 8 dp at 32 dp | `soft_radius(h)` application-wide: 12 at 40, 9 at 30 (D1) | The owner's boards; the owner rejected the stadium look |
| Icon button shape | Material icon buttons are circular | Soft squares, `soft_radius(container)` (D3) | Board `.ib` (40 × 40, radius 12) |
| Segment height 30 radius | Board `.seg button` uses 8 | `soft_radius(30)` = 9 | One rule instead of a special case; one pixel |
| Outlined border at rest | `Outline` | `OutlineVariant`, `Outline` on hover | Boards; the label identifies the control, so the border is decoration. Input fields keep `Outline` for 3:1 boundary contrast |
| Disabled buttons on the bar | 12 % `OnSurface` container, 38 % label | No fill, `SurfaceContainerHighest` border, `Outline` label | Boards; keeps disabled labels readable (an earlier capture measured 1.7:1) |
| Disabled Filled in the dialog | Board: no fill | Shared `Button` rule: `SurfaceContainerHigh` fill, `Outline` label | The shared rule covers every dialog; changing it is outside this handoff (Q11). One Filled role has two disabled looks |
| Filled label weight | 500 | 700 | Boards (`.btn.p`); separates the committing action by weight as well as colour |
| Selected chip weight | Board `.chip.on` weight 500 | Regular weight | The chip's width does not change when toggled, so the wrap layout does not jump |
| Bar leading padding | Board `.btn` 16 | 16 (matches) | Recorded because an earlier draft used 14 |
| Split button | Two shapes, 2 dp gap, small inner corners | One shape with a 1 px inset seam | The two-shape version read as a pill plus a circle |
| Elevation on the primary action | `elev2` in the earlier kit note | None | Boards draw flat buttons |
| Section labels | Sentence-case title styles | Uppercase 11 px labels without icons | Boards (`.lbl`); the kit's leading glyphs on headers (`insights`) are dropped in the dock |
| Containers | Cards may be outlined | Tonal steps only, no outlines | Boards |
| Status | Assist chips or plain text | Small tags | Boards (`.tag`) |
| Selected chip fill | `SecondaryContainer` in M3 already; the app used `Primary` | `SecondaryContainer` | Brings the dock back to both M3 and the boards |
| Group label tracking | n/a | Not reproduced in ImGui | The ImGui atlas cannot express letter spacing; weight, size, colour and case carry the style |
| Action bar language | Every label bilingual in bilingual mode | English labels, Cantonese in the tooltip (D2) | Owner decision; keeps the bar's width and order stable |
| Dialog surface | Board `.dlg` #1f2823 (`SurfaceContainerHigh`) | `SurfaceContainer` | Shell default shared by every `MD3Dialog` |
| Dialog padding | Board `.dlg` 22 | 24 | Shell default |
| Dialog window corners | Board `.dlg` radius 28 | Square | The resizable shell cannot be shaped (Q3) |
| Dialog shadow | `elev5`; board 0 12 32, 45 % black | None | `MD3Dialog` cannot draw a shadow on a shaped or resizable borderless window (Q3) |

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
| Preview, Ink view | Enough inks to scroll the INK group inside the well | English | Both | dock 344 |
| Preview, Ink view | Imperial units | English | Light | dock 344 |
| Preview, Line Type view | Total Estimation with Total Filament, Model Filament, Cost | All | Both | dock 344 |
| Preview, any view | Prepare and timelapse time present | English, bilingual | Both | dock 344 |
| Preview, any view | Mode switch link present (normal and silent differ) | All | Both | dock 344, and the width where the link moves to its own row |
| Preview, any view | Narrow canvas: dock clamped below 344, status in the dock header, long labels wrapping | All | Both | the narrowest frame the app allows |
| Preview, any view | Dock scrolled horizontally (values stay on the well's value edge) | English | Both | the narrowest frame the app allows |
| Preview, all plates | All-plates statistics: INK totals and per-plate TIME rows with the total row | All | Both | dock 344 |
| Preview, chips | Each chip at rest, hover, pressed, selected, selected and hovered; a wrap to a second chip row | English | Both | dock 344 |
| Preview | Dock collapsed (no well drawn) | English | Both | dock 344 |

### 8.2 Slice and Print bar

The language column proves the label never changes: in every multi-language row the expectation
is "English label unchanged, group width identical across modes, Cantonese note present in the
tooltip in bilingual mode". "English, Cantonese, bilingual (labels identical)" is written as
"E/C/B".

| Screen | State | Language | Theme | Minimum width |
| --- | --- | --- | --- | --- |
| Prepare | Not sliced: Slice enabled, Print disabled | E/C/B | Both | 1366, and the width just below the two-row threshold |
| Prepare | Empty plate: every action disabled | E/C/B | Both | 1366 |
| Prepare | Sliced, back on Prepare: all enabled | E/C/B | Both | 1366 |
| Preview | Sliced and valid: Slice disabled, Print enabled | E/C/B | Both | 1366 |
| Prepare | Tooltip on Slice plate and on Print plate in bilingual mode (Cantonese note shown) | bilingual | Both | 1366 |
| Prepare | Pointer over the Slice main half; then over the options half | English | Both | 1366 |
| Prepare | Pressed on the Print main half | English | Both | 1366 |
| Prepare | Slice options menu open (options half held pressed, `ExpandLess`); then closed by Escape and by a click outside (held state released, `ExpandMore`) | English | Both | 1366 |
| Prepare | Keyboard focus on each half in turn (ring on one half only, square on the seam side) | English | Both | 1366 |
| Prepare | Slice set to all plates and Print set to send (alternative labels) | E/C/B | Both | 1366 |
| Prepare | Action group on the second row | E/C/B | Both | the narrowest frame the app allows |
| Prepare | Plate chip at rest, hover, disabled; Add plate at rest and hover; expand control (soft square) at rest and hover | English | Both | 1366 |
| Prepare | DPI change and theme change with the bar visible (seam and state-layer colours recomputed) | English | Both | 1366 |

### 8.3 Multi-slice dialog

| Screen | State | Language | Theme | Minimum width |
| --- | --- | --- | --- | --- |
| Multi-slice | Opened, none chosen, Slice and save disabled | All | Both | 560 × 520 and 640 × 680 |
| Multi-slice | One ink chosen | English | Both | 560 |
| Multi-slice | Two or more chosen, ready | All | Both | 560 |
| Multi-slice | Pointer over a row; over a checked row; over the type tag inside a row (row hover kept) | English | Both | 640 |
| Multi-slice | Keyboard focus on a check box, on each footer button and on the soft-square close | English | Both | 640 |
| Multi-slice | Search with matches; with regular expression mode on; with no matches | All | Both | 560 |
| Multi-slice | No inks fit the printer (empty well) | All | Both | 560 |
| Multi-slice | Running: Waiting, Slicing and File saved tags at once, progress bar, Stop button | All | Both | 560 |
| Multi-slice | Header close during a run: stops after the current ink, restores the inks, then closes | English | Both | 560 |
| Multi-slice | Escape during a run: same behaviour as the header close | English | Light | 560 |
| Multi-slice | Stopping | All | Both | 560 |
| Multi-slice | Finished, all saved, Show files visible on the left of the footer | All | Both | 560 |
| Multi-slice | Finished with one Not saved row and its detail line | All | Both | 560 |
| Multi-slice | Destination is the USB drive (green tag) | All | Both | 560 |
| Multi-slice | No folder chosen; very long folder path (middle ellipsis, tooltip) | English | Both | 560 |
| Multi-slice | Ten or more inks (scrolling inside the well) | English | Both | 560 × 520 |

### 8.4 Application-wide shape sweep (D1, D3)

One pass, 100 % and 200 % scale, English, both themes:

| Screen | State |
| --- | --- |
| A test panel or existing screens showing the shared `Button` in each size tier (Small 36, Medium 42, Large 44, Action 40) and each variant | rest and hover, radius 11, 13, 13 and 12 |
| Three unrelated dialogs, for example the Helio release note, the version policy dialog and Preferences | footer buttons soft, close button a soft square |
| The Preferences Appearance density preview | the Sample button follows the soft radius in both densities |
| Any `SearchField` (Prepare sidebar, Preferences) | full pill, trailing icon buttons clear of the end arc (CJ-015) |
| The camera footer play button and a tab strip with close buttons | soft squares, footer fit unchanged |

---

## 9. Open questions

- **Q1. Search field geometry.** Closed by D1: search fields are in the exemption list, so the
  shared `SearchField` becomes a full pill (radius 22 at 44 px). Its trailing padding was already
  tuned for that arc (CJ-015).
- **Q2.** Removed by D2 (the action bar stays English-only; 5.5).
- **Q3. Dialog shadow and corners.** The boards give dialogs a deep shadow (0 12 32, 45 % black)
  and a 28 px silhouette. The resizable `MD3Dialog` has neither: it cannot be shaped, so its outer
  corners are square, and no shell variant draws a shadow. A DWM shadow and DWM corner rounding
  would need a separate decision.
- **Q4. New interface strings.** Closed by 6.5: existing msgids are reused for `INK`, `STATUS` and
  the no-matches message; "Save to" and "USB drive" are new, with their Cantonese.
- **Q5. App-wide shape.** Closed by D1.
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
- **Q11. One disabled look for Filled.** The bar's disabled Filled has no fill (board style); the
  shared `Button` Filled disabled keeps a `SurfaceContainerHigh` fill. Should the shared `Button`
  adopt the board's disabled style application-wide, as D1 did for shape?
- **Q12. Cantonese-only mode on the action bar.** D2 names bilingual mode. This handoff shows the
  catalogue's Cantonese labels in Cantonese-only mode (5.5). Confirm, or state that the bar shows
  English in that mode too.
- **Q13. Width of square icon buttons.** `IconShape::Square` today adds 4 px of width ("the kit
  window-control shape"); D3 and `.ib` call for true squares, so this handoff drops the extra
  width. About ten existing Square callers (for example the camera footer play button, colour
  pickers and Helio copy and save icons) lose 4 px of width. Confirm, or keep the wider shape for
  window-caption controls only.

---

## 10. Contracts and tests

Implementation lands together with the matching edits to these checks. Each one is updated to
assert the new behaviour, never deleted, and each updated check keeps a negative case that fails
when the old behaviour returns. Every entry below was checked against the current file.

| Check | What it asserts today | Why it turns red | New expectation |
| --- | --- | --- | --- |
| `tests/prepare_chrome_surfaces.test.mjs`, `checkActionBar` (lines 41 to 54) | Exactly two `SetBottomColour(bar_bg)` calls in `update_side_button_style` ("both pill styles take the bar ground"); no other `SetBottomColour`; no `SurfaceContainerLow` | Tonal, Outlined and Filled styles make three or more | Route every action button through one style helper that calls `SetBottomColour(bar_bg)` once, and assert that count is 1 and that all four buttons pass through the helper. Keep the negative assertions on other corner colours and `SurfaceContainerLow` |
| `tests/native_shared_controls/atlas_control_anatomy.test.mjs`, `checkButtonStates` (lines 48 to 59) | `applyMD3Style` contains `MD3::Metrics::active().radius` (line 57) and `MD3::Metrics::active().small_radius` (line 58) | Both are replaced by `soft_radius` (3.1, D3) | Assert `MD3::Metrics::soft_radius(` for the variant path and for the Square icon-button path; add a negative case that substituting `pill_radius` or `/ 2.0` fails |
| `ui-md3/tests/preview-overlays.test.mjs`, "the dock blocks span the dock and the grouping card is sized from its content" (lines 101 to 112) | `stats_min` at `dock_x0`, `stats_max` at `max(dock_x0 + dock_span, ...)`, the outlined card | The well is inset 8 and has no outline (4.3) | Assert the well spans `dock_x0 + 8 * m_scale` to `dock_x0 + dock_span − 8 * m_scale`, fills `SurfaceContainerLowest`, draws no outline; keep the negative check that nothing starts left of the clip rect |
| `ui-md3/tests/device-popup-atlas.test.mjs:53`, `ui-md3/tests/lane-image-tiles.test.mjs:75`, `ui-md3/tests/lane-text-fields.test.mjs:109`, `ui-md3/tests/nozzle-rack-atlas.test.mjs:51` | Specific call sites use `IconShape::Circle` | D3 moves pressable icon buttons to `Square` | Assert `IconShape::Square` at the same call sites, with a negative case for `Circle` |
| `ui-md3/tests/lane-scalable-button.test.mjs:195` | `SetIconButton(container > 36 ? Square : Circle, container)` | D3 makes every pressable icon button Square | Assert `SetIconButton(Button::IconShape::Square, container)` |
| `ui-md3/tests/monitor-atlas-dpi.test.mjs` (lines 26 to 38) | Square width is `container + 4` (camera play button 36 wide at container 32) | 3.1 makes Square width equal to `container` (Q13) | Expected width 32 at every scale; the footer-fit assertion is kept |
| `scripts/i18n/check_translated_content.py` | Every edited `docs/features` article has a `.yue_HK.md` twin whose `source-sha256` matches | 3.5 edits `md3-design-system.md`, `md3-parity-register.md`, `studio-atlas-shared-controls.md`, and any CJ added to `clipping-inventory.md` | Update each twin's text and `source-sha256` in the same change |
| `scripts/documentation/build_bundle.py --check` | `Documentation/DocumentationBundle.hpp` matches the articles | The bundle embeds the edited articles | Regenerate the bundle in the same change |
| `scripts/i18n/Test-LanguageMissingReport.ps1` | Missing translations are reported | Two new msgids (6.5) | Both new msgids carry yue_HK translations |
| `scripts/i18n/test_untranslated_literals.py` | No raw prose literals reach label setters | New labels and headers | Every new string goes through `_L` (and `PersonalVocabulary::display`); the uppercase transform is applied after the lookup, never to a literal |

The capture matrix in section 8, including the application-wide sweep in 8.4, is the visual
acceptance for these changes; source contracts alone do not prove the drawn result.
