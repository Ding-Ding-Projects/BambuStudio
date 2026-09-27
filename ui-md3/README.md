# Bambu Studio — Material Design 3

**Live:** https://ding-ding-projects.github.io/BambuStudio/ · **App:** https://ding-ding-projects.github.io/BambuStudio/app/

A self-contained web implementation of a **Material Design 3 (Material You) concept redesign** of the
Bambu Studio slicer UI. Every screen of the real app's information architecture, re-skinned in pure
MD3 — no invented features. Runs in any browser with **zero dependencies and no build step**.

> Independent, open-source **concept** redesign built on a fork of the open-source Bambu Studio
> project. Not affiliated with, authorized by, or endorsed by Bambu Lab.

## Run it

- **Simplest:** open [`index.html`](index.html) directly in a browser (`file://`).
- **Static server:** serve this folder and open `/index.html`.
- **The site:** [`landing.html`](landing.html) + [`site/`](site/) — the Pages root. It is a
  browser-style **tabbed application**, not a landing page: eight tabs with a pinnable, draggable,
  groupable, searchable strip; English and Hong Kong Cantonese copy at five funny levels per
  language; the shared regex builder behind every search bar; a changelog viewer over every
  published release; non-blocking notifications; and a 10% dim sum surprise. Compose it exactly as
  CI does with `node scripts/compose-site.mjs _site`. Documented in
  [`../docs/features/pages/`](../docs/features/pages/README.md).
- **Native Windows app:** use the
  [latest installer](https://github.com/Ding-Ding-Projects/BambuStudio/releases/latest/download/BambuStudioMD3-Setup.exe).
  It is built from the C++ application by the
  [`Windows build and release`](../.github/workflows/build_all.yml) workflow. The installer is
  unsigned; verify the accompanying
  [SHA-256 file](https://github.com/Ding-Ding-Projects/BambuStudio/releases/latest/download/BambuStudioMD3-Setup.exe.sha256),
  which confirms integrity but not publisher identity.
- **Legacy concept wrapper:** [`desktop/`](desktop/) retains the earlier Electron prototype for
  reference; it is not the published installer.

Query overrides for QA:
`index.html?view=preview&theme=light&density=compact&accent=%237c5cff&lang=bilingual_en_yue_HK`
(`view` ∈ home·prepare·preview·device·multi·project·calibration·filament·settings;
`lang` ∈ `en`·`yue_HK`·`bilingual_en_yue_HK`). The shorter legacy aliases `yue-HK` and
`bilingual` remain accepted for older QA links, but canonical state and persistence use the IDs above.

The interactive Pages/browser prototype provides exactly three language choices under
**Settings → Appearance**: English, playful Hong Kong Cantonese, and compact bilingual. A Settings
choice persists across reloads. The `lang` query parameter takes precedence for that load without
overwriting the saved preference; an unknown value safely falls back to English. Bilingual mode
keeps English primary, adds short Cantonese labels inline, and moves longer translations behind a
small **廣** disclosure badge so narrow layouts remain usable. Errors, privacy controls, and
restore actions use restrained literal Cantonese.

## What's inside

| Screen | Highlights |
|---|---|
| **Home** | Welcome hero, quick actions, recent projects |
| **Prepare** | 3D editor — gizmo rail, scene toolbar, plate bar, printer/filament/process/objects/manipulation sidebar |
| **Preview** | G-code viewer — color schemes, legend, layer & move sliders, statistics |
| **Device** | Live camera, temperatures, fans, axis control, AMS, calibration |
| **Multi-device** | All printers at a glance |
| **Project** | Project files (pictures, BOM, guides, notes) |
| **Calibration** | Flow dynamics, flow rate, max volumetric speed, temp tower, retraction, Z fine-tuning |
| **Filament** | Preset manager with filter / select / bulk-export |
| **Settings** | Appearance, three persisted language modes, general prefs, presets, network, version control, about |

Plus the Export / Send-to-print / Add-filament dialogs, an appearance popover, snackbars, and a
version-history drawer. **Material You** is first-class: light/dark themes, a comfortable/compact
density toggle, an accent **seed color** that regenerates the full HSL tonal ramp live, and the
persisted language selector.

## Architecture

```
ui-md3/
  index.html            app shell + inlined <template>s + boot
  runtime/mini-dc.js    tiny dependency-free runtime for the design's template dialect
  app/
    i18n.resources.js  English-source → Hong Kong Cantonese catalog + message patterns
    i18n.js            mode validation, persistence, URL override, fallback + bilingual metadata
    styles.css          MD3 token system (light/dark, density, accent) — verbatim from the design
    main.logic.js       Main class: shared state + shared/dialog methods + commonVals + renderVals
    boot.js             stitches screen templates into the shell, registers + mounts the app
    searchfield.logic.js  shared regex-capable search component
    screens/
      registry.js       registerScreen(): mixes each screen's methods into Main, collects vals
      <id>.template.html  one <template data-screen> per screen (verbatim MD3 markup)
      <id>.logic.js       that screen's render helpers + vals slice
      CONTRACT.md         the foundation↔screen contract
  assets/showcase/        optimized original artwork shared by Home, landing, and social previews
  scripts/
    assemble-index.mjs  deterministic modular-template → index.html synchronization/check
  tests/
    i18n.test.mjs       browser-free language, persistence, coverage + assembly checks
  design-source/        read-only original Claude Design (the source of truth) + SPEC.md, ARCH-PARALLEL.md
  desktop/              legacy Electron concept shell (not the native release)
  landing.html          the tabbed Pages root (its modules live in site/)
  site/                 the Pages site: copy, core, regex, tabs, views, settings, changelog, CSS
```

**`mini-dc.js`** reimplements the design's template dialect (`{{ }}` interpolation, `<sc-for>`,
`<sc-if>`, `<dc-import>`, `style-hover`, `onClick`/`onInput`) as a keyed virtual-DOM patcher that
preserves input focus/caret and child-component state across re-renders — so the design's markup and
logic port over almost verbatim, maximizing fidelity. See [`SPEC.md`](SPEC.md) and
[`ARCH-PARALLEL.md`](ARCH-PARALLEL.md).

## How it was built

The [design](design-source/) was imported from a Claude Design project, then implemented as real code
by **orchestrating the Codex CLI** (`gpt-5.6-sol`): the runtime and foundation first, then the nine
screens built **in parallel — each by its own Codex agent in a dedicated git worktree** — and finally
assembled and QA'd in the browser.

## Verify language modes

Run the dependency-free Node tests and the template assembly check from the repository root:

```powershell
node --test ui-md3/tests/i18n.test.mjs ui-md3/tests/site.test.mjs ui-md3/tests/site-behaviour.test.mjs ui-md3/tests/dim-sum-runtime.test.mjs ui-md3/tests/layout-clipping.test.mjs
node ui-md3/scripts/assemble-index.mjs --check
```

The GitHub Pages workflow additionally composes the root landing page, verifies every local script
and showcase image with `ui-md3/tests/assert-pages-layout.mjs`, and drives headless Chrome through
447 layout cases with `ui-md3/tests/runtime-layout-clipping.mjs` — 156 on the landing page, 288 that
activate every tab, and 3 compact corner-surface cases. To repeat the
runtime matrix locally, serve `ui-md3/` over HTTP and set `BAMBU_PAGES_TEST_URL` to its
`landing.html` URL before running that test.
