# Native Windows installer

## Behavior

The Windows workflow packages the installed native C++ payload as `BambuStudioMD3-Setup.exe` with
NSIS 3.12. The installer requests user-level privileges and uses the fixed target
`%LOCALAPPDATA%\Programs\Bambu Studio MD3`; it does not request administrator elevation or expose an
arbitrary destination picker. It creates current-user Start menu shortcuts and registers its
uninstaller with Windows.

At build time, `GenerateUninstallInclude.ps1` converts the exact payload into explicit per-file
`Delete` commands and deepest-directory-first, non-recursive `RMDir` commands. Uninstall therefore
removes product-owned files only. Unknown files and their non-empty parent directories are preserved,
and user profiles and projects remain outside the installation directory.

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

## Language selection and hand-off

Interactive setup offers exactly these baseline choices:

- English (`en`)
- 廣東話（香港，預覽版） (`yue_HK`)
- English + 廣東話（香港，預覽版） (`bilingual_en_yue_HK`)

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
- A missing previous uninstaller, failed staged upgrade, unknown remaining path, skipped extraction,
  locked owned file, or invalid language identifier returns a nonzero result.
- No uninstall path uses `RMDir /r` or another recursive product-directory deletion.

## Automated behavior matrix

`scripts/ci/Test-WindowsInstaller.ps1` may execute unsigned setup programs only when explicitly
enabled on a disposable GitHub-hosted Actions runner. The workflow constructs a previous-version
fixture and checks:

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
shortcut directory, or profile sentinel on its runner.

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
[Windows CI and release supply chain](windows-release-supply-chain.md).
