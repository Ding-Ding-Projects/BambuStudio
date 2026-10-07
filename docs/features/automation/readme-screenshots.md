# README screenshots

`.github/workflows/readme-screenshots.yml` retakes the screenshots that `README.md` shows and
uploads them for review. Every other hosted capture route encrypts its images to a key that only
the maintainer holds. This one uploads plain PNG files, because its purpose is a replacement set
that a person can look at and commit. That is acceptable only behind the privacy check described
below, and the upload is kept for three days.

## Running it

Dispatch only: Actions, **Retake README screenshots for review**, Run workflow, or

```
gh workflow run readme-screenshots.yml -f target=native-app \
  -f release_tag=md3-v225 -f expected_source_commit=<40-character commit of md3-v225>
gh workflow run readme-screenshots.yml -f target=design-references \
  -f release_tag=md3-v225 -f expected_source_commit=<40-character commit of md3-v225>
```

| Input | Default | Meaning |
| --- | --- | --- |
| `target` | `native-app` | `native-app` drives the installed application; `design-references` renders the Pages app |
| `release_tag` | required | The published release to install, `md3-v<number>` |
| `expected_source_commit` | required | The release's exact 40-character lowercase source commit; the install fails on any other |
| `rows` | empty | A regular expression that narrows the native allowlist; empty retakes all of it |

The design-references job does not install anything, so it ignores `release_tag`,
`expected_source_commit` and `rows`; the first two are still required by the dispatch form.

The workflow has read-only access to the repository and uses no stored secret. Every action is
pinned to the commit the other hosted workflows use, and every input reaches a script only through
an environment variable.

## The native job

`native-app` runs on `windows-2025` for at most 60 minutes:

1. It installs the release with `scripts/ci/Verify-HostedSquirrelInstall.ps1`, the same check the
   other hosted workflows use: published digests, the `RELEASES` row, the package contract, the
   installed executable and the shortcuts, with the default silent install. The run's own token is
   given to this step alone, to read the release, and the script clears it before anything it
   downloaded runs, so no token ever reaches the application.
2. It installs the pinned headless computer-use tool and Pillow in a job-local environment, exactly
   as `scripts/ci/Invoke-HostedReleaseVerification.ps1` does.
3. It runs the privacy check's own fixtures (below), so a broken check stops the job before any
   capture.
4. It sets the display to 1920x1080 with `scripts/ci/HostedDisplayMode.cs`, the display-mode
   helper of the display scaling workflow.
5. It copies the package's own software OpenGL pair (`mesa\opengl32.dll`, `mesa\libgallium_wgl.dll`)
   beside the installed executable after checking both against their pinned SHA-256 values. A
   hosted runner has no GPU, so the application would otherwise make that copy itself and relaunch,
   and the capture driver follows only the process it started.
6. It prepares one data directory, English, light and comfortable, which is the tuple of every
   allowlisted row, under the runner's temporary directory and outside the user profile.
7. It deletes the allowlisted files from the checkout, so an old image can never pass for a new one,
   and runs `scripts/md3/recapture.py` with `--kinds page,crop-probe --mesa --evidence-probe` on the
   selected rows. `--evidence-probe` takes one more layout dump right after each finished capture,
   while its window is still up, and names it in the report row.

## The design-references job

The README's three Material design references (`material-prepare-light-en.png`,
`material-preview-dark-yue-hk.png`, `material-device-dark-bilingual.png`) are captures of the Pages
app, each linked to the app URL it shows. Later native recapture runs had overwritten them with
native captures; their recipes in `docs/screenshots/recapture-manifest.json` now say `pages`, so the
native route leaves them alone.

`design-references` runs on `ubuntu-latest` for at most 20 minutes. It composes the Pages site with
`ui-md3/scripts/compose-site.mjs`, serves it on the loopback interface with `ui-md3/tests/serve.mjs`,
and runs `ui-md3/scripts/capture-app.mjs --readme-references`, which takes a full-page shot of each
exact README URL at 1600x1000, the size they were first published at, in headless Chrome. For each
shot it writes the page's rendered text and the values of its fields as the evidence the privacy
check reads.

## The allowlists

Each job lists the paths it may upload, in the workflow file itself. Nothing outside the list is
ever staged, whatever the capture report says.

- Native, 23 images: the `yum-20260811-*`, `shot-*` and `native-material-*` images under
  `docs/readme-assets/`, and the README's images under `docs/screenshots/` (notifications, version
  history, regex builder, appearance, project tabs, the two Preferences tabs, the File menu, the
  wizard's Ink Selection page, Config profiles, and the three current process sidebar images).
- Design references, 3 images: the `material-*` images above.

The two process sidebar "before" images are historical evidence of a fixed defect. A retake would
show the fixed state under the old name, so they are never retaken.

Four native recipes were wrong and were corrected with this workflow:

| Image | Before | Now |
| --- | --- | --- |
| `shot-prepare-sidebar.png`, `after-sidebar-readable.png` | The `Process` label: a 350 by 53 pixel strip with the section title | The window named `Sidebar`, the whole Prepare sidebar the captions describe |
| `after-header-intact.png` | The `Process` label alone | The settings header row (title, Global and Objects switch, compare and table buttons), an unnamed panel reached as the parent of the `Compare presets` button (`"parent": 1`) |
| `native-material-filament-manager-light-en.png` | `nav:Prepare`, then `nav:Ink`, which matched the sidebar's Ink header and captured the Prepare page | `nav:Ink` alone |

`recapture.py` also now finds the workspace tab strip under its current name, `Workspace
navigation`, as well as the old `Navigation rail`, and no longer falls back to the whole window
when the strip is there but has no such tab: that fallback is what sent `nav:Ink` to the sidebar.

## The privacy check

`scripts/md3/check-readme-screenshot-privacy.py` runs before the upload and fails closed. It stages
an image only when all of these hold:

- its report row is `done` and its path is on the job's allowlist;
- its evidence file exists and is complete, ending with the dump's end record;
- no text in the evidence, as written or after JSON decoding, contains a path under a Windows user
  profile other than `Public` (the same pattern as `ui-md3/tests/evidence-privacy.test.mjs`), a
  POSIX home directory, the runner's account name or computer name, `runneradmin`, a GitHub token
  pattern, or an email address;
- the file is a PNG inside the checkout, at least 10 KiB for a whole window or page and 1 KiB for a
  cropped control, at most 32 MiB, and not blank: at least 12 distinct colours and a channel standard
  deviation of at least 10 on a 128-pixel thumbnail, the test the encrypted hosted captures apply.

A withheld image is recorded with a fixed reason word, such as `email` or `blank-image`. The text
that matched is never printed or stored. When no image passes, the job fails and nothing is uploaded.

The upload, `readme-screenshots-<run id>` or `readme-design-references-<run id>`, holds exactly:

- the staged PNG files, at their repository paths;
- `report.json`: for each row, its index, path, status, SHA-256, width and height, or the reason it
  was withheld;
- `SHA256SUMS`, in the format `sha256sum -c` reads.

Layout dumps, page text, the install receipt and every log stay on the runner and are deleted with
it.

## Privacy model

The images are plain PNG files in a workflow artifact of a public repository: for three days, anyone
signed in to GitHub can download them. The privacy check is an automated gate, not a guarantee. The
layout dump reports the text of the application's own windows, but not text drawn on the 3D canvas,
inside a web view or in a native popup menu. A person therefore reviews every image before any of
them is committed, and the workflow itself never commits, pushes or publishes anything else.

## Limits

- Nothing here is proven until the workflow is dispatched. The release must carry the layout probe
  with its end record and the driver commands the recipes use.
- The Home page and the Setup Wizard body are web views. If they render blank in a window capture,
  the pixel check withholds them.
- If the workspace tab strip has moved the Ink tab into its overflow menu at 1200 pixels, the Ink
  row is reported blocked instead of capturing another page.
- The README captions of the earlier installed-app captures describe the old images; update them
  together with the images.

## Verification

- `node --test ui-md3/tests/readme-screenshots-workflow.test.mjs` pins the dispatch-only trigger,
  the read-only permission, the pinned actions, the token on the install step alone, the privacy
  check before the upload, the three-day retention, the PNG-only allowlists against the README and
  the recapture manifest, the staged directory as the only upload path, and the README URLs of the
  design references.
- `python scripts/md3/test-readme-screenshot-privacy.py` builds a clean fixture and one fixture per
  leak class and checks what is staged, what is withheld and why. Both jobs run it before capturing.
