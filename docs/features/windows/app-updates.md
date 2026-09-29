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
- **Two routes once a newer release is found**: a copy installed by the Squirrel installer with
  **Update automatically** on (the default) updates itself in the background, see the next
  section. Every other copy (a portable zip, a developer build, or an installed copy with the
  preference off) shows the download dialog, exactly as before.
- **What the dialog offers**: the release's `Setup.exe` asset (the release page when no asset is
  listed). The dialog is the MD3 `UpdateVersionDialog` (`ReleaseNote.cpp`): kit header tile, the
  release name and notes as text in the scroll body, and Download / Skip this version / Cancel
  footer pills. Download opens the asset in the default browser.
- **Skip this version** stores the exact tag in `app_config` `app/skip_version`; a manual check
  (Help ▸ Check for updates) ignores the skip. It applies to the dialog only: the automatic
  update has no dialog to skip.
- **Beta channel**: `check_beta_version()` is a no-op; this fork has no beta channel.

## Automatic update of an installed copy

1. **Detection.** An installed copy lives in `%LOCALAPPDATA%\BambuStudioMD3\app-<version>\bambu-studio.exe`
   with Squirrel's `Update.exe` one folder up. The copy counts as installed only when the name of
   the folder holding the running executable starts with `app-` and that `Update.exe` exists.
   Nothing else takes this route.
2. **Update.** On a worker thread (never the UI thread) the app starts
   `Update.exe --update=https://github.com/Ding-Ding-Projects/BambuStudio/releases/latest/download`
   without a console window and waits for it, up to 30 minutes. GitHub redirects that address to
   the latest release. Update.exe reads `RELEASES`, downloads the full package, checks it against
   the SHA-1 recorded in `RELEASES` and stages the new `app-<version>` folder beside the running
   one. Only one update runs at a time.
3. **Ready.** Exit code 0 together with a newer `app-<version>` folder on disk counts as ready
   (Update.exe also exits with 0 when there is nothing to install, so the folder is checked too).
   The app then shows a non-blocking banner that stays until the user acts: "Bambu Studio `<tag>`
   is ready. It starts the next time you open the app. Updates from this fork are
   not code-signed.", with two links, **Restart to install update** and **Release notes** (the page
   of that release in the browser; the banner stays). Closing the banner is "later": the new
   version starts the next time the app is opened.
4. **Restart to install update.** The main window is closed through the normal close path, so the
   unsaved-project prompt still applies and Cancel keeps the app open with no restart pending
   (the request is taken back at the start of every close and handed on again only when the close
   is accepted, or replayed by the project page). When the application is really exiting it
   starts `Update.exe --processStartAndWait bambu-studio.exe` without a console window; Update.exe
   waits for the app to exit and then starts the newest installed version.
5. **Manual check.** Help ▸ Check for updates takes the same route. It first shows a short
   "Downloading Bambu Studio `<tag>` in the background." notification, so the check does not look
   like it did nothing while a large package downloads.
6. **While it runs.** An installed copy with the preference on checks again every six hours, so a
   session left open for days still finds a new release. Turning the preference off stops these
   checks until the next launch.

## Configuration

- `auto_update` (Preferences ▸ General ▸ **Update automatically**, default on): whether an
  installed copy updates itself. Turn it off to get the download dialog on every copy. The switch
  has no effect on a portable or developer build, and Reset preferences restores the default.
- `enable_beta_version_update` no longer changes behavior.

## Failure modes

- No network, an API error, or a malformed payload: nothing is shown on the automatic check; a
  manual check shows the "newest version" toast rather than an error, and the reason is logged.
- Unparseable `published_at` or build time: logged, treated as "no update".
- Update.exe cannot be started, exits with a non-zero code, exits with 0 but stages nothing newer,
  or does not finish within 30 minutes: every step is logged (lines starting with `auto update:`).
  The check then falls back to the download dialog, as on a copy without automatic updates (a
  skipped version stays skipped), so a broken update never hides a new release: always after a
  manual check, and once per release for the automatic checks, so the six-hourly re-check does not
  repeat it. The automatic update is tried again at the next check. The wait is abandoned after 30 minutes but Update.exe
  itself is never terminated, since killing it half way through staging could leave a partial
  folder.
- The application closes while an update is downloading: the wait stops and Update.exe is left
  to finish on its own, so the next launch starts the new version. The app does nothing about a
  second Update.exe started meanwhile (by the next launch, say); whatever that run reports
  follows the paths above.
- Restart to install update cancelled at the unsaved-project prompt (or any other veto of the close): nothing
  restarts, and a later, unrelated quit does not restart either.
- Update.exe cannot be started for the restart: the app simply exits and the new version starts
  the next time it is opened.
- Squirrel feed ordering: since md3-v106 the package version is `2.8.<patch*1000+N>` (`2.8.2106`) with `N` the release
  number, so a Squirrel-feed updater ranks releases correctly; md3-v104 and md3-v105 both carried
  `2.8.2-build61` and are not distinguishable by package version.

## Security considerations

- Anonymous read of a public API; no token is sent. The rate limit (60 requests per hour per IP)
  is far above the app's one call per launch, one every six hours on an installed copy, plus
  manual checks.
- The automatic update uses HTTPS to GitHub and to the release-asset hosts GitHub redirects to.
  The feed address is a constant in the source; nothing from the release JSON, the preferences or
  user input reaches the Update.exe command line, and the restart command line is fixed too. The
  release notes link is the fork's release page for a tag of the form `md3-v<N>`; any other tag
  opens the list of releases.
- The app downloads and executes nothing itself. Squirrel's `Update.exe` does, and it verifies the
  package against the SHA-1 in `RELEASES`. That checks integrity, not authorship: the packages are
  unsigned by policy, so trust rests on HTTPS to GitHub and on control of the release. A portable
  or developer copy never runs `Update.exe`.
- On the dialog route the installer is unsigned by policy too; the release notes carry the
  SHA-256 of `Setup.exe`, and the app hands the download to the browser rather than fetching and
  executing it.

## Verification

- Contract: `node --test ui-md3/tests/app-auto-update.test.mjs` pins the install detection, the routing in
  `check_new_version`, the fixed feed, the hidden process, the exit-code-and-folder success rule,
  the restart hand-over, the cancelled-close rule, the banner that never fades with its two links
  and unsigned notice, the six-hourly re-check, the `auto_update` default and the extracted
  messages. The comparison of `published_at` with the build time is not covered by a contract.
- Runtime (pending a release capture): install an older release with `Setup.exe`, launch it and
  confirm the `auto update:` log lines, the "is ready" banner and, after Restart to install update, that
  the newer version starts; then repeat with the preference off and with a portable copy and
  confirm the download dialog appears instead. Launch a build newer than the latest release and
  confirm neither route triggers.

## Related

- [Windows native installer](../releases/windows-native-installer.md)
- [Release code names](../releases/release-codenames.md)
