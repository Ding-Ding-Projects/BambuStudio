> [!NOTE]
> A background update shows only non-blocking notices: the ready banner when a new version is
> staged, or one failure notice per release when a newer release could not be staged. Only an
> explicit update check opens the download dialog or the newest-version message. See
> [quiet workflow](quiet-workflow.md).

# App updates from this fork's releases

## Behavior

- **Source of truth**: the GitHub Releases of `Ding-Ding-Projects/BambuStudio` (tags `md3-v<N>`),
  read through `https://api.github.com/repos/Ding-Ding-Projects/BambuStudio/releases/latest`.
  The Bambu Lab cloud feed is no longer consulted for application updates, so the app never
  offers to replace itself with the stock upstream build (it did: the 2.8.2.61 prompt).
- **Newer means published later, in UTC**: a release counts as newer when its `published_at` is
  more than three hours after `SLIC3R_BUILD_TIME_UTC`, the moment of the build stamped at compile
  time as `%Y-%m-%dT%H:%M:%SZ`. Both stamps are UTC and `AppUpdateCheckPolicy`
  (`src/slic3r/GUI/AppUpdateCheckPolicy.hpp`) turns them into seconds with plain calendar
  arithmetic, so the verdict is the same in every local time zone. The margin covers the time
  between compiling a release and publishing it. Release numbers are not compared, so a local
  development build newer than the latest release is never nagged.
- **Two routes**: a copy installed by the Squirrel installer with **Update automatically** on (the
  default) runs Update.exe at every check, see the next section. Update.exe compares package
  versions, so it installs a newer release even when the time comparison disagrees, and installs
  nothing when the feed holds the installed version. Every other copy (a portable zip, a developer
  build, or an installed copy with the preference off) shows the download dialog after a manual
  check when the release is newer; a background check of such a copy shows nothing.
- **What the dialog offers**: the release's `Setup.exe` asset (the release page when no asset is
  listed). The dialog is the MD3 `UpdateVersionDialog` (`ReleaseNote.cpp`): kit header tile, the
  release name and notes as text in the scroll body, and Download / Skip this version / Cancel
  footer pills. Download opens the asset in the default browser.
- **Skip this version** stores the exact tag in `app_config` `app/skip_version`; a manual check
  (Help ▸ Check for updates) ignores the skip. A background check of an installed copy does not
  run Update.exe for a skipped tag.
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
   After a manual or a background check the app then shows a non-blocking banner that stays until
   the user acts: "Bambu Studio `<tag>` is ready. It starts the next time you open the app. Updates
   from this fork are not code-signed.", with two links, **Restart to install update** and
   **Release notes** (the page of that release in the browser; the banner stays). Closing the
   banner is "later": the new version starts the next time the app is opened.
4. **Restart to install update.** The main window is closed through the normal close path, so the
   unsaved-project prompt still applies and Cancel keeps the app open with no restart pending
   (the request is taken back at the start of every close and handed on again only when the close
   is accepted, or replayed by the project page). When the application is really exiting it
   starts `Update.exe --processStartAndWait bambu-studio.exe` without a console window; Update.exe
   waits for the app to exit and then starts the newest installed version.
5. **Manual check.** Help ▸ Check for updates takes the same route. When the release is newer it
   first shows a short "Downloading Bambu Studio `<tag>` in the background." notification, so the
   check does not look like it did nothing while a large package downloads. When the release is
   not newer and Update.exe stages nothing, the check shows the "newest version" message.
6. **While it runs.** An installed copy with the preference on checks again every six hours, so a
   session left open for days still finds a new release. Turning the preference off stops these
   checks until the next launch.
7. **Nothing staged.** When the release was newer but Update.exe staged nothing, the update did not
   finish. A manual check opens the download dialog. A background check shows a non-blocking
   notice, "The background update to Bambu Studio `<tag>` did not finish.", with a **Download from
   the release page** link; it stays until closed and appears once per release. When the release
   was not newer there was nothing to install, and a background check stays silent.

## Copies built before this fix

Builds before this fix compared `published_at` with `SLIC3R_BUILD_TIME`, the build host's local
time with no offset, read in the local time zone of the computer running the app. For a build made
on a host that runs in UTC, as the hosted Windows builds do, the three-hour margin grew west of UTC
(to about seven hours in Toronto while daylight saving time is in effect), so a release published
soon after the installed build was not offered. East of UTC it fell below zero (Hong Kong), so a
copy could be offered its own release. An installed copy also ran Update.exe only after that
comparison said newer. These copies have no compatibility path: reinstall once from the latest
release's `Setup.exe`, after which the corrected check applies.

## Shortcuts and install events

Squirrel makes a shortcut for, and starts, every executable in a package unless one of them is
marked as aware of it. Packages built before this change had no mark, so the regex helper
`bambu-regex-worker.exe` got a shortcut that carried the package title, **Bambu Studio MD3**, and
started a helper that shows nothing, while the application's own shortcut was named **BambuStudio**
in a Start Menu folder "Bambu Research" ([issue #52](https://github.com/Ding-Ding-Projects/BambuStudio/issues/52)).

Now the version resource of `bambu-studio.exe` carries `SquirrelAwareVersion` "1" in the
`040904B0` block, the only one Squirrel reads, and names the product **Bambu Studio MD3** and the
company **codingmachineedge**. Squirrel then runs only the launcher for its events, and the
launcher handles each one and exits before it loads anything of the application:

| Event | What the launcher does |
| --- | --- |
| `--squirrel-install` | `Update.exe --createShortcut=bambu-studio.exe --shortcut-locations=Desktop,StartMenu`, which makes (or replaces) `Bambu Studio MD3.lnk` on the desktop and in the Start Menu folder "codingmachineedge" |
| `--squirrel-updated` | the same, but only for the places that still hold one of this application's shortcuts (new or legacy), so a shortcut the person deleted stays deleted |
| `--squirrel-uninstall` | `--removeShortcut` for both places, then the Start Menu folder "codingmachineedge" when it is empty |
| `--squirrel-obsolete` | nothing |
| `--squirrel-firstrun` | hands the first start to the install root's `bambu-studio.exe` once Update.exe has finished (below); the argument never reaches the application |

After an interactive install, Squirrel starts `app-<version>\bambu-studio.exe --squirrel-firstrun`
itself while the installer is still finishing, and does not wait for it. Started that way, the
application has been reported not to appear, although the shortcuts, which start the install root's
`bambu-studio.exe`, work. So the launcher no longer makes that start itself: when its parent is
`Update.exe` it waits for it to exit (at most 60 seconds), starts the install root's
`bambu-studio.exe` with no argument, shown normally and outside the installer's job object where
Windows allows, and exits. With no install root or stub, or when that start fails, it makes a normal
start as before. Each step is written to `%TEMP%\bbs-launcher-trace.log`, whose lines now begin with
the local time and the process id. A silent install (`--silent`) starts nothing, so no hosted install
check has ever exercised this path: whether the application really appears after an interactive
install is verified only by the [installer first-run diagnostic](../automation/installer-first-run-diagnostic.md)
and by installs on real machines, not by the contract tests.

Once the new shortcuts exist (and on uninstall) it also removes the two shortcuts older packages made,
`BambuStudio.lnk` on the desktop and in "Bambu Research", but only when they point into this
installation. The first update from an older package replaces the helper's wrongly aimed
`Bambu Studio MD3.lnk` in place, because the new shortcut has the same name and folder.

A copy installed by the setup program used before the Squirrel packages (in
`%LOCALAPPDATA%\Programs\Bambu Studio MD3`) is a separate installation. Squirrel never updates it;
remove it with its own uninstaller.

## Configuration

- `auto_update` (Preferences ▸ General ▸ **Update automatically**, default on): whether an
  installed copy updates itself. Turn it off to get the download dialog, after a manual check, on
  every copy. The switch has no effect on a portable or developer build, and Reset preferences
  restores the default.
- `enable_beta_version_update` no longer changes behavior.

## Failure modes

- No network, an API error, or a malformed payload: nothing is shown on the automatic check; a
  manual check shows the "newest version" toast rather than an error, and the reason is logged.
- `published_at` or the build time not in the exact `YYYY-MM-DDTHH:MM:SSZ` form, or an impossible
  date: logged, and the release does not count as newer. An installed copy still runs Update.exe.
- Update.exe cannot be started, exits with a non-zero code, exits with 0 but stages nothing newer,
  or does not finish within 30 minutes: every step is logged (lines starting with `auto update:`).
  When the release was newer, the update did not finish: a manual check falls back to the download
  dialog, as on a copy without automatic updates, and a background check shows the non-blocking
  failure notice once per release, so the six-hourly re-check does not repeat it. A broken update
  therefore never hides a new release. When the release was not newer, nothing staged is not a
  failure. The automatic update is tried again at the next check. The wait is abandoned after 30
  minutes but Update.exe itself is never terminated, since killing it half way through staging
  could leave a partial folder.
- The application closes while an update is downloading: the wait stops and Update.exe is left
  to finish on its own, so the next launch starts the new version. The app does nothing about a
  second Update.exe started meanwhile (by the next launch, say); whatever that run reports
  follows the paths above.
- Restart to install update cancelled at the unsaved-project prompt (or any other veto of the close): nothing
  restarts, and a later, unrelated quit does not restart either.
- Update.exe cannot be started for the restart: the app simply exits and the new version starts
  the next time it is opened.
- Squirrel feed ordering: since md3-v106 the package version is `2.8.<patch*1000+N>` (`2.8.2106`),
  so a Squirrel-feed updater can rank releases; md3-v104 and md3-v105 both carried
  `2.8.2-build61` and are not distinguishable by package version. Builds packaged before the
  run-number change took `N` as the highest release number plus one, read when the package was
  built, so builds queued behind one another could share a version: md3-v155 and md3-v156 both
  carry `2.8.4155`, and releases published from builds that were already queued may repeat one too. Squirrel installs
  nothing when the feed's version equals the installed one, so the later of two such releases
  never arrived by itself; the app fell back to its download dialog, as for any update that
  prepares nothing. Hosted builds now take `N` from the run number of the Windows build and
  release workflow, which only grows in push order, while a build becomes the latest release only
  when it is ahead of the current one, so the feed's version always goes up. The first package
  built that way jumps once, to about `2.8.4607`.

## Security considerations

- Anonymous read of a public API; no token is sent. The rate limit (60 requests per hour per IP)
  is far above the app's one call per launch, one every six hours on an installed copy, plus
  manual checks. Each check of an installed copy also lets Update.exe read the small `RELEASES`
  file; it downloads a package only when the feed holds a newer version.
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

- Policy: `tests/app_update_check_policy_test.cpp` is a standalone C++ assertion executable for
  `AppUpdateCheckPolicy`. It covers the strict UTC parser (leap days, year boundaries and malformed
  stamps), the margin boundary (exactly three hours is not newer, one second more is), the
  md3-v225 case, every routing and outcome branch, and the same verdicts under the `UTC`,
  `America/Toronto` and `Asia/Hong_Kong` time zones. Build it without `NDEBUG` (the file undefines
  it) and run it, for example
  `g++ -std=c++17 -Wall -Wextra -Werror tests/app_update_check_policy_test.cpp -o aucp && ./aucp`.
- Contract: `node --test ui-md3/tests/app-auto-update.test.mjs` pins the install detection, the routing in
  `check_new_version`, the UTC comparison without a time-zone API, the fixed feed, the hidden
  process, the exit-code-and-folder success rule, the outcome handling (ready banner after every
  check, download dialog after a failed manual check, one failure notice per release after a failed
  background check, the newest-version message or silence when nothing is newer), the restart
  hand-over, the cancelled-close rule, the banner that never fades with its two links and unsigned
  notice, the six-hourly re-check, the `auto_update` default and the extracted messages.
  `node scripts/check-quiet-prompts.mjs` requires every download dialog in the update route to sit
  behind an explicit request and both background notices to be non-blocking notifications.
- Runtime (pending a release capture): install an older release with `Setup.exe`, launch it and
  confirm the `auto update:` log lines, the "is ready" banner and, after Restart to install update, that
  the newer version starts; then repeat with the preference off and with a portable copy, run a
  manual check and confirm the download dialog appears instead. Launch a build newer than the
  latest release and confirm neither route triggers. The native compile of this change is pending
  the hosted Windows build. On hosted Windows, the
  [self-update diagnostic](../automation/self-update-diagnostic.md) installs an older release, starts
  it with its default settings and records whether it stages the latest one by itself.

## Related

- [Windows native installer](../releases/windows-native-installer.md)
- [Release code names](../releases/release-codenames.md)
