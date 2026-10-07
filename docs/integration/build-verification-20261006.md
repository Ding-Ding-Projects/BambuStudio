# Windows build and redesign verification, 6 October 2026

## Merged redesign continuation, 7 October 2026

Main now merges the reviewed redesign and preparation tip `1fa14f33a025e58f2a82ef4e21d4edeff027c6ea` on top of `2f66e245a116f515ba8022db45a74e7d63bb58a8`. The merged tree has no native build, installer, package-byte, launch or screenshot result. The exact-main receipts below belong to `cc059003d362b87d53786152ee8c28621d9c2813`, and the candidate receipts belong to `a28944e3c14b2066ee63d14151c8aca23066d743`; neither is evidence for the merged tree.

Only platform-neutral source checks ran after the merge: 711 of 757 node source-contract cases, 7 of 12 CMake source contracts and the strict Cantonese catalog check (8,102 entries) pass. Each remaining failure is shared with a parent or needs the Windows compiler, configured wx headers or network access. Main's preserved unfinished security wiring (`MainFrame::open_service` and `GUI_App::school_credentials()`) is expected to stop the next native compile until it is repaired.

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

## Later local-inspection tooling

Local inspection route `0a02f1a` required two independently confirmed repairs: `415b213d` establishes process-container membership during creation; `e8b7a654` binds all production records to the latest completed invocation. Combined source `b777219432f1df917fbd7638dd7f2105afc2ff1d` passes 26 non-window tests and validates the actual immutable a28944e3c receipt. No live route ran. The paired article's containment sentence is aligned in this update. The source documentation check passes 144 paired articles and 1,296 changelog entries; the two new local-review articles were added after the installer candidate and are not claimed in its 310-article bundle. The saved no-launch boundary and earlier visible-desktop request are awaiting structured clarification.

## Successful installer production and byte verification

Both exact root entrypoints passed for `a28944e3c14b2066ee63d14151c8aca23066d743`: `build.bat /s` returned 0 at `2026-10-06T13:04:12Z`; `build-installer.bat /s` returned 0 at `2026-10-06T13:18:59Z`. The installer invocation ran from `2026-10-06T13:05:30Z`, lasting 13 minutes 29 seconds. Independent byte verification passed for all 13,639 SBOM files against both staged and compressed bytes, all 310 bundled articles, both compiled catalogs, source/version metadata, RELEASES hashes/lengths and the unsigned PE boundary. The generated execution stub's executable section matches the pinned Squirrel template. No installer or application execution, visual acceptance or release publication is claimed.

The [machine-readable byte report](package-byte-verification-20261006.json) records exact hashes and exclusions. `Setup.exe` is 772,747,776 bytes, SHA-256 `42f1a4fe732a937b4a7016db26d946f14bae85e9d1bc354cc39a99b522579435`. Full package `BambuStudioMD3-2.8.4814-full.nupkg` is 778,525,807 bytes, SHA-256 `629867b0d5854cafe249754d6b4fe644a70bb058f6d94f0ad46577584270432e`. Its 1,823,278,738 payload bytes match the SBOM; the one additional execution stub is separately accounted for. Package version `2.8.4814` and release sequence 229 are local package metadata, not a published release. A successful run on this existing host is not a fresh-machine bootstrap receipt.

## Successful exact native producer

Exact root `build.bat /s` succeeded with exit 0 at `2026-10-06T13:04:12Z` for `a28944e3c14b2066ee63d14151c8aca23066d743`. The observed run started at `2026-10-06T12:41:43Z`, lasting 22 minutes 29 seconds. Source was clean before and after. Native compilation/linking, pinned renderer staging and the automation companion completed. Exact `build-installer.bat /s` is now running against the same unchanged source; installer production and byte verification are still pending. No application launch, installed-copy behavior or rendered acceptance is claimed.

| Built item | SHA-256 |
| --- | --- |
| `bambu-studio.exe` | `4f71c93f61c071fd338884475f72eef49c4ff8dc0dfeceaa3ebc779b101d3750` |
| `BambuStudio.dll` | `9469e21f6c41cd5b36323aa68e93e30949d40f7a569878837fd7f9eb6535c339` |
| `automation/bambu-automation.exe` | `c960fad17e0bf90639d0d021cff7007b6977cd268b4b33a73b72f7e887ccad88` |
| Generated documentation bundle | `e0ca774d4f8d5632da48ce2ebf06007f4d39713237b1765a99ac4a5122f50040` |
| Compiled English catalog | `5aedb969453905d78e0a1768d791b40d21d3c1b31df9be7f8e3d3f4b4eff0037` |
| Compiled Cantonese catalog | `96c12ddc299bc261ba0ef43a6893605a87b93c66e47b6a8bb9ca6912bb28458c` |

The generated documentation bundle matches all 310 source Markdown articles including category indexes; the paired feature-article check separately covers 143 articles and 1,296 changelog entries. Both compiled catalogs match fresh source-derived reference bytes. These are generated/staged-byte checks; package copies remain to be checked. The full private invocation receipt and immutable transcript are retained in ignored local build evidence, not published here.

## Preceding production result

The exact root build at `4147ca9eeb0004f7e18b4a2f5a8c6cb190975d21` ended with exit 1 at `2026-10-06T12:30:40Z`, after 1 hour 26 minutes 18 seconds from source pinning. The earlier three compiler causes did not recur. The remaining diagnostics were the nozzle-card helpers declared on the wrong class and the nozzle status icon calling an unsupported Button member. Reviewed repairs `a913ae69cb250862e55143949043da964c527d04` and `7b70048ed946bf128463789a8f729bf1ac11c4b6` are incorporated in `e78328d15b6bcaf97d73568c26f70f64494735b9`. Independent real-header MSVC checks passed, with deliberate C2039 negative cases. Full native production verification remains pending. No source or index changed during the completed producer.

## Earlier resolved diagnostics

Earlier native diagnostic: the pinned `05dd932d4` run reached GUI compilation
and reported MSVC `C2397` at `StaticBox.hpp:91`. The active density radius was
implicitly converted from integer to double inside a brace initializer.
Reviewed repair `661f4678e430e6832a510fa20cba420d735c68f1` makes that conversion
explicit without changing its value or lifecycle. The independently executed
actual-declaration compiler check and four density assertions pass. That producer
ended with exit 1 at `2026-10-06T10:26:28Z`, without any source/index mutation
during its run. A successful native production retry is pending.

Two further diagnostics from that same pass have reviewed candidate repairs:
`StatusPanel.cpp` C2664 is corrected by accepting the actual `wxStaticText*`
member in the title helper (`ac04eda5`); `Tab.cpp` C2039 is corrected by using
`wxSizerItem::SetMinSize` for the existing spacer (`9bb8eeb7`). Independent
checks compile the actual extracted production code against configured wx
headers. They do not substitute for the full native translation units.

Local build authorization and bounded repair retries remain in effect. The
external-wait limit does not require a separate extension for local compilation.
The catalog-consistency repair is independently reviewed and incorporated at
`4147ca9eeb0004f7e18b4a2f5a8c6cb190975d21`. Eight focused tests and the terminology
check pass. The next exact root producer pinned that source at
`2026-10-06T11:04:22Z` and ended with the nozzle diagnostics above. All 50 documentation findings are
reconciled: 143 articles and 1,296 changelog entries pass. The subsequent
PowerShell 5.1 empty-report reader repair `0625de0d2` preserves strict rejection
of real missing entries. Three production-AST cases pass independently on
PowerShell 5.1.26100.9444, followed by the complete strict language check with
`-RequireComplete`. That check passes for 8,102 native entries, 7,109 audited
drafts, 212 DeviceWeb resources, 282 legacy keys, 334 keyed elements and all
18 interface-language tests. No successful native-build verdict is claimed.
The broader source integration selection now passes 29 checks; nine additional
checks preserve the exact callers of the replaced raw scroll/table owners.
The [tracked publication scan](public-boundary-scan-20261006.md)
records its exact text inventory and contextual review, with no confirmed
violation and explicit exclusions.

## Generated route identity and current source review

DeviceWeb rewrites `src/slic3r/GUI/DeviceWeb/device_page/src/routeTree.gen.ts`
with LF endings. On the current checkout, `core.autocrlf=true` and no exact
attribute caused persistent porcelain dirtiness despite an empty content diff
and equal normalized blob `bc5e97ff6fcb9d164c3f1ecd84e9830ef29c68f9`.
The delivered exact-path `text eol=lf` rule fixes that state. Eight independent
disposable Git assertions reproduce the old mismatch, preserve unchanged-source
identity after generation and reject a genuine added source statement. The
running candidate predates this rule; its final identity verdict remains pending.
No primary index refresh or generated-file restoration was performed during it.

The candidate now also incorporates independently reviewed shell focus/drag,
selection-control allocation, Appearance preset-content reflow, humidity final
placement, calibration result-table ownership/wheel handling and device-name
validation ordering. Their focused checks do not establish native event delivery
or appearance. The source ledger remains pinned to its named review revision;
static design boards are never current runtime captures.

## Scope and preserved source

The current work makes `build.bat` and `build-installer.bat` acquire missing supported prerequisites automatically, preserves useful caches, verifies unsigned Squirrel.Windows outputs, and delivers reviewed repairs to `main`. The maintainer subsequently resumed a complete native interface redesign, preserving existing functions and features. All design and redesign implementation/review uses explicitly selected `gpt-6-astra` workers. Build-script implementation remains in one isolated `gpt-6.1-sol` lane.

The original preserved baseline is `9a55b7aa1e900f85c2ded1389854beefabff159f`.
It and the reviewed `d048cfc3040a1566b78e03334f3871f1dd0144bb` integration are
ancestors of exact-main candidate `cc059003d362b87d53786152ee8c28621d9c2813`.
Local main, fetched main and `git ls-remote` matched that candidate before this
report. An active build's checked-out source remains immutable.

Earlier production history from the redesign line:

The baseline production run read the unchanged source at `9a55b7aa1e900f85c2ded1389854beefabff159f` and terminated with exit 1 at `2026-10-06T06:49:14Z`. A second exact root run at `be5e1205dcdad8f372d2ba63f367dbd7977c29c4` exited 1 at `2026-10-06T07:41:56Z`. OpenCV completed; wxWidgets patch replay then failed. The third run at `dcadb944e8c8a796b7ec37dcb6904f2c62a27c84` completed those dependencies but exited 1 during application configuration at `2026-10-06T08:15:46Z`, without the terminal CMake diagnostic in its transcript.

After repairing concurrent native stdout/stderr capture, the exact root run at `5e3f28274baec20ddae974aef24ac7dad4c54257` exited 1 at `2026-10-06T08:44:32Z`. The retained diagnostic identifies generated `OpenCASCADEConfig.cmake:46`: compiler `/pathmap` strings were not escaped for the generated CMake quoted string, producing `Invalid character escape '\U'`. The delivered source-template repair preserves compiler path mappings and corrects their configuration export. Subsequent root configuration passed. No generated installed file was accepted as the sole repair.

Verified remote `main` was then `cc059003d362b87d53786152ee8c28621d9c2813`. The exact root producer recorded at that time read `4147ca9eeb0004f7e18b4a2f5a8c6cb190975d21`, pinned at `2026-10-06T11:04:22Z`. Configuration and DeviceWeb completed, and native C++ compilation began at `2026-10-06T11:09:39Z`. It ended with exit 1 at `2026-10-06T12:30:40Z`. Later reviewed documentation and the two nozzle repairs are incorporated in `e78328d15b6bcaf97d73568c26f70f64494735b9` without altering the completed producer.

Application launches, installer execution, physical printer actions, and manual release publication remain excluded. Source-based tests and design references do not override those limits. The broader redesign is incomplete and has no current rendered acceptance.

## Delivered repair evidence

| Source receipt | Change | Focused verification |
| --- | --- | --- |
| `3fa5dd4a48ba21e5b286f29035fb9b0e892abbe3` | Strawberry detection, bounded recovery without blanket removal, producer exit forwarding | 9 bootstrap assertions and 6 entrypoint assertions |
| `d048cfc3040a1566b78e03334f3871f1dd0144bb` | Integration of reviewed bootstrap and mixed AMS Lite reading-state corrections | 8 tray-state cases plus the bootstrap/entrypoint checks |
| `48f9df9b8e797a4373fdece63960ffe189946693`, `97a6790b1a89e7068d4b3ae40cdfaf3c4a952b6d` | Bounded nested MSBuild/compiler pools; OpenSSL preserves the selected budget | 21 script-mode CMake and stub assertions, independently rerun |
| `c660e846fa42e8e0d18f95bac3c7de16a84777ed`, `12a986b201ff59ecd9afea5cb68a3e3eb26cd13d` | Source identity pinned before compilation; tracked and nonignored untracked inputs checked; build-only staging rechecked | 11 mocked source-state assertions, independently rerun |
| `f41adbf1e212e6afcc25eeec8bd06a301ee73d6d`, `6cfe889fb85f7fa0e9a21dd9411f990f9d57aa9d` | Pinned warm Squirrel tool bytes, hidden-file inventory, same-volume staging, complete RELEASES hashes/lengths, recoverable output promotion | 12 cache and 11 output-set fixture assertions, independently rerun under Windows PowerShell |
| `4ec74967d878afe7a47fa1b121fa465598fd797f`, `a95ab58fbc53fee855418a80af4cea44ba26ed6b` | Content-bound application configuration, preserved discovery cache and application compiler cap | 14 cache assertions and 7 real configure-only assertions, independently rerun with CMake 4.3.1-msvc1 |
| `92778d4fb315c2b4d704f25f184cc84d8fef56df`, `085738374211e3c192e32fcb570b95d543335b5e` | Repeat-safe OpenCV patch sequence and complete native stdout transcription | 9 patch and 4 PowerShell diagnostic assertions, independently rerun |
| `0e16c9652895313dc4017c5b8332554741a20dfa`, delivered to main as `6a2b2a7de24d0be435d6c94d479d28a14a5ce718` | wx patch replay uses the unchanged forward/reverse helper | 10 real nested-Git/template assertions independently rerun, supplied wx source `a9d946902685b9946d8775f07d2a73a9b5bef394` |
| `33e3b31e74e5dc363b96268d9653a6e6cc75a794`, delivered as `c523fe3a7d9fb10d07973d83935154be92811947` | Concurrent native stdout and stderr capture with exact exit and argument propagation | 12 diagnostics, 21 worker-budget and 14 cache assertions independently rerun |
| `9af4777546053532db0580a6250f49945e270d27`, delivered as `8dc8bff8025ac2d21ef0a3d1743a14d3e6423121` | Disable retained MSBuild nodes only in the producer child environment | 5 self-expiring inherited-pipe assertions independently rerun; direct ownership of the production pipes was not inspected |

These focused results are not production-build or installer-execution evidence.
The output fixtures mock the PE reader. Real package verification for the
separate redesign candidate is recorded above; exact-main package verification
remains pending its own root installer result.

The bootstrap installed or selected Visual Studio Build Tools 2026, its CMake,
Strawberry Perl, Python 3.13, .NET 10 and archive tooling through the supported
route. The completed dependency prefix is reused read-only by the current main
run. This records activity on the actual host, not pristine-VM certification.

Earlier bootstrap history: wxWidgets completed installation at `2026-10-06T06:49:13.8497026Z`, but its umbrella custom-build then reported `MSB8066`, code `-1`. The historical transcript does not expose the underlying diagnostic. OpenCV separately had three already-applied patches and one individually applicable patch, a proven retry hazard now repaired. Neither fact is a successful root build. Completed dependency outputs are retained for the next exact entrypoint run.

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

The retained design specification at `3c8fe2708ba03470c1b171e2fce12fcbdfaab0fc` contains 28 surface groups, 324 states, 56 static light/dark structural references and explicit caller-level anatomy contracts. They are design references, not screenshots of implemented behavior. Its 1,204 feature obligations preserve missing and unverified states. The first combined candidate passed 28 Node cases, including 30 compiled Print-state assertions, and six actual-extraction/compiled-translation cases. All 56 deterministic reference files matched. Later combined source `5274e22fa8bbc8a3257d9b12b663117aef6d38ec` passed nine Preferences production-method fixture cases and 18 Prepare cases including 152 compiled geometry assertions. These results do not cover native window integration or rendering.

Independent review repaired structural Preferences search-row identity, three cards' DPI lifecycle, camera-button double scaling, stale Printing Progress minimum height and the renderer tooltip style member. Further nested surfaces remain in progress. Source reviews explicitly distinguish preserved callback text from structural behavior, and focused checks are not accepted as rendered evidence.

Combined source `f3565c7580bebd38bcbf5c5f01e272fa5c4b6224` additionally incorporates Workspace content reflow, setup/calibration presentation with two corrected label receivers, thirteen embedded stylesheets with vertical thumbnail reveal preserved, and Print continuation cards with cached-reopen sizing. Independent focused runs passed 8 Workspace cases, 17 setup/calibration cases, 2 gallery cases, and 2 Print fixtures containing 10 lifecycle and 106 geometry assertions. The earlier embedded accessibility suite retains one independently established baseline inline-handler expectation mismatch; it is not reported green. Shell drag/focus and confirmation minimum-space repairs remain separately reviewed work until incorporated.

The maintainer may request reversal if the new appearance is not preferred. The pre-redesign baseline is `d048cfc3040a1566b78e03334f3871f1dd0144bb`. Review and reverse only design changes, preserving later build, packaging, AMS and functional repairs. Mixed navigation/function commits must not be reverted wholesale as if they were pure styling. The design lane maintains the detailed change ledger.

## Remaining decisive work

The default calibration result fixture was independently rerun after integration
at `7334513f1fdcb8c84bd0f57f9a2cb750c85fc7c8`: one compilation, six geometry cases
and six wheel-routing cases passed. The wheel cases no longer require a separate
optional invocation. This does not establish native event delivery or appearance.

- [x] Review and incorporate application-cache, bounded-worker, patch-replay, transcript, OCCT, generated-LF and PowerShell 5.1 repairs into main.
- [x] Verify both root entrypoints and unsigned package bytes for the separate redesign candidate, with its exact-source limitation.
- [x] Record the baseline production result: exit 1, with the diagnostic limits above.
- [x] Finish the exact-main `build.bat` invocation and record its actual terminal result: the retry passed at `2026-10-06T16:43:57.0661669Z`.
- [ ] Run exact-main `build-installer.bat` and verify genuine unsigned Squirrel bytes, versions, source identity, RELEASES contents and all output hashes.
- [ ] Integrate reviewed redesign source and complete its extraction/catalog and documentation requirements without claiming rendered acceptance.
- [ ] Continue the whole-application visual implementation inventory; preserve all existing functions.
- [ ] Obtain permitted built interaction and visual evidence before claiming appearance, accessibility or layout acceptance.
- [ ] Update this report with exact final receipts, remote proof and retained-work disposition.
