# Release date on the splash screen

The splash screen that shows while the app starts says when the running
version was released, on its own line under the app name and version:

- **English:** `Released 29 September 2026`
- **Hong Kong Cantonese:** `2026年9月29日發佈`
- **Bilingual:** both lines, the English one first and the Cantonese one under it
- **Other languages:** the translated wording with the system's short date

A build that did not come from the release workflow says `Built` instead of
`Released` (Cantonese `構建`), because no release published it.

## Where the date comes from

- CMake records the moment the build is configured, in UTC
  (`SLIC3R_BUILD_TIME_UTC` in `libslic3r_version.h`, written by
  `src/libslic3r/CMakeLists.txt`). The older `SLIC3R_BUILD_TIME` stamp is the
  build host's local time with no offset, so it cannot be converted safely.
- The splash converts that moment to the user's time zone and shows the date.
  A user in Hong Kong who installs a release built at 23:55 UTC sees the next
  day, which is the day their own clock showed.
- The release workflow (`.github/workflows/build_bambu.yml`, called from
  `build_all.yml`) configures with `-DBBL_RELEASE_TO_PUBLIC=1`, which selects
  the word "Released". A build configured without it (the default in
  `version.inc` is `0`) says "Built".
- The same workflow run builds the app and publishes its `md3-v<N>` release,
  normally within an hour, so the build date is the release date. A run that
  crosses midnight in the user's time zone can show the day before the date on
  the release page; the release page's timestamp is the exact moment of
  publication.

## Languages

`splash_release_date_text()` in `src/slic3r/GUI/GUI_App.cpp` translates
`Released %s` and `Built %s` through the language mode service:

- the English wording gets an English date: day, English month name, year;
- the Cantonese wording gets the Hong Kong form: year年month月day日;
- when the Cantonese catalogue has no entry for the line, Cantonese mode shows
  the English line with the English date, never an English sentence around a
  Chinese date;
- bilingual mode stacks the two lines, and the splash centres both.

## Layout

The line is drawn in the kit's `Body_13` font in the muted text colour,
centred across the 480 x 480 DIP splash, in the gap between the title and the
dim sum logo (72 DIP at 100% scale). If the header grows taller than that
gap, for example two bilingual lines at a large system font size, the logo
moves down so the date never overlaps it.

## Failure modes

- A build whose version header has an empty or malformed UTC stamp draws no
  date line; the rest of the splash is unchanged.
- The date belongs to the build, not to the moment of publication. A run whose
  release fails to publish leaves its packages only as workflow artifacts, and
  those still show their build date with the word "Released".

## Security considerations

The date is compiled into the binary. Nothing is fetched at startup, no
network request is made, and no user data is read.

## Verification

- `node --test ui-md3/tests/splash-release-date.test.mjs` pins the UTC stamp,
  the splash drawing and the language routing, and checks that the Cantonese
  catalogue translates both lines.
- The release workflow's build of the change is the compile check.
- The splash of the first release that contains the line is captured in all
  three language modes as the visual check. The splash lives for well under a
  second, so a screenshot of its window comes back before it paints (the
  `md3-v148` attempt produced only black frames). With `BAMBU_LAYOUT_PROBE` set,
  the splash saves the exact bitmap it shows as `splash.png` beside the probe
  dumps, and the capture reads that file.
