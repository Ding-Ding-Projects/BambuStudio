# Dim sum startup surprise

On one launch in ten, the desktop app greets a returning user with a small card in the
bottom-right corner of the main window: a photo of one Hong Kong dim sum dish, its name in
English and Traditional Chinese, and one sentence around it. It fades in after the window has
settled, never takes focus, and fades away again on its own.

This is the desktop counterpart of the landing site's
[dim sum surprise](../pages/dim-sum-surprise.md); the two share a catalog and a card design but
not a line of code. The desktop implementation lives in
[`src/slic3r/GUI/DimSumSurprise.cpp`](../../../src/slic3r/GUI/DimSumSurprise.cpp) with its
window-free model in
[`src/slic3r/GUI/DimSumSurpriseModel.hpp`](../../../src/slic3r/GUI/DimSumSurpriseModel.hpp).

> A capture of the card from a built artifact is pending: the card appears on one eligible launch
> in ten and only once a photo has been cached, so the visual smoke harness needs a warm cache and
> a forced draw before it can photograph it. Until that exists, the layout is described below
> rather than shown.

## Behavior

| Aspect | What happens |
| --- | --- |
| Chance | Exactly 10 % per launch, from a fresh `std::random_device`-seeded `std::mt19937` draw. Nothing is counted or stored to make it more or less likely. |
| Frequency | At most once per process. A `LaunchGuard` claims the draw on the first call; any later call is a no-op. |
| Timing | The draw happens in `GUI_App::post_init` after the startup wizard decision. If it hits, a one-shot timer waits 1.5 s so the card never appears before the window is usable, then eligibility is checked again right before showing. |
| Dish | Picked uniformly among catalog dishes whose photo is already on disk. |
| Card | Material Design 3 card on `SurfaceContainerHigh` with an `OutlineVariant` hairline, 16 dp corners, a 96 dp rounded photo, a `Primary`-toned badge ("Dim sum surprise"), the dish name in `Head_14`, and the line in `Body_12` on `OnSurfaceVariant`. Width 380 dp, height from the wrapped text. |
| Focus | The card is a `wxPopupWindow`; it is never activated and never receives keyboard focus. Whatever had focus keeps it. |
| Dismissal | Auto-dismisses after 8 s. Clicking anywhere on the card dismisses it. Escape dismisses it too, via a `wxEVT_CHAR_HOOK` on the main frame, and the key still reaches the focused control. Resting the pointer on the card pauses the timer. |
| Follows the window | Repositions when the main frame moves or resizes; dismisses when the frame is iconized or closed. |
| Motion | Fades in over roughly 130 ms unless Windows reports client-area animations disabled (`SPI_GETCLIENTAREAANIMATION`), in which case it appears at once. |
| Alt text | The card's accessible name and tooltip are the catalog's alt text for the dish in both languages ("Warm tea-house photograph of Classic Har Gow / 港式茶樓木枱上嘅蝦餃"). |

### Language modes and funny level

- **English**: badge "Dim sum surprise", name "Classic Har Gow · 蝦餃", English line.
- **Cantonese**: badge 點心驚喜, name 蝦餃 · Classic Har Gow, Cantonese line.
- **Bilingual**: English badge and name, the English line, then the Cantonese line stacked beneath.

The sentence around the name follows the per-language funny level (`funny_level_en` /
`funny_level_yue` in the app config, 1 to 5, default 3). Levels 1 and 2 read "A one-in-ten
launch. Today it is …"; level 5 has the trolley stop at your table. The dish's own names are the
catalog's `name.en` and `name.zhHant` verbatim at every level and in every mode; only the sentence
changes. English source strings pass through `_L()` so a translation catalog may override them;
the Cantonese lines ship beside them so the bilingual card never depends on a catalog entry.

### When it stays home

The card is skipped, with one `info` log line naming the reason, when any of these hold:

- first run: `firstguide/finish` is not `true`, or the `dim_sum_prior_launch` marker was not
  written by an earlier launch (this launch writes it once onboarding has finished);
- a file or URL was passed on the command line (the user is mid-task);
- the config wizard ran this launch;
- an error dialog was raised during startup (`DimSumSurprise::mark_startup_error()`);
- any modal dialog is open when the timer fires (update prompt, privacy notice, anything);
- Windows reports a quiet state via `SHQueryUserNotificationState`: busy, presentation mode,
  quiet time, or a full-screen application;
- the main frame is hidden or iconized;
- the catalog cache is empty, or no photo is cached yet, or the cached photo does not decode
  (a photo that fails to decode is deleted so the next fetch replaces it).

There is **no placeholder image and no fallback dish**: with nothing to show, nothing is shown.

## Configuration

None. There is no setting to disable the surprise, change its odds, or pick a dish, and none may
be added. The only stored value is the `dim_sum_prior_launch` marker in `BambuStudio.conf`, which
exists so a first launch is never interrupted. No earlier build of this fork ever shipped an
opt-out key, so there is nothing to migrate.

## Catalog and photo source

The one source for names, alt text and photos is the public
[`Ding-Ding-Projects/dim-sum-photos`](https://github.com/Ding-Ding-Projects/dim-sum-photos)
repository:

- **Catalog**: `https://raw.githubusercontent.com/Ding-Ding-Projects/dim-sum-photos/main/catalog/index.json`,
  schema `1.x`, about eight megabytes for ~2 900 dishes. `name.en` and `name.zhHant` are
  authoritative; `image.path` gives the photo's file name; `image.alt.{en,yue}` the alt text.
- **Photos**: release assets only, under
  `https://github.com/Ding-Ding-Projects/dim-sum-photos/releases/download/<volume>/<file>`.
  Published volumes are `catalog-v1` (dishes 1 to 995), `catalog-v1-part-002` (996 to 1985) and
  `catalog-v1-part-003` (1986 to 3070). The dish number picks the first volume to try; the others
  are fallbacks on a 404.

Nothing from the catalog is committed to this repository. The app keeps an application-data cache
under `data_dir()/dim-sum/`:

```
dim-sum/
  catalog.json        compact record: schemaVersion, sourceUrl, revision (ETag of the
                      raw response), fetchedAt (UTC), and per dish id/en/zhHant/altEn/altYue/file
  photos/<file>.png   up to six cached photos, each written atomically via a .part file
```

The cache is warmed on a background thread on every eligible launch, whether or not the draw
hits, so the first surprise a user is owed has a photo ready. The worker makes at most one catalog
GET (only when `catalog.json` is missing) and downloads photos until six are on disk, giving up
after three consecutive failures. It never blocks the UI thread and nothing waits on it.

## Failure modes

| Situation | Result |
| --- | --- |
| Offline, empty cache | Catalog GET fails; one `info` line; nothing shown this launch. |
| Offline, warm cache | Card shows from disk. |
| Catalog response over 24 MB, or a photo over 8 MB | The HTTP helper aborts the transfer; nothing is written. |
| Catalog parses to zero valid dishes | Not cached, `warning` logged, nothing shown. |
| Asset URL 404 on the expected volume | The other volumes are tried in order. |
| Downloaded asset is not a PNG (signature check) | Discarded, `warning` logged. |
| Cached photo fails to decode or is under 64 px | Deleted, `warning` logged, nothing shown this launch. |
| A modal dialog opens during the 1.5 s delay | The late eligibility check skips the card. |
| Main frame closes while the card is up | The card is dismissed with it. |

## Security and privacy

- Exactly two kinds of outbound request, both HTTPS GET, both to GitHub: the raw catalog and a
  release asset. No query strings, no identifiers, no headers beyond what libcurl sends, no
  telemetry. Nothing about the user or the launch leaves the machine.
- Transfers are bounded in size (24 MB / 8 MB) and time (30 s), run off the UI thread, and are
  parsed defensively: a record is dropped unless its id matches `hk-dish-NNNN`, both names are
  present, and the photo file name matches `hk-dish-NNNN-<slug>.png` with a lowercase slug. The
  file name is the only path component the app composes, so a hostile catalog cannot escape the
  photos directory.
- Files are written under the app's own data directory via a temporary `.part` and a rename.
- The card renders a decoded `wxImage`, not a browser view; the catalog's descriptions and prompts
  are never rendered.

## Verification

- `tests/dim_sum/dim_sum_tests_main.cpp` (Catch2, target `dim_sum_tests`): draw fraction over
  200 000 seeded trials within 9–11 %, once-per-process guard, the full eligibility matrix, catalog
  parse with valid, incomplete, path-traversal and malformed records, image file validation, asset
  URL construction across all three volumes and past them, cache record round trip with a tampered
  entry, cached-only picking, and funny-level copy never altering the dish name. Set
  `DIM_SUM_TEST_CATALOG` to a downloaded `index.json` to also parse the live catalog
  (hidden test tag `[.optional]`).
- `src/slic3r/GUI/DimSumSurprise.cpp` and `GUI_App.cpp` compile clean under `cl /Zs` with the
  GUI target's include set.
- Manual: set `funny_level_en`/`funny_level_yue` and the language mode, warm the cache by
  launching once online, then launch repeatedly; roughly one launch in ten shows the card,
  never two cards in one launch, never on a launch with a file argument.

## Suggested articles

- [Release splash art](release-splash-art.md) — the per-release dim sum SVG, a separate seeded
  library that never touches the public catalog.
- [Release codenames](../releases/release-codenames.md) — how releases borrow a dish name.
- [English, Hong Kong Cantonese, and bilingual modes](language-modes.md) — the language modes the
  card honours.
- [Native Material Design 3 UI](md3-native-ui.md) — the tokens and typography the card is drawn with.
- [Dim sum surprise on the landing site](../pages/dim-sum-surprise.md) — the site's version of the
  same delight.
