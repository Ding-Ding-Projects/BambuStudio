# Prepare inspector and list presentation

The Studio Atlas presentation pass changes the native Prepare inspector at four existing
ownership points: `ParamsPanel`, `OptionsGroup`, `ObjectList`, and `PresetComboBox`.
It uses the existing Material Design 3 semantic colors and density settings. It adds no
new setting, translated UI string, search engine, network request, or printing action.

## Layout and states

- The Process header has two rows. The first contains its measured title and the existing
  object-table and compare actions. The second contains Global/Objects and any existing
  scope controls. Each row retains one flexible spacer and uses measured child minima.
  Removing the old 12-em switch cap and 40-em panel floor lets the actual controls and
  existing scroll owner determine the available width.
- Setting sections use a 16-point heading, a tonal surface and divider, and measured
  minimum height. Section spacing and grid gaps follow the current comfortable or compact
  density and are recalculated on the existing DPI-rescale route.
- Preset controls use 14-point comfortable or 13-point compact text, a quiet resting fill,
  a visible focus/hover outline, and the existing disabled tones. Their dropdowns retain
  the original entries, groups, disabled markers, content-width behavior, and search.
  Preset control corner radii keep the shared automatic DPI-rescale route enabled.
- Object rows use the same typography and measured 40/32-DIP density floors. Alternating
  tones distinguish rows; selected rows add a primary-container fill, a leading marker,
  and a focus outline. This new painting is restricted to the object hierarchy. The
  globally installed renderer retains its previous behavior for unrelated data views.

The row-height helper accepts device-pixel measurements, not untranslated string lengths.
A density height is a floor: taller text or controls can increase it. Bitmap dimensions
already measured in device pixels are not scaled a second time. Long preset names retain
the existing display, tooltip, and dropdown behavior; this pass does not rewrite names.

## Behavior retained

The existing controls, event bindings, selected values, client data, object model and
columns remain in place. This pass does not change slicing, preset serialization or
inheritance, grouping, model identity, undo, object manipulation, drag and drop, context
menus, printer submission, or the final confirmation route. The modified-preset indicator
continues through the existing preset-label logic.

Embedded settings still delegate child reveal to their host scroll owner. Standalone
settings retain their bounded viewport. The Process header stays in the same parent-owned
location; this change does not introduce a second scroll owner or a new sticky layer.

## Scope and limitations

This is a source implementation of one part of the broader Studio Atlas design. The
actual field editor, unit, reset marker, supporting/validation row, and orchestration
families in `Field.cpp`, `Tab.cpp`, `OG_CustomCtrl.cpp`, shared `ComboBox`/`TextInput`,
`BitmapComboBox.cpp`, and `Plater.cpp` are separate ownership scopes. No claim of complete
application-wide visual coverage follows from these four changed anchors.

No new persisted preference or external data flow is introduced. Existing preset and
object names may contain user data; future captures must use an isolated, nonprivate
fixture. No hardware interaction is required to inspect these surfaces.

## Verification

The focused `prepare-inspector-layout.test.mjs` compiles the production C++17 geometry
helper and checks 152 assertions for comfortable/compact density, synthetic measured
short/large/multiline text extents, existing control heights, and 100%, 125%, 150%, and
200% scaling. Removing the density floor or imposing a fixed-height cap causes the same
executable assertions to fail. These inputs exercise arithmetic, not native font metrics.

The existing scroll-owner, sidebar-width, section-header measurement, and stacked
plate-setting checks remain passing. Together with the new compiled test they comprise
18 focused tests. Source review checks that event and data paths remain unchanged;
source checks cannot prove native rendering or accessibility behavior.

No full native build, application launch, installer execution, printer command, or
screenshot capture was performed for this change. Native compilation and the required
normal/minimum viewport, English/Cantonese/bilingual, light/dark, four-scale layout and
keyboard matrix remain unverified. In particular, the taller two-row header, focus ring,
last visible settings row, preset dropdowns, and object-tree editing need real built
interaction and capture before visual completion can be claimed.
