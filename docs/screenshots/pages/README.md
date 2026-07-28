# GitHub Pages site captures

Every image here is a genuine headless-Chrome capture of the **published** site at
<https://ding-ding-projects.github.io/BambuStudio/>, taken with
[`ui-md3/scripts/capture-site.mjs`](../../../ui-md3/scripts/capture-site.mjs) through the DevTools
protocol — the same browser the layout gate measures. None is a mockup, a design file, or a
hand-edited image.

Reproduce them with:

```bash
node ui-md3/scripts/capture-site.mjs https://ding-ding-projects.github.io/BambuStudio/index.html docs/screenshots/pages
```

| File | Surface | Viewport |
|:---|:---|:---|
| `tab-overview.png` | Overview tab | 1280×980, dark, English |
| `tab-screens.png` | Screens tab, image-led cards | 1280×980 |
| `tab-materialyou.png` | Material You tab, live theme/density/accent | 1280×980 |
| `tab-download.png` | Download tab, latest release and its real assets | 1280×980 |
| `tab-changelog.png` | Changelog tab, every published release | 1280×980 |
| `tab-regex.png` | Regex lab tab | 1280×980 |
| `tab-settings.png` | Settings tab | 1280×980 |
| `tab-build.png` | How it is built tab | 1280×980 |
| `tabstrip-wide.png` | Header and tab strip with every label shown | 1280 wide |
| `tabstrip-overflow-menu.png` | The overflow menu tabs fall back to when labels stop fitting | 420 wide |
| `settings-language-and-funny.png` | Language mode plus both funny sliders with live previews | 1280 wide |
| `settings-search-cross-tab.png` | Settings search reporting a match that lives on another tab | 1280 wide |
| `regex-lab-matches.png` | Guided parts, flags, live matches and capture groups | 1280 wide |
| `changelog-calendar.png` | Date range picker with month/year jump and presets | 1280 wide |
| `bilingual-narrow.png` | Bilingual mode at 420px — the longest labels the site must hold | 420×900 |
| `settings-cantonese-narrow.png` | Cantonese-only settings at 420px | 420×900 |
| `dim-sum-card.png` | The dim sum surprise card | 1280 wide |
| `material-you-light.png` | Material You tab in the light theme | 1280×980 |

## The prototype at `/app`

`app/` holds captures of the interactive prototype, taken with
[`ui-md3/scripts/capture-app.mjs`](../../../ui-md3/scripts/capture-app.mjs), plus
`app/evidence.json` — measurements taken in the same session through Chrome's
`Accessibility.getFullAXTree` and by driving the UI.

| File | Shows |
|:---|:---|
| `app/titlebar-640.png` | The title bar at 640 CSS px with minimize, maximize and close all in frame |
| `app/settings-switches.png` | The six General preference switches |
| `app/version-history-dialog.png` | The version-history drawer as a real modal |
| `app/ink-search-before.png` | The Ink Manager unfiltered |
| `app/ink-search-filtered.png` | The same list filtered to one row by a plain-text search |
| `app/evidence.json` | Accessible names, switch states, inert counts, focus behaviour, measured rects |

Some of those fixes have **no visible surface** — an accessible name is not a pixel. For those,
`evidence.json` is the evidence, and no screenshot is substituted to make the report look fuller.

```bash
node ui-md3/scripts/capture-app.mjs http://127.0.0.1:4173/app/index.html docs/screenshots/pages/app
```

`dim-sum-card.png` is the one capture whose trigger is simulated: the surprise fires on a genuine
1% draw that cannot be waited for in a scripted capture, so the script calls the same renderer with
the same data to photograph the surface. Everything it shows — artwork, bilingual name, copy at the
active funny level — is what a visitor who wins the draw sees.
