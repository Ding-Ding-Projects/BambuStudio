# App updates from this fork's releases

## Behavior

- **Source of truth**: the GitHub Releases of `Ding-Ding-Projects/BambuStudio` (tags `md3-v<N>`),
  read through `https://api.github.com/repos/Ding-Ding-Projects/BambuStudio/releases/latest`.
  The Bambu Lab cloud feed is no longer consulted for application updates, so the app never
  offers to replace itself with the stock upstream build (it did: the 2.8.2.61 prompt).
- **Newer means published later**: a release counts as an update when its `published_at` is more
  than three hours after `SLIC3R_BUILD_TIME` (stamped at compile time as `%Y%m%d-%H%M%S` on the
  build host). The margin absorbs the build host's clock offset from UTC and the minutes between
  compiling and publishing. Release numbers are not compared, so a local development build newer
  than the latest release is never nagged.
- **What is offered**: the release's `Setup.exe` asset (the release page when no asset is listed).
  The dialog is the MD3 `UpdateVersionDialog` (`ReleaseNote.cpp`): kit header tile, the release
  name and notes as text in the scroll body, and Download / Skip this version / Cancel footer
  pills. Download opens the asset in the default browser.
- **Skip this version** stores the exact tag in `app_config` `app/skip_version`; a manual check
  (Help ▸ Check for updates) ignores the skip.
- **Beta channel**: `check_beta_version()` is a no-op; this fork has no beta channel.

## Configuration

`enable_beta_version_update` no longer changes behavior. No other setting is involved.

## Failure modes

- No network, an API error, or a malformed payload: nothing is shown on the automatic check; a
  manual check shows the "newest version" toast rather than an error, and the reason is logged.
- Unparseable `published_at` or build time: logged, treated as "no update".

## Security considerations

- Anonymous read of a public API; no token is sent. The rate limit (60 requests per hour per IP)
  is far above the app's one call per launch plus manual checks.
- The installer is unsigned by policy; the release notes carry the SHA-256 of `Setup.exe`, and the
  app hands the download to the browser rather than fetching and executing it.

## Verification

- Contract: `ui-md3/tests/md3-conversion-contracts.test.mjs` (`check_new_version` reads this
  fork's releases; the beta check is a no-op).
- Runtime: launch with a build older than the latest release and confirm the dialog names the
  `md3-v<N>` tag and offers `Setup.exe`; launch a build newer than the latest release and confirm
  no dialog.

## Related

- [Windows native installer](../releases/windows-native-installer.md)
- [Release code names](../releases/release-codenames.md)
