# Readable dates

Human-facing dates use full month names in English, for example `29 September
2026`. Cantonese uses `2026年9月29日`; bilingual mode presents both. This applies
to build information, release history, recent projects, project and profile
snapshots, notifications, workspace agenda and deadlines, printer media, and
the bundled web and documentation surfaces.

UTC timestamps are converted to the viewer's local time once at presentation.
A date-only value remains the same calendar day in every time zone. Machine
storage, network messages, sorting keys, file names, logs, and date editor input
syntax keep their existing formats. The user's 12-hour clock setting remains
effective for recent projects.

The splash reports **Built** using the binary's compiled UTC timestamp. A build
timestamp does not establish when a release was published. Startup neither
waits for the network nor substitutes an unrelated latest release date. A
future **Released** label requires verified publication metadata for that exact
running version.

Native presentation shares `src/slic3r/GUI/HumanDate.hpp`. Bundled web pages
share `resources/web/include/human-date.js`, whose byte-identical documentation
copy lives in `ui-md3/site/human-date.js`. An automated comparison prevents the
copies from drifting. Invalid date values produce no invented replacement.

Focused checks are in `ui-md3/tests/human-date.test.mjs`. They cover all three
language modes, leap days, invalid values, date-only stability, viewer-local
UTC conversion, and a Toronto daylight-saving transition. Native build and
visual verification remain necessary to establish actual rendered layout.
