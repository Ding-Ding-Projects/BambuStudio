# Keyboard, assistive, and responsive GUI accessibility

## Behavior

The native Windows application and its bundled web surfaces share the same interaction baseline:
controls that perform an action are reachable without a pointer, expose a programmatic name and
state, and show a visible focus indicator. The accessibility wave covers three layers:

- **Native wxWidgets controls:** the Slice/Print split actions, their option menus, segmented
  selectors, text links, Ink Dispenser settings, tabs, media actions, and printer controls accept
  keyboard focus and preserve their existing wx event contracts. Popup menus support directional
  navigation, Home/End, Escape, and focus restoration. Custom controls expose `wxAccessible`
  roles, names, states, and default actions instead of presenting as anonymous painted windows.
- **Filament Manager DeviceWeb page:** tabs, filters, dialogs, sortable table headers, expandable
  groups, selection controls, pagination, and notifications use semantic browser controls. Dialogs
  contain focus, close with Escape, and return focus to the opener. The toolbar and translated
  labels reflow at narrow widths and browser zoom rather than clipping.
- **Project and setup-guide resources:** owned actions are buttons or links, browser zoom remains
  available, dynamically generated legacy actions receive keyboard-compatible semantics, and
  focus remains visible. Project layouts use bounded fluid widths instead of a fixed desktop-only
  canvas.

Informational and success web notifications dismiss after a timeout. Warnings and errors remain
until dismissed. Native acknowledgement-only messages use the existing corner notification
funnels; prompts that require a decision remain modal.

## Configuration

There is no separate accessibility switch. Keyboard operation, accessible names, responsive
layout, and warning/error persistence are unconditional.

The existing application language and appearance settings still control translated copy, light or
dark colors, density, and fonts. DeviceWeb uses English as the resource fallback. English, Hong
Kong Cantonese (`yue_HK`), and bilingual presentation use structured primary/secondary labels on
migrated Filament Manager surfaces so the two languages do not depend on newline-concatenated
fixed-height text.

Motion follows the operating system preference where the surface animates. With reduced motion,
custom native switch/camera transitions and owned web transitions snap to their stable state.

## Keyboard summary

- `Tab` and `Shift+Tab` move through interactive controls in logical order.
- `Space` or `Enter` activates buttons, links, split-button segments, and other button-like custom
  controls. Segmented selectors commit once on the matching key release, so keyboard auto-repeat does
  not oscillate the selection or emit repeated commands.
- Arrow keys move within option menus, tabs, segmented choices, and value controls where the
  platform convention expects them.
- `Home` and `End` select the first or last enabled popup option.
- `Escape` closes a popup or dialog and restores focus to the invoking control.

## Failure modes and limitations

- A disabled or hidden control remains unavailable to keyboard and accessibility activation.
- If a localized label must be visually shortened, its full accessible name and tooltip remain
  available; layout uses measured minimum sizes before resorting to elision.
- WebView2 content does not appear in the native `PrintWindow` capture path on this build host.
  Bundled DeviceWeb, Project, and guide pages are therefore verified with their browser build and
  resource tests; native frames and controls are verified through the repository's off-screen
  Windows harness.
- The repository-wide DeviceWeb lint target contains unrelated pre-existing failures outside the
  changed Filament Manager files. The changed DeviceWeb files are linted separately until that
  existing debt is resolved.

## Security considerations

Accessibility metadata contains only the same user-visible labels and state already present on
screen. No pattern, project, credential, printer access code, or notification payload is sent to a
new service.

Dialogs retain focus only while open and remove their document listeners on close. Owned web
notifications render message text as text, not executable markup. Removing zoom suppression does
not grant page content any additional native bridge capability.

## Verification

The source and resource contracts are designed to fail on the pre-fix behavior. The maintained
local checks are:

```bash
cmake -DBAMBU_SOURCE_DIR=. -P tests/native_shared_controls/native_shared_controls_accessibility_contract.cmake
```

```bash
node --test tests/web_resources_accessibility.test.mjs ui-md3/tests/md3-conversion-contracts.test.mjs
```

```bash
npm --prefix src/slic3r/GUI/DeviceWeb/device_page run test:i18n
```

```bash
npm --prefix src/slic3r/GUI/DeviceWeb/device_page run test:a11y
```

```bash
npm --prefix src/slic3r/GUI/DeviceWeb/device_page run build
```

On 2026-07-30, the delivery candidate passed all three focused native accessibility contracts, all
10 combined owned-web/MD3 contracts, both DeviceWeb focused tests, changed-file ESLint, TypeScript
compilation, and the Vite production build. MSVC compiled the repaired GUI library and linked the full
Release application. Exact Release DLL and real-app headless evidence are recorded in `HANDOFF.md`
after the last rebuild and capture; source-only success is not represented as runtime proof.
