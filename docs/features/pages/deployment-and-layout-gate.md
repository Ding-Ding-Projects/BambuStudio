# Deployment and the layout gate

Workflow: [`.github/workflows/ui-md3-pages.yml`](../../../.github/workflows/ui-md3-pages.yml).
It runs on every push touching `ui-md3/**`, the workflow itself, or the native dialogs the static
contracts assert against, and on `workflow_dispatch`.

## Composing the site

`node ui-md3/scripts/compose-site.mjs _site` produces the published tree:

```
_site/index.html      the tabbed landing site   (ui-md3/landing.html)
_site/site/           its modules and stylesheet (ui-md3/site/)
_site/assets/         bundled fonts and showcase artwork
_site/app/            the interactive prototype, served at /app/
_site/app/i18n*.js    the shared localisation runtime the landing loads
```

The composer is a committed script rather than inline shell in the workflow, so the tree CI
publishes can be reproduced byte-for-byte on a development machine and a layout failure can be
debugged locally instead of only in a runner.

`ui-md3/tests/assert-pages-layout.mjs` then checks the composed tree: every local script,
stylesheet and image reference resolves inside the root, the site modules are all present, the
complete showcase and every bundled font ship, no third-party font or script is referenced, and the
artwork the site modules build at runtime resolves too — a renamed image fails the deploy instead of
404-ing live.

The third-party check sweeps **every** published page, not just the landing page. It was scoped to
the root until 2026-07-28, which is how the design-system UI kit at
`/app/design-system/ui_kits/bambu-studio/` spent months loading React, ReactDOM and
`@babel/standalone` from unpkg — on a site whose first documented promise is that it makes no
third-party requests. The page it broke was simply never the page being inspected. Protocol-relative
`//host/path` URLs count as off-site too; they are same-origin only by accident of how the page was
reached.

## Assembling generated pages

Two pages in the tree are generated and are checked for staleness before the site is composed:

| Script | Generates | From |
|:---|:---|:---|
| `assemble-index.mjs --check` | `ui-md3/index.html` | `ui-md3/app/screens/*.template.html` |
| `assemble-ui-kit.mjs --check` | the UI kit's `index.html` | the twelve `.jsx` sources beside it |

The UI kit's assembler also compiles the JSX, using `ui-md3/scripts/jsx-transform.mjs` — a
dependency-free compiler for the subset the kit uses, whose output matches
`@babel/plugin-transform-react-jsx`'s classic runtime and which throws on anything outside that
subset rather than compiling it to something merely plausible. That is what removed the in-browser
Babel transform the kit used to perform on every page load. The assembler refuses to build a page
whose head has drifted back to a CDN, and refuses one where two sources declare the same top-level
`const` — the compiled blocks are classic scripts sharing a global scope, so a collision is a
`SyntaxError` that silently kills every script after it.

## The gate

Before anything is uploaded, the workflow runs:

| Suite | What it holds |
|:---|:---|
| `ui-md3/tests/i18n.test.mjs` | the shared localisation runtime and the landing's use of it |
| `ui-md3/tests/site.test.mjs` | copy ladders, placeholder parity, facts-per-level, key coverage, regex bounds, date parsing, changelog data integrity, dim sum catalogue |
| `ui-md3/tests/layout-clipping.test.mjs` | static contracts: no ellipsis, no horizontal scroller, 44px floors, the strip's overflow stages, tablist semantics |
| `ui-md3/tests/site-behaviour.test.mjs` | storage round-trips, element-appearance apply/reset, notification recording and persistence |
| `ui-md3/tests/jsx-transform.test.mjs` | the JSX compiler: agreement with Babel's output and whitespace rules, and refusal of everything outside its subset |
| `ui-md3/tests/offline-render.test.mjs` | the composed site rendering in a headless browser with **every off-site host blackholed** |
| `ui-md3/tests/runtime-layout-clipping.mjs` | **447 measured site cases (156 landing + 288 per-tab + 3 compact corner-surface cases) plus 6 that load the published prototype**, in a real headless browser |

`offline-render.test.mjs` composes its own copy of the site, serves it, and points Chrome at it with
`--host-resolver-rules=MAP * 0.0.0.0, EXCLUDE 127.0.0.1`, so loopback still connects and everything
else fails as if the machine were unplugged. It then requires that real UI came back: that the mount
point filled, and that a floor of elements and text rendered. A blank page and a page that quietly
depends on a CDN both fail. The static contract catches an off-site URL that is *written down*; this
catches one that is *reached*, and it is the check that would have caught the UI kit years earlier.

Both suites share the Chrome plumbing in `ui-md3/tests/devtools.mjs`.

The runtime harness is the one that matters. It drives Chrome through the DevTools protocol against
a locally served copy of the composed tree:

- **156 landing cases** — 13 physical widths × 4 display scales × 3 language modes — asserting no
  document overflow, no header element outside the viewport, no overlapping header controls, every
  header target at least 44×44, no feature card clipping its own content, and the language mode
  actually applied.
- **288 per-tab cases** — 4 widths × 3 scales × 3 language modes × 8 tabs — activating every tab
  in-page and asserting that the panel rendered content, that no element inside it is positioned
  outside the viewport or has content wider than itself, that every control is at least 44×44
  (a checkbox inside a 44px label counts, an inline link inside prose is text and is not a target),
  and that the tab strip stayed on one row rather than wrapping.

The prototype published at `/app/` is measured too — three widths at two display scales — asserting
that its window controls stay inside the viewport, that its title bar does not overflow itself, and
that no visible button lacks an accessible name. It was previously unmeasured, which is how a title
bar that pushed its own close button 217px past a 640px viewport reached production.

Because the harness fails on `scrollWidth > width`, the site may not use `text-overflow: ellipsis`
or a horizontally scrolling container anywhere: those hide a clip rather than fix it. Long strings
wrap instead, which is why bilingual mode at 200% scale is a first-class layout case rather than an
afterthought.

## Publishing

A green run uploads `_site` and deploys it with `actions/deploy-pages`. Pages is auto-enabled
through `actions/configure-pages` with `enablement: true`, so no manual repository setting is
required on a fresh fork. Concurrency is capped at one in-flight deploy, newest push wins.

## Reproducing the gate locally

```bash
node ui-md3/scripts/compose-site.mjs _site
node ui-md3/tests/assert-pages-layout.mjs _site
node --test ui-md3/tests/i18n.test.mjs ui-md3/tests/site.test.mjs ui-md3/tests/layout-clipping.test.mjs
node ui-md3/tests/serve.mjs _site 4173 &
BAMBU_PAGES_TEST_URL=http://127.0.0.1:4173/index.html node --test ui-md3/tests/runtime-layout-clipping.mjs
```

The runtime suite needs Chrome or Edge; it finds one through `CHROME_PATH` or the usual install
locations, and takes roughly three minutes for all 447 site cases.
