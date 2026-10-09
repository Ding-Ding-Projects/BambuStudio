# Self-update diagnostic

An installed copy is meant to update itself: at startup it asks Squirrel's `Update.exe` to stage the
latest release beside the running version, and shows a ready banner when that worked
([App updates](../windows/app-updates.md#automatic-update-of-an-installed-copy)). No run had shown
that happen ([issue #46](https://github.com/Ding-Ding-Projects/BambuStudio/issues/46)).
`.github/workflows/diagnose-self-update.yml` installs an older release on a disposable hosted Windows
runner, starts it the way its shortcuts do, and records whether it stages the latest release by itself.

## Running it

Dispatch only: Actions, **Diagnose self-update on hosted Windows**, Run workflow, or

```
gh workflow run diagnose-self-update.yml -f from_tag=md3-v231 -f observe_seconds=900
```

| Input | Default | Meaning |
| --- | --- | --- |
| `from_tag` | `md3-v231` | The published release to install and update from; it should be older than the latest release |
| `observe_seconds` | `900` | How long the running copy is watched for its update, 120 to 1500 |

One job runs on `windows-2025`, limited to 45 minutes, with read-only access to the repository and
only the run's own token, which reads the releases. The PowerShell is in
`scripts/ci/Diagnose-SelfUpdate.ps1`.

## What it does

1. Reads `from_tag`, the latest release and the latest release's `RELEASES` file, before anything
   downloaded runs.
2. Installs `from_tag` silently through `scripts/ci/Verify-HostedSquirrelInstall.ps1`, which verifies
   the release, the installed executable and the shortcuts and clears the token, then checks that the
   install root holds exactly `app-<version>` of that release.
3. Starts the install root's `bambu-studio.exe` with no argument, as both shortcuts do. No preference
   is changed: **Update automatically** (`auto_update`) is on by default and a fresh runner has no
   configuration file. The receipt records the value the application saved.
4. On a fresh profile the first-run **Setup Wizard** opens as a modal dialog, and the startup update
   check runs only after it closes. A person closes it by finishing it; the diagnostic closes it with
   `WM_CLOSE` and records every close.
5. Every five seconds it records the `bambu-studio.exe`, `Update.exe` and `Setup.exe` processes and
   the application's windows, with the installer first-run diagnostic's own helpers, loaded from
   `scripts/ci/Diagnose-InstallerFirstRun.ps1` rather than copied, and reads the install root's
   `app-<version>` folders and the notification history. It stops after `observe_seconds`, once a
   newer version is staged, `Update.exe` has finished and the banner is recorded (or a minute has
   passed), or once neither the application nor `Update.exe` has run for three polls.
6. Asks the application to close through its windows, so it writes out its log, and stops whatever
   still runs.
7. When a newer version was staged, starts the install root's `bambu-studio.exe` again for 90
   seconds and records which `app-<version>` runs.

## Evidence

All of it is text:

- the application's update lines (`check new version` and `auto update:`). A release build encrypts
  its log with a key compiled in for logs written before a region is chosen; the diagnostic reads that
  key from `src/libslic3r/LogSink.cpp` in the checkout and keeps a decoded copy;
- every `Update.exe` process, with its command line, parent process and exit code;
- Squirrel's logs, the install root's `packages` folder and local `RELEASES`, and the `RELEASES` file
  the feed serves;
- the `app-<version>` folders and when each appeared;
- the ready banner and the failure notice as the notification history records them. The banner is
  drawn inside the 3D view, where no window enumeration can see it, and nothing takes a screenshot;
- Application Error, Windows Error Reporting and .NET Runtime entries for the application and for
  `Update.exe`.

## Classification

`receipt.json` holds one of these values, decided in this order, with its basis.

| Value | Meaning |
| --- | --- |
| `app_crashed` | An application process crashed while the update was observed or at the next start, by the installer first-run classifier's rules: a crash exit code, or an Application Error or Windows Error Reporting crash entry for `bambu-studio.exe` |
| `updated_staged` | A newer `app-<version>` folder with its `bambu-studio.exe` was staged and `Update.exe` had finished. The basis gives the `Update.exe` exit code, whether it is the feed's version, the application's log line, the banner record and the version the next start ran |
| `update_failed` | Nothing newer was staged, and `Update.exe` exited with an error or crashed, or exited with 0 although the feed holds a newer version, or the application logged a failed update or release check, or recorded the failure notice |
| `update_offered_not_staged` | An update was offered (`Update.exe --update` ran, or the application logged a newer release) but nothing was staged by the end, without a failure, such as an `Update.exe` still downloading |
| `no_update_seen` | None of the above; the basis says whether the feed held a newer version and whether the Setup Wizard was still open |

The classification is the result: the job fails only when the releases cannot be read, the release
cannot be verified or installed, or the install root's `bambu-studio.exe` cannot be started.

## Reading the evidence

The job uploads `self-update-<run id>-<attempt>`, kept for seven days, with only `.json`, `.jsonl`,
`.txt` and `.log` files: `preflight.json`, `install-receipt.json`, `feed-RELEASES.txt`,
`receipt.json`, and one folder per start (`observe`, and `next-start` when a version was staged) with
`samples.jsonl`, `process-events.json`, `update-log-lines.txt`, `notification_history.json` and `logs/`
(Squirrel, the launcher trace, the decoded application logs and the event-log entries); `observe` also
holds `squirrel-update-lines.txt` and `local-RELEASES.txt`.

## Limits

- A hosted runner has no GPU and nobody at its desktop. Closing the Setup Wizard is not finishing it,
  so no printer is set up. Updates on real machines remain the final check.
- The **Restart to install update** link is not clicked. The next start through the install root
  shows what a person gets the next time they open the application.
