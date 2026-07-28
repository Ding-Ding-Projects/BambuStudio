# GitHub Pages site

The published site at <https://ding-ding-projects.github.io/BambuStudio/> is a self-contained
static application built from [`ui-md3/landing.html`](../../../ui-md3/landing.html) and the modules
in [`ui-md3/site/`](../../../ui-md3/site/). It is not a marketing page with a scroll bar: it is a
browser-style tabbed surface carrying the same obligations as the desktop app — three language
modes, two funny-level sliders, a full regex builder behind every search bar, non-blocking
notifications, a complete changelog viewer, and per-element appearance customization.

Everything is served from this repository. There are no third-party requests, no CDN, no analytics
and no cookie banner; preferences live in the visitor's own browser and nowhere else.

## Features

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
- [Dim sum surprise](dim-sum-surprise.md) — the 1% startup delight, its bundled artwork, and the
  conditions under which it stays quiet.
- [Deployment and the layout gate](deployment-and-layout-gate.md) — how the site is composed,
  published, and held to 444 measured layout cases before a deploy is allowed.

## Postman

Not applicable. This category ships no HTTP API: the site is static files, and the only network
requests it makes are for its own assets on the same origin. The repository's HTTP contracts and
their Postman collections live under [`../api/`](../api/README.md).

## Verification

| Check | Command |
|:---|:---|
| Data and logic contracts | `node --test ui-md3/tests/site.test.mjs` |
| Localisation runtime | `node --test ui-md3/tests/i18n.test.mjs` |
| Static clipping contracts | `node --test ui-md3/tests/layout-clipping.test.mjs` |
| Composed tree | `node ui-md3/scripts/compose-site.mjs _site && node ui-md3/tests/assert-pages-layout.mjs _site` |
| 444 runtime layout cases | `node ui-md3/tests/serve.mjs _site 4173 &` then `BAMBU_PAGES_TEST_URL=http://127.0.0.1:4173/index.html node --test ui-md3/tests/runtime-layout-clipping.mjs` |
