# Every feature page

**Surface:** Help > Every feature, the command palette (`Ctrl+Shift+F`, type
"Every feature"), or the Local tools tab once it is open →
`FeatureFinderPanel` (`src/slic3r/GUI/FeatureFinder/FeatureFinderPanel.{hpp,cpp}`),
backed by the data-only catalogue in
`src/slic3r/GUI/FeatureFinder/FeatureCatalogue.{hpp,cpp}`.

Design: [design/local-tools-2026-11](../../../design/local-tools-2026-11/README.md),
board FeatureFinder.

## What it shows

Every required feature in one place, grouped as Language and voice, Look, Find
and navigate, Records, Tools and Safety. Each row has:

- the feature's name and where it lives in this build, for example
  "File, Local file converter" or "Preferences, Narrator";
- its real shortcut when it has one (`Ctrl+Shift+F`, `Ctrl+Shift+E`, `F1`,
  `Shift+?`);
- a **Fixed** label when this version fixed it;
- an **Open** button, which does exactly what the menu item or setting does.

A feature that is designed but not in this build says **Not built yet** and has
no Open button. A feature that this system cannot open says why instead: for
example **Hidden in School mode** for wording and humour settings while School
mode is on, or **Not available on this system** when its menu command is not
registered here.

## Finding a feature

- Type in the search field. Plain text matches the name, the place, the
  shortcut and extra words (search "json" for the personal wording file,
  "VHDX" for virtual disks, "TOTP" for the authenticator). The `.*` button
  switches to a regular expression with the shared regex builder.
- The chips filter by group, or show only the features that are not built yet.
- Sort by group, A to Z (by the translated name), or Recently used.
- Recently opened features appear above the list. The last eight are kept.
- The command palette lists every feature as "Feature / name" with the same
  place and reason, so `Ctrl+Shift+F` finds them too.

## How Open works

| Kind | What Open does |
| --- | --- |
| A setting | Opens Preferences on the row, scrolls to it, focuses it and flashes it |
| A menu command | Runs the same handler as the menu item (registered under one id) |
| The command palette | Opens the palette |
| An article | Opens the offline documentation article |

Menu items register their handlers with `feature_command(id, handler)` in
`MainFrame.cpp`, so a command shown as available really runs. Settings use the
same row registry as the command palette's teleport, so the funny-level
sliders, the app name and the app logo rows are now teleport targets too.

## Stored values

| AppConfig key | Value |
| --- | --- |
| `feature_finder_sort` | `group`, `a-z` or `recent` |
| `feature_finder_recent` | Up to eight feature ids, most recent first |

Unknown or not-built ids in `feature_finder_recent` are dropped when read.

## Keeping it honest

`tests/feature_catalogue.test.mjs` compiles `tests/feature_catalogue_test.cpp`
and also checks the sources:

- every `command:` id in the catalogue is registered in `MainFrame.cpp`;
- every `prefs:` key is a registered Preferences row;
- every `docs:` article exists and is in the offline bundle;
- every icon exists in the shipped Material Symbols font;
- a not-built feature has no action and a ready one has a valid action.

A negative build that breaks the recently-opened order must fail, so the suite
is proven able to notice. When a designed feature ships, flip its entry to
`State::Ready` with its real action in the same commit.
