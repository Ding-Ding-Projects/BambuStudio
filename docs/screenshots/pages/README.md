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

`dim-sum-card.png` is the one capture whose trigger is simulated: the surprise fires on a genuine
1% draw that cannot be waited for in a scripted capture, so the script calls the same renderer with
the same data to photograph the surface. Everything it shows — artwork, bilingual name, copy at the
active funny level — is what a visitor who wins the draw sees.
