# GitHub Pages site

- [Scheduled visitor settings](scheduled-settings.md), local-only typed overrides and explicit mediation limits.

The published site at <https://ding-ding-projects.github.io/BambuStudio/> is a self-contained
static application built from [`ui-md3/landing.html`](../../../ui-md3/landing.html) and the modules
in [`ui-md3/site/`](../../../ui-md3/site/). It is not a marketing page with a scroll bar: it is a
browser-style tabbed surface carrying the same obligations as the desktop app — three language
modes, two funny-level sliders, a full regex builder behind every search bar, non-blocking
notifications, a complete changelog viewer, and per-element appearance customization.

Code, fonts and interface artwork are served from this repository. There are no off-site scripts
or stylesheets, no CDN, no analytics, no trackers and no cookie banner; preferences live in the
visitor's own browser and nowhere else. On an eligible repeat visit, the dim-sum surprise makes one
image request to a published release asset in the public
[`Ding-Ding-Projects/dim-sum-photos`](https://github.com/Ding-Ding-Projects/dim-sum-photos)
catalog with a `no-referrer` policy.

That claim is now enforced rather than merely stated. `assert-pages-layout.mjs` sweeps every
published page for an off-site script or stylesheet, and `offline-render.test.mjs` loads the
composed site in a headless browser with every off-site host unreachable and requires it to render
anyway. Both were added on 2026-07-28, after the design-system UI kit under `/app/` was found
loading React, ReactDOM and `@babel/standalone` from unpkg — which also meant it compiled its own
source in the visitor's browser on every visit, and rendered nothing at all when unpkg was blocked
or down. See [deployment and the layout gate](deployment-and-layout-gate.md) for how both checks
work.

## Features

- [Two-key destructive confirmation](destructive-confirmation.md): action-specific keys, full-range
  authorization slider, emergency cancellation and one-shot execution.

- [Local personal wording](personal-wording.md): private per-visitor JSON import, replacement,
  cache validation and clear behavior, with explicit remaining integration and runtime proof.
- [Browser event narration](event-narration.md): opt-in serialized speech, independent voice
  choices, platform capability and network disclosure, pause and stop.
- [Attention accommodations and message decoration](attention-and-message-decoration.md): five
  independent off-by-default accommodations and a persisted decorative-message emoji switch.

- [Tabbed navigation](tabbed-navigation.md) — the strip, its overflow surface, reordering, pinning,
  grouping, the searchable tab list, and the measured layout algorithm behind them.
- [Language modes and funny levels](language-and-funny-levels.md) — the copy catalog, the two
  independent tone ladders, and the rule that separates voice from facts.
- [Regex builder](regex-builder.md) — the shared component, its engine and dialect, the guided
  construction controls, and the bounded evaluation that keeps a runaway pattern off the page.
- [Changelog viewer](changelog-viewer.md) — every published release, the calendar and typed-date
  filter, the composing search, and the Markdown export.
- [Settings and appearance](settings-and-appearance.md) — theme, density, accent seed, typography,
  per-element editors, the settings search, and the one blocking dialog on the site.
- [Notifications](notifications.md) — the toast stack, the notification centre, and which messages
  are allowed to block.
- [Dim sum surprise](dim-sum-surprise.md) — the 10% startup delight, its public catalog photo, and
  the conditions under which it stays quiet.
- [Deployment and the layout gate](deployment-and-layout-gate.md) — how the site is composed,
  published, and held to 555 measured layout cases before a deploy is allowed.

## The redesign: layer by layer

Rebuilt on 9 November 2026 around the two things the site already had and people liked: the hero
graphic of the printer and the live prototype under `/app/`. The ground is a faint build-plate
grid, the big surfaces are cut like a nozzle (one long corner radius against one short), and the
toolpaths over the hero image draw themselves while the printer chip reports the nozzle holding
near its target. The Overview shows the prototype in a window frame with the three language modes
and a funny-level slider beside it, so the demo is the first thing after the hero.

- **Every feature** (`ui-md3/site/features.js`): one card per feature of the Windows app, sixteen
  in all, each with an inline animated drawing in the accent colour, an info icon, where it lives
  in the app and a link that opens its article in the Docs tab. The first six are on the
  Overview as a teaser.
- **Docs** (`ui-md3/site/docs.js`): every feature article under `docs/features` and every page of
  the GitHub wiki, inside the site. `scripts/build-docs-data.mjs` bundles them at compose time
  (the wiki is shallow-cloned when the network allows and skipped quietly when it does not), and
  the bundle loads the first time anything needs it. Start from a task or an area, search with
  the shared search field and its regex builder over both languages, read the English article or
  its Cantonese twin, have it read aloud with the narrator's voice and rate, copy a link
  (`?article=<id>`) or jump to the source. The markdown is rendered in the browser with an
  escaping converter: nothing in an article can run.
- **Tools** (`ui-md3/site/tools.js`): the command palette (`Ctrl+Shift+F` anywhere on the site,
  also the search button in the header), listing tabs, settings as live switches, features,
  articles and the headings of the open panel, with Enter, Up, Down and Esc, and a teleport to the
  exact element; the Base64 toolkit for text and files (images and PDFs preview inline, files stay
  in the browser, over 8 MB is refused, the last ten results are kept); the accessibility tools
  (text size, high contrast, reduced motion with system, on and off, always-visible focus ring,
  larger targets, read the page aloud), also behind the header's accessibility button; and the
  Animation and transition tab (reveal duration, cross fade, slide or none between tabs, the hero
  toolpaths, cards lifting on hover, and a preview of reduced motion).
- **Motion** (`ui-md3/site/motion.js`): sections rise into view as you scroll, tabs enter with a
  short cross fade or slide, and the feature drawings animate. Reduced motion is a design of its
  own, not a slower one: it follows the system until chosen, every section is simply there, the
  tabs switch at once, the drawings are complete, and the hero toolpaths settle on the one
  primary hue. An automated browser (the layout gate, the capture) sees everything at once.

The three new tabs raise the layout gate to 555 cases (156 landing, 396 per tab, 3 corner
surfaces). `tests/site-redesign.test.mjs` checks the sources, with a broken copy of each that
must fail.

## The prototype at `/app`

The interactive prototype published under `/app/` is documented with the design system, but two of
its contracts are enforced by this category's tests and belong here:

- **Every decorative icon is `aria-hidden`.** An icon-font ligature is read as literal text, and on
  an icon-only button that text becomes the accessible name, shadowing the `title` meant to name it.
- **Every one of its ten search fields is wired**, with plain text as the default and the search
  field's mode travelling with the query. A field that opens a regex builder and filters nothing is
  worse than no field at all.

Both, plus the prototype's dialog semantics and title-bar collapse contract, are asserted in
`ui-md3/tests/layout-clipping.test.mjs` and captured in
[`docs/screenshots/pages/app/`](../../screenshots/pages/README.md).

## Postman

Not applicable. This category ships no HTTP API: the site is static files. Its executable and style
assets are same-origin; an eligible dim-sum surprise may load one public catalog release image.
The repository's HTTP contracts and their Postman collections live under
[`../api/`](../api/README.md).

## Verification

| Check | Command |
|:---|:---|
| Data and logic contracts | `node --test ui-md3/tests/site.test.mjs` |
| Behavioural contracts (storage, appearance, notifications) | `node --test ui-md3/tests/site-behaviour.test.mjs` |
| Localisation runtime | `node --test ui-md3/tests/i18n.test.mjs` |
| Static clipping contracts | `node --test ui-md3/tests/layout-clipping.test.mjs` |
| Composed tree | `node ui-md3/scripts/compose-site.mjs _site && node ui-md3/tests/assert-pages-layout.mjs _site` |
| 447 runtime layout cases (156 landing + 288 per-tab + 3 compact corner-surface cases) | `node ui-md3/tests/serve.mjs _site 4173 &` then `BAMBU_PAGES_TEST_URL=http://127.0.0.1:4173/index.html node --test ui-md3/tests/runtime-layout-clipping.mjs` |
