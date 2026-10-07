# Studio Atlas shared control anatomy

This appearance-only unit starts from preserved candidate
`be5e1205dcdad8f372d2ba63f367dbd7977c29c4`. It refines four existing native
primitives without replacing their command, input or data models. The complete
application redesign remains larger than these shared defaults.

## Implemented treatment

| Primitive | Appearance change | Preserved boundary |
| --- | --- | --- |
| Button | Density-aware rounded rectangles replace stadium shapes for the action variants; explicit circles remain circles. Filled and tonal variants use 8% hover and 12% pressed foreground state layers, bounded to preserve readable text contrast. Outlined, text and icon variants use distinct semantic hover/pressed surfaces. Checked and disabled precedence remain explicit. | Existing size tiers, measured label/icon geometry, padding, input handlers, callbacks, accessible identities and caller-selected variants. |
| StaticBox | Default card radius follows the active density at creation. Interactive-card feedback uses the existing shared 100-ms duration. Painted radius is bounded by the actual rectangle. | Explicit caller radius remains authoritative, including TextInput and SpinInput. Border/fill overrides, gradient path, badges, hover ownership and reduced-motion handling remain intact. |
| SearchField | Low-container rounded field with a parent-colored backing outside its shape, instead of a solid square behind the rounded paint. Entry and child actions share the interior role. The focused field uses a two-DIP outline without moving its input or action controls. | The 44-DIP field, 40-DIP action targets, entry/clear/toggle/builder placement, query bounds, matching engine, callbacks and focus behavior. |
| MD3Menu | Hover and keyboard selection paint as inset rounded surfaces. Selection also has a short leading marker. Popup corner radius follows active density. | Full original row hit rectangles, icon/label/shortcut/chevron columns, scroll/filter behavior, menu identities, enable/check rules, dispatch, focus return and motion reduction. |

The search field's two-DIP focus stroke is centered one DIP from the outside
edge. Its existing 40-DIP child targets begin two DIP inside the 44-DIP field,
so the stroke stays outside their rectangles and is not painted over. This is a
documented geometry adaptation of the general inset-ring reference. Button focus
continues to use its existing two-DIP inset ring and contrast fallback.

The accent state-layer helper reduces decorative opacity when the requested
blend would drop text contrast below 4.5:1. For a user-selected pair already
below 4.5:1, it does not make the contrast worse. It does not rewrite the user's
colors or claim to repair an already-low-contrast choice. Some such pairs may
have no safe decorative blend; the original surface is retained. All semantic
state changes and callbacks remain immediate. No additional timer, canvas fade,
delayed activation or continuous decoration is introduced.

## Coverage and explicit caller gaps

This change reaches controls through their existing shared implementation. It
does not establish complete styling of a page simply because that page contains
a Button or SearchField.

| Remaining caller boundary | Source examples | Remaining work |
| --- | --- | --- |
| Explicitly styled buttons and navigation | `Notebook.cpp`, `Widgets/TabCtrl.cpp`, `Widgets/TabStrip.cpp` | These call sites own radii, backgrounds and state colors. Review their registered navigation contracts individually; the shared default must not erase them. |
| Text inputs, selects and numeric fields | `Widgets/TextInput.cpp`, `Widgets/ComboBox.cpp`, `Widgets/SpinInput.cpp` | Persistent label/value/support/validation stacks and caller-owned field anatomy are separate. SearchField still has its existing placeholder-derived accessible name rather than a new persistent-label API. |
| Alternate menu and control renderers | `Widgets/SideMenuPopup.cpp`, `Widgets/CheckBox.cpp`, `Widgets/SwitchButton.cpp`, `GLToolbar.cpp`, `ImGuiWrapper.cpp` | Separate paint/input implementations need their own scoped styling and real interaction evidence. |
| Card composition | `ParamsPanel.cpp`, `StatusPanel.cpp`, `Preferences.cpp` and their nested panels | Header/body/footer structure, explanatory copy, separators, padding and scrolling belong to each caller. A rounded border does not supply these structures. |
| Custom button tiers | Existing Small/Medium/Large callers | The existing 36/42/44-DIP tiers and their type sizes remain unchanged to avoid unreviewed caller layout changes. Moving callers to the 40/32-DIP density contract remains an explicit follow-up. |
| Long menu and bilingual content | `MD3MenuList` measurement and row model | This unit preserves current measured width, maximum width and secondary-label disclosure. It does not claim that existing ellipsis or every longest bilingual label meets the final no-clipping contract. |
| Product-owned embedded views and canvas tools | `resources/web`, `DeviceWeb`, OpenGL/ImGui consumers | Shared native controls do not prove their appearance or interaction. Their separate implementation scopes remain open. |

## Focused verification

Run:

```powershell
node --test tests/native_shared_controls/atlas_control_anatomy.test.mjs
```

Seven source-contract cases cover state precedence, unchanged dimensions, focus
stroke geometry, menu paint bounds, caller-radius authority, reduced-motion
routes, and 15 unchanged input/callback/measurement functions. Their recorded
hashes were compared against the preserved candidate before authoring the
manifest. Deliberately removing pressed-state entries and changing entry width
must be rejected, followed by the original source passing. These are bounded
source checks, not proof that the preserved functions are defect-free.

The exact production color calculations can also be compiled independently:

```powershell
node tests/native_shared_controls/atlas_control_anatomy.test.mjs --extract $scratch
# In the supported MSVC x64 environment, with $scratch set to a temporary directory:
cl /nologo /std:c++17 /EHsc /W4 /I tests /I $scratch tests/native_shared_controls/atlas_button_state_tests.cpp /Fe:$scratch/atlas_button_state_tests.exe /Fo:$scratch/atlas_button_state_tests.obj
& $scratch/atlas_button_state_tests.exe
```

The extraction copies the production luminance, contrast and state-layer
functions verbatim and prints their SHA-256. The C++ tests use a minimal RGB
value adapter, not wxWidgets, and cover default accent pairs plus a deterministic
custom-color grid. They prove the calculation boundary only. No application
window is launched by these checks.

Native compilation, screenshots, actual focus painting, theme transitions,
screen-reader output, long localized labels, custom appearance rendering and
motion timing remain unverified. The full built matrix still needs normal and
minimum viewports, both themes and densities, English/Cantonese/bilingual,
100/125/150/200% scale, and system/reduced motion after launches are authorized.

## Reversal boundary

This unit is separate from the earlier navigation and palette commits. A
reviewed revert of its own commit removes only these shared appearance changes,
their focused tests and this article. It must retain Print routing, stable page
IDs, printer/AMS fixes, build repairs and the independent palette unit. The
central design ledger records the exact integrated commit. No reset or history
rewrite is part of the reversal route.
