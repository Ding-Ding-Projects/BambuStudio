# Windows build and redesign verification, 6 October 2026

## Current exact-main result

The exact root build retry passed at `2026-10-06T16:43:57.0661669Z` for `cc059003d362b87d53786152ee8c28621d9c2813`. Post-run metadata-only recovery preserved the complete index tree after the preceding source-state failure. The exact root installer started at `2026-10-06T16:45:58.9532106Z` and is running; package verification remains pending.

Reviewed preparation is preserved separately at `1fa14f33a025e58f2a82ef4e21d4edeff027c6ea`, with 38 preparation and 33 native-review checks passing offline, all 55 declared states covered and 147 translated articles current. Two authorized visible attempts exited before inspection. The latest actual target code is `0xFFFFFFFF`, observed before verified teardown. There is no probe or screenshot. A process-specific loader diagnostic is being prepared; its cause remains unproved.

Later visible-desktop and screenshot requests supersede the older launch exclusion. Installer execution, physical printing and release publication remain excluded. The [current continuation](../../CLOSEOUT_PROMPT.md) and [issue handoff](https://github.com/Ding-Ding-Projects/BambuStudio/issues/55#issuecomment-6021087940) distinguish these results. The following older records retain historical source and verification states; they do not override this update.

## Earlier verification records

**Status: exact-main production is in progress. Both root entrypoints and independent package-byte verification passed for the separately preserved redesign candidate. Those results are not main verification, rendered acceptance, installer execution or release publication.**

## Current production receipts

The isolated exact-main invocation began at `2026-10-06T13:55:51Z`, pinned
`cc059003d362b87d53786152ee8c28621d9c2813` at `13:55:54Z`, and selected the
supported read-only dependency cache at `13:55:55Z`. The command is
`build.bat /s -DependencyCacheDirectory <verified-dependency-root>`. The cache
argument names `deps/build/BambuStudio_dep`, not its `usr/local` child. The
isolated application build and staging directories were absent before this run.
The terminal result is pending. This existing-host run is not pristine-VM proof.

The separate candidate `a28944e3c14b2066ee63d14151c8aca23066d743` has these
completed local receipts:

| Operation | UTC start | UTC finish | Result |
| --- | --- | --- | --- |
| `build.bat /s` | `2026-10-06T12:41:43Z` | `2026-10-06T13:04:12Z` | Exit 0; 22m 29s |
| `build-installer.bat /s` | `2026-10-06T13:05:30Z` | `2026-10-06T13:18:59Z` | Exit 0; 13m 29s |

Its independent byte inspection matched 13,639 SBOM files and 1,823,278,738
payload bytes against both staging and package entries. All 310 bundled articles
matched their source; both compiled catalogs matched fresh source-derived
references. The sole extra package payload file is Squirrel's generated
execution stub, whose executable section matches the pinned template. Nuspec,
automation identity, RELEASES hashes/lengths, absent PE certificate data and
`NotSigned` status were verified. The package version is `2.8.4814` and product
version `02.08.04.61`. These are local outputs, not a new release.

| Candidate output | Bytes | SHA-256 |
| --- | ---: | --- |
| `Setup.exe` | 772747776 | `42f1a4fe732a937b4a7016db26d946f14bae85e9d1bc354cc39a99b522579435` |
| `BambuStudioMD3-2.8.4814-full.nupkg` | 778525807 | `629867b0d5854cafe249754d6b4fe644a70bb058f6d94f0ad46577584270432e` |

The combined redesign and reviewed local inspection route remain preserved at
`e9dd8e0a7c0200f212cb777a4bc7d5918f2ccecc`. The route has 26 passing non-window
checks and validates the genuine candidate build receipt without launching it.
Nine visual-review boundaries remain unverified. The entire-application scope
and selective-reversion note below still apply.

### Reused-checkout metadata observation

The current main build generated the exact committed LF bytes of
`src/slic3r/GUI/DeviceWeb/device_page/src/routeTree.gen.ts`, raw blob
`bc5e97ff6fcb9d164c3f1ecd84e9830ef29c68f9`. Content diff is empty, but porcelain
reports a modification because the reused index retains CRLF-era metadata
(5,894 cached bytes versus 5,724 actual bytes). The producer's source check
rejects any porcelain row. No source or index mutation was made during this run.

A disposable repository reproduced the same state. Standard refresh and
`--really-refresh` both returned 1 and retained the row. Staging only the
verified identical file cleared the row while preserving the entire index tree,
raw bytes and an empty staged diff. A subsequent genuine content edit still
failed the actual producer source check. Any recovery of the production checkout
must wait for its terminal result and repeat those exact identity checks.

## Scope and preserved source

The current work makes `build.bat` and `build-installer.bat` acquire missing supported prerequisites automatically, preserves useful caches, verifies unsigned Squirrel.Windows outputs, and delivers reviewed repairs to `main`. The maintainer subsequently resumed a complete native interface redesign, preserving existing functions and features. All design and redesign implementation/review uses explicitly selected `gpt-6-astra` workers. Build-script implementation remains in one isolated `gpt-6.1-sol` lane.

The original preserved baseline is `9a55b7aa1e900f85c2ded1389854beefabff159f`.
It and the reviewed `d048cfc3040a1566b78e03334f3871f1dd0144bb` integration are
ancestors of exact-main candidate `cc059003d362b87d53786152ee8c28621d9c2813`.
Local main, fetched main and `git ls-remote` matched that candidate before this
report. An active build's checked-out source remains immutable.

Application launches, installer execution, physical printer actions, and manual release publication remain excluded. Source-based tests and design references do not override those limits. The broader redesign is incomplete and has no current rendered acceptance.

## Delivered repair evidence

| Source receipt | Change | Focused verification |
| --- | --- | --- |
| `3fa5dd4a48ba21e5b286f29035fb9b0e892abbe3` | Strawberry detection, bounded recovery without blanket removal, producer exit forwarding | 9 bootstrap assertions and 6 entrypoint assertions |
| `d048cfc3040a1566b78e03334f3871f1dd0144bb` | Integration of reviewed bootstrap and mixed AMS Lite reading-state corrections | 8 tray-state cases plus the bootstrap/entrypoint checks |
| `48f9df9b8e797a4373fdece63960ffe189946693`, `97a6790b1a89e7068d4b3ae40cdfaf3c4a952b6d` | Bounded nested MSBuild/compiler pools; OpenSSL preserves the selected budget | 21 script-mode CMake and stub assertions, independently rerun |
| `c660e846fa42e8e0d18f95bac3c7de16a84777ed`, `12a986b201ff59ecd9afea5cb68a3e3eb26cd13d` | Source identity pinned before compilation; tracked and nonignored untracked inputs checked; build-only staging rechecked | 11 mocked source-state assertions, independently rerun |
| `f41adbf1e212e6afcc25eeec8bd06a301ee73d6d`, `6cfe889fb85f7fa0e9a21dd9411f990f9d57aa9d` | Pinned warm Squirrel tool bytes, hidden-file inventory, same-volume staging, complete RELEASES hashes/lengths, recoverable output promotion | 12 cache and 11 output-set fixture assertions, independently rerun under Windows PowerShell |

These focused results are not production-build or installer-execution evidence.
The output fixtures mock the PE reader. Real package verification for the
separate redesign candidate is recorded above; exact-main package verification
remains pending its own root installer result.

The bootstrap installed or selected Visual Studio Build Tools 2026, its CMake,
Strawberry Perl, Python 3.13, .NET 10 and archive tooling through the supported
route. The completed dependency prefix is reused read-only by the current main
run. This records activity on the actual host, not pristine-VM certification.

## Inspected issue inventory

All eleven issues received separate source inspections at the baseline. None is closed by this report. Source presence does not establish runtime completion.

| Issue | Inspection disposition |
| --- | --- |
| [#16](https://github.com/Ding-Ding-Projects/BambuStudio/issues/16) | Existing Home Assistant handover is bounded to its documented paths. Companion licensing/validation and ongoing settings synchronization are separate unresolved boundaries. |
| [#35](https://github.com/Ding-Ding-Projects/BambuStudio/issues/35) | Historical version, Mesa and font repairs exist. Current local package evidence and release metadata remain separate. |
| [#36](https://github.com/Ding-Ding-Projects/BambuStudio/issues/36) | Offline documentation, schedules and seven documented bulk-action surfaces exist; universal coverage is not established. |
| [#41](https://github.com/Ding-Ding-Projects/BambuStudio/issues/41) | Broad upstream printing/project changes are present. Historical startup acceptance and current native build remain unverified. |
| [#43](https://github.com/Ding-Ding-Projects/BambuStudio/issues/43) | Language infrastructure exists, but article translation gaps, draft catalog review and rendered coverage remain. New redesign strings require their own complete extraction/catalog updates. |
| [#45](https://github.com/Ding-Ding-Projects/BambuStudio/issues/45) | Shared component foundations exist; whole-application design parity and current rendered evidence are incomplete. |
| [#46](https://github.com/Ding-Ding-Projects/BambuStudio/issues/46) | Updater implementation exists. Installed-copy behavior has not been exercised in this task. |
| [#51](https://github.com/Ding-Ding-Projects/BambuStudio/issues/51) | Reported Preview source repairs are present; genuine current post-slice visual evidence is missing. |
| [#53](https://github.com/Ding-Ding-Projects/BambuStudio/issues/53) | Existing automation/CLI implementation was located. Current packaged companion still needs production verification. |
| [#55](https://github.com/Ding-Ding-Projects/BambuStudio/issues/55) | Historical feature/preservation tips are already ancestors of the baseline; old integration checklist entries are stale. Delivery and cleanup evidence remain separate. |
| [#56](https://github.com/Ding-Ding-Projects/BambuStudio/issues/56) | Reliability/history work exists. Import cancellation remains cooperative, and full runtime/history-graph acceptance is not established. |

All 16 upstream branch tips were inspected. The upstream master and current release-line tips were already ancestors of the baseline. Older divergent histories are not proof that every historical feature is retained. The reviewed mixed AMS Lite correction was adapted to the current type-aware mapping, preserving single-slot AMS HT indexing instead of copying an obsolete unconditional stride.

## Redesign status and reversibility

The requested scope is the entire application: shell, Prepare, Preview, Print, Monitor, sidebars, settings, menus, dialogs, editors and product-owned embedded panels. The first source units add a Print preparation page and shared navigation styling. They do not complete that scope.

The retained design specification at `669af08fe25582d75a4775389eb53760fea39481` contains 28 surface groups, 324 states and 56 static light/dark structural references. They are design references, not screenshots of implemented behavior. Its 1,204 feature obligations preserve missing and unverified states.

The maintainer may request reversal if the new appearance is not preferred. The pre-redesign baseline is `d048cfc3040a1566b78e03334f3871f1dd0144bb`. Review and reverse only design changes, preserving later build, packaging, AMS and functional repairs. Mixed navigation/function commits must not be reverted wholesale as if they were pure styling. The design lane maintains the detailed change ledger.

## Remaining decisive work

- [x] Review and incorporate application-cache, bounded-worker, patch-replay, transcript, OCCT, generated-LF and PowerShell 5.1 repairs into main.
- [x] Verify both root entrypoints and unsigned package bytes for the separate redesign candidate, with its exact-source limitation.
- [ ] Finish the exact-main `build.bat` invocation and record its actual terminal result.
- [ ] Run exact-main `build-installer.bat` and verify genuine unsigned Squirrel bytes, versions, source identity, RELEASES contents and all output hashes.
- [ ] Integrate reviewed redesign source and complete its extraction/catalog and documentation requirements without claiming rendered acceptance.
- [ ] Continue the whole-application visual implementation inventory; preserve all existing functions.
- [ ] Obtain permitted built interaction and visual evidence before claiming appearance, accessibility or layout acceptance.
- [ ] Update this report with exact final receipts, remote proof and retained-work disposition.

