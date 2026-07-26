# Native Windows installer

## Behavior

The Windows workflow packages the installed native C++ payload as `BambuStudioMD3-Setup.exe` with
NSIS 3.12 (Unicode). The installer requests user-level privileges and uses the fixed target
`%LOCALAPPDATA%\Programs\Bambu Studio MD3`; it does not request administrator elevation or expose an
arbitrary destination picker. It creates current-user Start menu shortcuts and registers its
uninstaller with Windows.

At build time, `GenerateUninstallInclude.ps1` converts the exact prebuilt payload into explicit
per-file `Delete` commands and deepest-directory-first, non-recursive `RMDir` commands. Uninstall of a
prebuilt install therefore removes product-owned files only. Unknown files and their non-empty parent
directories are preserved, and user profiles and projects remain outside the installation directory.

An upgrade is accepted only for a directory bearing this installer's ownership marker. The previous
uninstaller is copied to NSIS's private temporary directory and run synchronously with `_?=` so the
new extraction cannot race old cleanup. Files owned only by the prior version are removed. If an
unknown path remains after old cleanup, the old application has been removed but the new version is
not installed; the user must move that path and retry.

The installer creates recovery metadata and an uninstaller before extracting the application
payload. It marks the short bootstrap interval separately and converts it to the checked `ready`
state only after the recovery uninstaller and incomplete-install registration are written
successfully. A failure while cleaning an incomplete bootstrap preserves its marker and recovery file
so a later setup can cleanly retry. Extraction or final-registration failures likewise preserve
enough owned state for retry or removal. A locked owned file causes uninstall to fail with its
ownership marker and Windows uninstall entry still present, allowing the user to close the
application and retry.

## MD3 visual design

Interactive setup is restyled to the Material Design 3 kit within the limits of classic Win32/NSIS
controls. It renders the MD3 **light** scheme only (see deviation D6). Colours come from the light
tokens whose hex mirrors `src/slic3r/GUI/Widgets/MD3Tokens.hpp` / `ui-md3/design-system` `colors.css`:

| Role | Hex | Used for |
|---|---|---|
| surface | `faf8fd` | page background, control backgrounds |
| on-surface | `1a1b1f` | primary text |
| on-surface-variant | `44464e` | secondary / help text |
| outline-variant | `c5c6d0` | divider hairlines |
| primary | `146c2e` | hero band, selected accent, progress fill, repair caption |
| on-primary | `ffffff` | hero wordmark |
| surface-container-low | `f4f2f9` | build log-tail panel background |
| error | `ba1a1a` | reserved for error accents |

Colours are applied with `SetCtlColors` on nsDialogs controls and, for common controls, with the
matching `COLORREF` messages (`PBM_SETBARCOLOR`/`PBM_SETBKCOLOR`, `EM_SETBKGNDCOLOR`).

Typography substitutes **Segoe UI** for Roboto and **Consolas** for Roboto Mono (deviation D4). Fonts
are created once in `.onInit` via `gdi32::CreateFontW` with a DPI-aware height derived from
`GetDeviceCaps(LOGPIXELSY)` and `MulDiv`, then applied per control with `WM_SETFONT`. The mapped
faces are hero 15pt/700, section heading 12pt/700, card/option label 11pt/600 (Segoe UI Semibold),
body 9pt/400, caption 8pt/400, and mono 8pt (Consolas).

Every custom page draws a full-width solid **hero band** (40u tall, primary background, on-primary
"Bambu Studio MD3" wordmark, Segoe UI Semibold 15pt) at the top. A committed bitmap header was
deliberately rejected: BMPs do not DPI-scale, add a binary asset to keep in sync with the tokens, and
risk palette drift. The solid band is DPI-independent and token-driven, and no new image asset is
committed.

## Wizard pages and flow

`MUI_PAGE_WELCOME` and `MUI_PAGE_FINISH` are replaced by custom nsDialogs pages; the License and
prebuilt progress pages remain MUI pages with a themed `SHOW` callback. The interactive page order is:

1. **Welcome** (custom) — hero band, bilingual headline and one-paragraph body on the MD3 surface.
2. **License** (MUI `MUI_PAGE_LICENSE`) — a `SHOW` callback recolours the license RichEdit background
   to the surface tone. RichEdit honours background/text colour only, so this is partial (see D-note
   below).
3. **Language chooser** (custom, restyled) — the same three radios and identical selection semantics
   as before, now themed with the hero band, section heading, MD3 radio colours, an outline-variant
   divider hairline, and Segoe fonts.
4. **Install-source chooser** (custom, new) — see "Install-source selection".
5. **Build progress** (custom, new, from-source only) — the non-closable page documented in
   [Build from source](windows-build-from-source.md). It aborts (is skipped) for the prebuilt path
   and under silent mode.
6. **Install files** (MUI `MUI_PAGE_INSTFILES`, prebuilt path) — a `SHOW` callback recolours the
   progress bar and the log list.
7. **Finish** (custom) — hero band, success message, and a themed "Launch Bambu Studio MD3 now"
   checkbox (checked by default) that runs the installed executable when it exists.

Uninstaller pages are unchanged (`MUI_UNPAGE_*`).

`MUI_ABORTWARNING` is intentionally not defined so its English-only warning cannot fire. MUI still
generates `.onUserAbort`, which is delegated to a custom abort hook that shows a bilingual, restrained
quit confirmation and refuses to quit while a from-source build is running.

## Install-source selection

Interactive setup offers two install sources, defaulting to prebuilt:

- **Install prebuilt (recommended)** — extracts the compiled payload exactly as before.
- **Build from source (advanced)** — clones and compiles the app on the user's machine. Its full
  behavior, toolchain bootstrap, repair loop, security posture, failure modes, and verification are
  documented in [Build from source](windows-build-from-source.md).

The chosen source is recorded in the registry as `InstallSource` (`prebuilt` | `from-source`) under
the product registration, written in both the bootstrap and the final registration blocks alongside
the existing `LanguageMode` value. The uninstaller reads `InstallSource` to decide how owned files are
enumerated (compiled `Delete` list for prebuilt; owned manifest for from-source).

**Silent guard.** `/S` never reaches the install-source page: the page's create function aborts under
silent mode after forcing `InstallMode=prebuilt`, and `.onInit` sets `prebuilt` as the default. There
is no command-line flag that enables from-source under `/S`. Consequently every silent install records
`InstallSource=prebuilt` and exercises only the prebuilt path.

## Honest Win32/NSIS deviations

The restyle approximates MD3 but cannot reproduce it fully with classic controls. The known,
intended deviations are:

- **D1 — no rounded corners.** Win32 static/edit/button controls are rectangular; region clipping is
  fragile and was not attempted.
- **D2 — no elevation or shadow.** `SetCtlColors` is a flat fill with no compositor layer; MD3
  layering is approximated only by surface/container tone shifts.
- **D3 — no ripple or state layer.** Classic controls keep the fixed system hover/focus visuals; there
  is no Material state layer or motion.
- **D4 — font substitution.** Roboto -> Segoe UI, Roboto Mono -> Consolas; Material Symbols are
  omitted (text only, no icon glyphs).
- **D5 — system-themed wizard buttons.** Back/Next/Cancel are the standard MUI push buttons;
  `SetCtlColors` cannot fill them, so they stay system-themed rather than Material buttons.
- **D6 — light scheme only.** There is no runtime dark mode; classic dialogs do not theme reliably in
  dark mode.
- **D7 — OS-drawn progress bar.** The common-control progress bar accepts bar/track colours, but its
  shape and marquee animation remain OS-drawn.
- **D-note — partial License theming.** The License page RichEdit honours background/text colour only;
  its content formatting is not themed further.

## Language selection and hand-off

Interactive setup offers exactly these baseline choices:

- English (`en`)
- 廣東話（香港，預覽版） (`yue_HK`)
- English + 廣東話（香港，預覽版） (`bilingual_en_yue_HK`)

New installer strings introduced by the restyle (Welcome, install-source chooser, build progress,
finish, quit-confirmation, and from-source error/fallback dialogs) are wrapped bilingually inline
following the existing English + `yue_HK` pattern: Cantonese-only text in `yue_HK`, English + a blank
line + Cantonese in bilingual mode, and English otherwise. These installer strings are not part of the
application gettext catalog. Cantonese for destructive, long-running, and error copy stays restrained
and explicit per `bbl/i18n/yue_HK/STYLE.md`.

Silent automation may pass `/LANGMODE=<identifier>`. An invalid value fails before setup creates an
install directory. With no option, setup reuses a valid saved selection and otherwise defaults to
English.

Setup records the selection under both the product registration and
`HKCU\Software\codingmachineedge\BambuStudioMD3Preferences\LanguageMode`. The application reads the
persistent preference on first launch only when its own configuration does not already contain a
language. Product and uninstall registrations are removed by a successful uninstall; the language
preference intentionally survives. Error dialogs follow the selected mode and keep destructive and
recovery wording literal.

## Fail-closed boundaries

- A fresh non-empty target without the expected marker is never adopted.
- Reparse points in the source payload, install root, Programs parent, fixed install path, Start-menu
  root, product shortcut directory, shortcuts, owned directories, or owned files are rejected; setup
  must not write through a destination junction or symbolic link.
- A pre-existing product Start-menu directory containing any path is rejected; an upgrade's staged
  previous uninstaller must remove its owned shortcuts before new files are written.
- The ownership generator refuses rooted/escaping paths, quotes, newlines, and output inside the
  payload it describes; top-level `Uninstall.exe` is reserved for the generated recovery uninstaller
  and is rejected case-insensitively in source payloads.
- A from-source uninstall reads an owned manifest instead of the compiled `Delete` list. Each manifest
  entry passes the same fixed-ancestor reparse asserts and a per-entry safe-relative-path check that
  rejects absolute, drive-qualified, and `..` traversal paths; an unsafe path or a missing manifest
  aborts fail-closed with no files removed. A locked owned file sets the error flag so the ownership
  marker and registration are preserved for a retry, exactly as on the prebuilt path.
- A missing previous uninstaller, failed staged upgrade, unknown remaining path, skipped extraction,
  locked owned file, or invalid language identifier returns a nonzero result.
- No uninstall path uses `RMDir /r` or another recursive product-directory deletion.

## Automated behavior matrix

`scripts/ci/Test-WindowsInstaller.ps1` may execute unsigned setup programs only when explicitly
enabled on a disposable GitHub-hosted Actions runner. Because every automated invocation uses `/S`,
the matrix exercises only the prebuilt path (from-source is unreachable under silent mode). The
workflow constructs a previous-version fixture and checks:

1. Fresh English install, ownership marker, recovery uninstaller, registration, and shortcuts.
2. Synchronous upgrade to `yue_HK` and removal of a file owned only by the old fixture.
3. Preservation of an unknown file, fail-closed upgrade, removal of that test file, and clean retry.
4. Locked-executable uninstall failure with registration preserved, followed by a successful retry.
5. Preservation of project and profile sentinels outside the installation directory.
6. Rejection of an invalid mode, then bilingual install, hand-off, and uninstall.
7. Bootstrap-cleanup failure with recovery metadata preserved, followed by a successful retry.
8. Exact path-and-SHA equality between the installed payload and CycloneDX inventory.
9. Rejection of an install-directory junction without writing through it.
10. Preservation and rejection of an unknown product Start-menu entry.
11. Rejection of product Start-menu, Programs-parent, and Start-menu-parent junctions without writing
    through them.

The test refuses to overwrite any pre-existing install, product registration, language preference,
shortcut directory, or profile sentinel on its runner. A silent install records
`InstallSource=prebuilt`, so the from-source branch stays off in automation. The from-source path is
never executed in CI; it is validated by compilation and static/parse checks only, as described in
[Build from source](windows-build-from-source.md).

## Signing and verification status

The installer remains unsigned. The release checksum detects an altered download but provides no
publisher identity. GitHub provenance/SBOM attestations bind an artifact digest to the repository
workflow; they are also not Authenticode and do not remove Windows unknown-publisher warnings. A
trusted certificate/provider and secure signing configuration remain externally blocked.

The last fully published baseline before this candidate work is release
`md3-windows-v02.08.01.55-r6.1` from commit `1f1ecb960`, GitHub Actions run `29671557311`. Its installer
checksum was `c66137776687585240e757ac33baa61d3693737c5aac73c145bf2d08d783a89d`. The expanded execution
matrix above is configured in candidate code but has not yet produced a successful candidate run.
No installer or unsigned application was executed locally while preparing this work.

Release gating, CycloneDX, attestations, and immutable publication are documented separately in
[Windows CI and release supply chain](windows-release-supply-chain.md). The optional build-from-source
install source is documented in [Build from source](windows-build-from-source.md).
