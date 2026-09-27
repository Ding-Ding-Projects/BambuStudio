# In-app changelog viewer ("What's new")

The desktop application ships a changelog viewer covering **every** release this fork has
published, not only the newest. It is reachable from **Help ▸ What's new / Changelog…**, from the
**What's new** button on the About dialog, and — because the command palette indexes every enabled
menu command — by typing "what's new" or "changelog" into <kbd>Ctrl</kbd>+<kbd>F</kbd>.

This article describes the application surface. The Pages site has its own viewer over the same
facts; see [Changelog viewer (site)](../pages/changelog-viewer.md).

Source: [`src/slic3r/GUI/ChangelogDialog.cpp`](../../../src/slic3r/GUI/ChangelogDialog.cpp)
(dialog and calendar popover), [`src/libslic3r/Changelog.cpp`](../../../src/libslic3r/Changelog.cpp)
(wx-free model: parsing, typed dates, filtering, export).

## Behavior

- **Every version, newest first.** Each release card shows the release number, its dim-sum code
  name in English and Traditional Chinese exactly as the release named it, the UTC publication
  date, the tag, a *Release page* link, and one row per commit between that release and the
  previous one. A release with nothing to list says which of the three honest reasons applies
  (oldest release, same commit as its predecessor, no commits recorded) rather than padding.
- **Every entry links its commit.** The row shows the 9-character short SHA in a monospace link
  whose tooltip carries the full 40-character SHA; activating it opens
  `https://github.com/Ding-Ding-Projects/BambuStudio/commit/<sha>` in the default browser. The
  link's accessible name is "Open commit *sha* on GitHub".
- **Category badges** are derived mechanically from the commit subject's leading verb (`fix…` →
  Fixed, `add…` → Added, `remove…` → Removed, `document…`/`handoff…` → Documented, otherwise
  Changed) by the exporter — the same rule the site uses.
- **Search** runs through the shared `SearchField` pill: plain text by default, the `.*` toggle
  and the `tune` button open the full [regex builder](regex-builder.md) with case, whole-word and
  multiline flags. A query matches entry text, short SHAs, and the release's version, tag and code
  name (a header hit keeps all of that release's entries).
- **Date filter.** *From* and *To* fields accept a typed date — ISO `YYYY-MM-DD` always, or the
  locale's short order (`DD/MM/YYYY` or `MM/DD/YYYY`, separated by `/`, `-`, `.` or a space) with a
  four-digit year. An incomplete or impossible entry (`2026-09`, `8/9/26`, `30/02/2026`) is kept in
  the field and reported inline ("The From date is not complete yet. Expected …"); the previous
  bound stays applied until the text becomes a date again. An empty field is an open bound. A *To*
  before *From* is reported as "no version can match" instead of silently swapping.
- **Calendar picker.** The calendar button opens an anchored popover under the date row: previous
  and next month, a month choice, a year spinner (1970–9999), a Monday-first 7×6 grid with today
  outlined and the selected range filled, a *Clear dates* action and *Done*. Two clicks select a
  range (the second click before the first swaps them). Keyboard: arrows move by day and week,
  <kbd>PgUp</kbd>/<kbd>PgDn</kbd> change month, <kbd>Home</kbd> jumps to today, <kbd>Enter</kbd> or
  <kbd>Space</kbd> picks, <kbd>Esc</kbd> closes. Every change writes ISO dates into the fields and
  refilters the list live. The popover flips above the anchor or slides left when it would leave
  the display.
- **Presets:** *Last 30 days* (today and the 29 days before it), *This year* (1 January to today),
  *All versions* (clears both bounds).
- **Composition.** Search and date range compose; the status line states "*N* versions and *M*
  changes shown (*T* versions in total)". The empty state says which filter excluded everything
  and how to widen it.
- **Show all.** The first 30 matching releases are rendered immediately; a *Show all N versions*
  button renders the rest. Copy and Export always cover the whole filtered set, not only the
  rendered cards.
- **Copy** places the filtered view on the clipboard as Markdown. **Export…** writes Markdown
  (`.md`) or plain text (`.txt`) through the standard save dialog. Both formats begin with a header
  stating the repository, the exported range (`2026-01-01 to 2026-09-08`, `from …`, `until …` or
  `all versions`), the active search and flags, and the counts; every entry carries its full SHA
  (Markdown additionally links it). Results are announced through the app's non-blocking
  notification snackbar; a clipboard held by another application or an unwritable file is
  reported the same way as an error notification, never a modal.
- **Language modes.** All copy goes through `_L()` and follows English, Hong Kong Cantonese and
  bilingual modes. Dish names, versions, dates and SHAs are never localized or restyled.
- **Layout.** The dialog is a resizable MD3 shell (minimum 720×500 DIP, opening at 880×640,
  clamped to the display) so it fits the 1000×600 minimum window. Long entries wrap; nothing is
  clipped at 200% scale.

## Data and configuration

The viewer reads `resources/changelog/changelog.json` (installed with the application's
`resources/` tree) — no network access at runtime. The file is **generated and committed**:

```
node scripts/changelog/export-app-changelog.mjs            # refresh from the GitHub Releases API + git log
node scripts/changelog/export-app-changelog.mjs --offline  # rebuild from ui-md3/site/changelog.data.js
node scripts/changelog/export-app-changelog.mjs --check    # exit 1 when the committed JSON is stale
```

The exporter reuses the parsing of [`ui-md3/scripts/build-changelog.mjs`](../../../ui-md3/scripts/build-changelog.mjs)
(release-name parsing, body metadata, verb categorization, `git log` between tags), so the app and
the site can never disagree about a version, a date or a category. Requirements: Node 18+, an
authenticated `gh` (online mode) and a checkout with the release tags fetched. A full refresh takes
about two minutes because every referenced SHA is resolved individually.

**Every SHA is validated.** Each release commit and entry SHA is resolved with
`git rev-parse --verify <sha>^{commit}`; an unknown, ambiguous or non-commit id fails the export.
The parser on the application side additionally refuses any entry whose SHA is not exactly 40 hex
characters, so a dead commit link cannot ship. The exporter also refuses to write a file listing
fewer releases than the committed one (a partial API page or missing tags).

Schema (`schema: 1`):

| Field | Meaning |
| --- | --- |
| `repository`, `commitUrlTemplate` | Owner/name and `https://github.com/<repo>/commit/{sha}` |
| `generated`, `source`, `categoryDerivation` | Provenance, kept stable across identical refreshes |
| `releases[]` | Newest first: `tag`, `version`, `ordinal`, `date` (UTC `YYYY-MM-DD`), `published` (ISO instant), `codeName.en/.yue`, `qualifier`, `url`, `commit`, `prerelease`, `baseline`, `sameCommit`, `build`, `entries[]` |
| `entries[]` | `sha` (40 hex), `short`, `text` (commit subject), `category` |

There is no user-facing configuration; the viewer has no persisted state of its own.

## Failure modes

- **Missing or corrupt data file.** The dialog opens with "The changelog could not be read", the
  parser's exact reason (file path, JSON error, or the offending field and release), the path of
  the file reinstalling restores, and the public releases URL. Copy and Export are disabled with a
  tooltip saying why.
- **Empty release list** is reported as such with the refresh command.
- **No match** names the filter responsible (search, dates, or both).
- **Invalid typed dates** never change the filter; the text stays and the inline line explains the
  expected format.
- **Clipboard busy / file not writable** produce an error notification; nothing else changes.
- **Stale data** is caught by `--check`, which CI can run beside the site's own freshness check.

## Security considerations

- Runtime is offline: the viewer reads one bundled JSON file and opens links only through the
  system default browser with a URL built from the committed template and a validated 40-hex SHA.
- Regex search is evaluated by the shared bounded-regex worker with the same deadlines and size
  limits as every other search bar; a catastrophic pattern times out and matches nothing.
- Export writes only to the path the user chose in the save dialog; nothing is sent anywhere.
- The exporter runs `gh` and `git` read-only and writes only `resources/changelog/changelog.json`.

## Verification

- `tests/changelog/changelog_tests.cpp` (Catch2, target `changelog_tests`): JSON parsing incl.
  defaults and malformed-document rejection, the committed `changelog.json` parsing with every SHA
  40 hex and newest-first ordering, typed-date parsing (ISO in every locale order, DMY vs MDY,
  separators, leap days, rejection of partial input, two-digit years, letters and impossible days),
  civil-date arithmetic, preset ranges and inclusive bounds, filter composition (date only, text
  only, header hit, both, no match), and Markdown/plain-text export headers, range statement and
  full SHAs. Last run: 8 test cases, 1718 assertions, all passing.
- `node scripts/changelog/export-app-changelog.mjs --check` proves the committed data matches the
  published releases.
- Manual: open Help ▸ What's new, type `2026-09` into From (inline error, list unchanged), pick a
  range in the calendar with the keyboard only, toggle `.*` and search `^Fix`, Copy and paste into
  an editor, Export as `.txt` and confirm the header names the range.

## Suggested articles

- [Changelog viewer on the Pages site](../pages/changelog-viewer.md) — the same data, published.
- [Regex builder](regex-builder.md) — the search pill's `.*` toggle and builder popover.
- [Command palette (Ctrl+F)](command-palette.md) — reaches this dialog by name.
- [App updates from this fork's releases](app-updates.md) — how the versions listed here arrive.
- [Release splash art](release-splash-art.md) — where the dim-sum code names come from.
