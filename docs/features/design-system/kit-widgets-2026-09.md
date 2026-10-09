# Kit widgets added in the every-element sweep (2026-09-05)

Four kit widgets and one kit extension arrived so that no stock wxWidgets control had to remain on a
user-facing surface. Each mirrors an existing kit primitive so callers need no new vocabulary, and
each is pinned by a source-reading contract in `ui-md3/tests/md3-conversion-contracts.test.mjs`.

## LabeledRadioButton and RadioGroup (`src/slic3r/GUI/Widgets/LabeledRadioButton.{hpp,cpp}`)

**Behaviour.** A focusable row of the drawn `RadioBox` glyph plus a `Label`. Clicking the glyph, the
label or the row, or pressing Space / Enter while the row has focus, selects it and emits
`wxEVT_RADIOBUTTON` from the row (every user activation, like the native MSW control). A row on its
own can be selected but not deselected by the user. `RadioGroup` is a plain event handler the dialog
owns: `Add()` members, and it enforces mutual exclusion, moves selection and focus with Up / Left,
Down / Right, Home and End, skips hidden or disabled members, and reports `GetSelection() == -1`
when nothing is chosen. `SetSelection(-1)` clears the group programmatically.

**Accessibility.** The row carries a `wxAccessible` peer: role `ROLE_SYSTEM_RADIOBUTTON`, name from
the label, states focusable / focused / checked / unavailable / invisible, default action "Select".
A 2 px Primary focus ring is painted around the glyph. The glyph itself is not focusable.

**Configuration.** `SetColorScheme()` retints the selected dot to the Preview or Device accent;
`Rescale()` on DPI change; `SetLabel("")` hides the label for glyph-only rows.

**Failure modes.** A row added to two groups is ignored by the second `Add()`. Destroying a member
removes it from its group. Programmatic `SetValue()` never emits. A group that is a member of the
window owning its rows is destroyed before wx destroys those rows; its destructor unbinds every
handler it added, the destroy handler included, so no row event reaches the destroyed group.

**Sites.** FeedDirectionDialog, CalibrationWizardPresetPage (stage pair and per-slot selectors),
SavePresetDialog, the element lock wizard (unlock policy and duration), local support tickets
(category), Scheduled settings (value source).

## TextArea (`src/slic3r/GUI/Widgets/TextArea.{hpp,cpp}`)

**Behaviour.** The kit multi-line field: an outlined container hosting a borderless native editor
reached through `GetTextCtrl()`. OutlineVariant 1 px at rest, Primary 2 px while the editor has
focus, `radius_tiny` corners, SurfaceContainerLowest fill when editable and SurfaceContainerLow when
read-only. `SetMonospace(true)` switches the editor to Roboto Mono for JSON, logs and scripts.
`SetMinLines()` drives the best size when the parent does not size the field.

**Failure modes.** Callers that need `SetStyle()` or `wxTE_RICH` keep using them on the inner control.
The container does not scroll; the editor does.

**Sites.** Update dialog changelog, message-dialog script body, system-info JSON, network test log,
print-rating comment, both WebViewDialog developer viewers, unsaved-changes diff cells.

## ListBox (`src/slic3r/GUI/Widgets/ListBox.{hpp,cpp}`)

**Behaviour.** An owner-drawn `wxVListBox` painted with the DropDown menu anatomy: SurfaceContainer
field, rounded SurfaceContainerHigh hover pane, SecondaryContainer selected pane in the active
scheme, OnSurface text in the kit body face. Native keyboard model and `wxEVT_LISTBOX` are inherited.
Long rows ellipsize at the end and the hovered row exposes its full text as the tooltip.

**Extended selection.** Created with `wxLB_MULTIPLE`, the list replaces a `wxListBox` created with
`wxLB_EXTENDED`: `wxVListBox` supplies the extended model (a click selects one row, Ctrl+click
toggles a row, Shift+click and Shift+arrows extend the range, Space toggles the current row) and
`GetSelections()` returns the selected rows in ascending order in either mode, as
`wxListBox::GetSelections()` did. `SetSelection(n)` adds row `n` to a multiple selection, and
`Clear()` empties it.

**Failure modes.** The focus ring is drawn on selected rows. In a multiple-selection list, Ctrl+arrow
moves the current row without selecting it, and that row shows no ring until it is selected.

**Sites.** Smart home entity list; Scheduled settings rules (extended selection), available
settings, setting choices and timezones; authenticator entries, local support tickets and identity
history revisions (extended selection).

## Button::SetIconBitmap and ScalableBitmap(wxWindow*, wxBitmap)

`ScalableBitmap` can wrap a bitmap the caller already rendered (a colour swatch); it has no icon
name and `msw_rescale()` leaves it alone. `Button::SetIconBitmap()` shows such a bitmap in every
state, so colour swatches and other data images can live inside a kit icon Button without being
mistaken for an icon resource.

## Button is Material by default

A `Button` that reaches its first paint with neither `SetVariant()` / `SetIconButton()` nor caller
styling adopts the Outlined variant. Every explicit styling setter (background, border, text colour,
corner radius) marks the Button caller-styled, so hand-styled buttons keep their look. The Outlined
and Text variants define a Checked (selected) state: SecondaryContainer fill, OnSecondaryContainer
label.

The Outlined style brings its own label font and 18 DIP of padding on each side, so it usually widens
the button after its parent's sizer has already placed it. The first paint therefore compares the
minimum size before and after the style and, when it changed, queues one layout of the parent: one per
parent however many buttons restyle in the same pass, and only for a button a sizer places. Before
`e5faf503d` nothing asked again, and Smart home's Close button stayed squeezed below its minimum in English
and Cantonese mode (clipping inventory CJ-029).

## Verification

- `node --test ui-md3/tests/md3-conversion-contracts.test.mjs` pins every widget's registration,
  anatomy, accessibility role and the emptiness of the corresponding stock-control allowlist.
- `node --test ui-md3/tests/button-first-paint-layout.test.mjs` pins the parent layout after the
  first-paint style.
- Runtime captures (light and dark, EN / Cantonese / bilingual, 100 to 200 percent) are recorded in
  `docs/screenshots/md3-everything/` once the built artifact exists; until then the rows in
  `md3-parity-register.md` say so.

## Suggested articles

- [MD3 design system](md3-design-system.md)
- [MD3 parity register](md3-parity-register.md)
- [Themed surface colours](themed-surface-colors.md)
