# Design system

This category documents how the native wxWidgets/OpenGL application consumes the vendored Material
Design 3 design system.

- [Current native interface audit](native-interface-audit-2026-10-02.md): active menu,
  motion, personal-vocabulary and slicing-action requirements, with explicit pending
  hosted verification and privacy boundaries.

- [Vendored Material Design 3 design system](md3-design-system.md) — token source of truth, the
  ground-up color/type/metric migration, contextual schemes, fonts, failure modes, and the parity
  audit result.
- [MD3 parity register](md3-parity-register.md) — the canonical element-by-element conformance
  register and wave plan driving the structural-anatomy migration. The register itself carries the
  live done / deviation / open counts; consult it rather than any snapshot elsewhere.
- [Gizmo rail SVG completion](gizmo-rail-svg-icons-completion.md) — the bounded 34-asset MD3-token
  overlay for the remaining semantic gizmo composites, with native-runtime evidence still required
  before the parity row can move from partial to done.

- [Kit widgets added in the every-element sweep](kit-widgets-2026-09.md) — LabeledRadioButton and
  RadioGroup, TextArea, ListBox, Button::SetIconBitmap, and the Material-by-default Button.
- [Runtime layout probe](layout-probe.md) — the off-by-default NDJSON walker that finds starved
  sizer rows, zero-sized controls and clipped labels mechanically, and its report reader.
- [Layout clipping inventory](clipping-inventory.md) — every found clipping defect with its tuple,
  cause, fix commit and capture pair; machine-checked so a row cannot claim evidence it lacks.
- [Themed surface colors on StaticBox cards](themed-surface-colors.md) — how a card gets its fill,
  why `SetBackgroundColorNormal()` could silently do nothing, and the stale constructor-time window
  background behind light plates in dark mode.
- [Generated visual showcase](generated-visual-showcase.md) — the image suite shared by the
  interactive app, GitHub Pages landing page, and social preview, including loading, accessibility,
  deployment, and verification behavior.
- [Context menus](context-menus.md): every context menu is the Material menu, including the text
  fields' edit menu from the mouse and the keyboard, copyable labels and web pages, and how a released
  package is checked for a system menu.
- [Tooltips](tooltips.md): every tooltip is the Material plain tooltip, in both themes, and how a
  released package is checked for the system's.
- [Dialogs and pickers](dialogs-and-pickers.md): the Material dialogs that replaced wxWidgets' stock
  prompts, choosers, busy notice and colour dialog, the recently used colours, and what stays native.
- [Native controls on the kit](native-controls.md): the kit replacements for a disabled button's tip,
  the web pages' notice bar and the Workspace panel's tabs, tables, checklist and calendar, the one
  Material style every table takes, and the kit scrollbar in every scrolled page, panel, list and table.

## Studio Atlas implementation receipts

These articles record bounded source changes and their verification limits. They do not establish complete native rendering or application-wide redesign acceptance.

The incorporated source inventory is pinned to `c7868b48536e8f8277d6872bd7d8def9d8463d02`.
Calibration viewport documentation includes Cantonese inline. Shell/tab now has a Cantonese
companion; native-workflow navigation still has no separate companion. Connection/send
continuation unit `b60bd4c8` is held outside this snapshot for a disclosure-availability repair.

- [Shared controls](studio-atlas-shared-controls.md): button, card, search and menu anatomy with caller gaps.
- [Fields and presets](studio-atlas-fields-and-presets.md): measured field geometry, validation colors and preset controls.
- [Prepare inspector and lists](prepare-inspector-atlas.md): native inspector rows, sections and selection surfaces.
- [Renderer surfaces](renderer-atlas.md): toolbars, tooltips, timeline and Preview legend painting.
- [Native monitor](monitor-atlas.md): telemetry hierarchy, printer selection, camera footer and DPI corrections.
- [Device popups](device-popups-atlas.md): camera, fan, mapping, material and AMS setting surfaces.
- [Project, preferences and setup](native-preferences-setup-atlas.md): native page composition, card lifecycle and nested search.
- [Workspace](workspace-atlas.md): the five existing subviews and content-triggered reflow.
- [Setup index](setup-index-atlas.md): wizard step painting with preserved routing.
- [Calibration child pages](calibration-children-atlas.md): step, preset, active and result surfaces.
- [Readers and overlays](overlays-atlas.md): eight auxiliary native surfaces and explicit nested boundaries.
- [Live notifications](live-notifications-atlas.md): renderer-owned notification cards and preserved action targets.
- [Embedded palette](embedded-studio-atlas.md): product-owned stylesheet colors, focus and reduced-motion rules.
- [Embedded composition](embedded-composition-atlas.md): nine entrypoints, fourteen imports and source verification limits.
- [Workflow navigation and palette](native-workflow-navigation.md): stable page identities, visual projection and independent palette reversal.
- [Shell and tabs](studio-atlas-shell-and-tabs.md): measured allocation, visible-neighbor drag identities and focus/accessibility repairs.
- [Numeric and selection controls](studio-atlas-selection-controls.md): numeric allocation lifecycle, checkbox glyphs and switch segments.
- [Calibration viewport and presets](calibration-viewport-layout.md): measured instruction/advice text, complete preset rows and intrinsic scroll extents; English and Cantonese in one article.
- [Confirmations and message dialogs](confirmations-atlas.md): scrolling details, persistent authorization controls and cancel-only insufficient-space behavior.
- [Humidity details](humidity-details-atlas.md): measured legend/value rows and final popup placement repair.
- [Transform inspector](transform-inspector-atlas.md): field state paint and hierarchy with unchanged input and item geometry.
- [Appearance property pages](appearance-property-pages-atlas.md): stacked property/reset groups and same-size preset-content reflow.
- [Print preparation](../workspace/print-preparation.md): real Print workspace and existing action continuations.
- [Print setup](../workspace/print-setup-atlas.md): destination/mapping review composition and reopening bounds.
- [Device-name editor](../workspace/device-name-editor-atlas.md): measured validation, action hierarchy and wrapping-before-fit correction.
- [Common floating inspector framing](gizmo-inspector-framing-atlas.md): shared plate/heading paint across twelve existing tool callers.
- [Regex diagnostics and export details](reader-details-atlas.md): diagnostic content reflow and measured export disclosure rows.

## Design source

The canonical in-repo design source is [`ui-md3/design-system/`](../../../ui-md3/design-system/).
Token values there match `src/slic3r/GUI/Widgets/MD3Tokens.hpp` exactly; the header is the native
source of truth that the C++ code resolves against.

## Postman collections

Not applicable. The design system is a compile-time token and typography layer for a desktop
application; it exposes no HTTP or API surface, so no Postman collection is provided for this
category.
