# Windows build and redesign verification, 6 October 2026

**Status: in progress. Neither root production entrypoint has a successful result for the reconciled candidate yet. No new installer or application-runtime acceptance is claimed.**

## Scope and preserved source

The current work makes `build.bat` and `build-installer.bat` acquire missing supported prerequisites automatically, preserves useful caches, verifies unsigned Squirrel.Windows outputs, and delivers reviewed repairs to `main`. The maintainer subsequently resumed a complete native interface redesign, preserving existing functions and features. All design and redesign implementation/review uses explicitly selected `gpt-6-astra` workers. Build-script implementation remains in one isolated `gpt-6.1-sol` lane.

The baseline production run read the unchanged source at `9a55b7aa1e900f85c2ded1389854beefabff159f` and terminated with exit 1 at `2026-10-06T06:49:14Z`. A second exact root run at `be5e1205dcdad8f372d2ba63f367dbd7977c29c4` exited 1 at `2026-10-06T07:41:56Z`. OpenCV completed; wxWidgets patch replay then failed. The third run at `dcadb944e8c8a796b7ec37dcb6904f2c62a27c84` completed those dependencies but exited 1 during application configuration at `2026-10-06T08:15:46Z`, without the terminal CMake diagnostic in its transcript.

After repairing concurrent native stdout/stderr capture, the exact root run at `5e3f28274baec20ddae974aef24ac7dad4c54257` exited 1 at `2026-10-06T08:44:32Z`. The retained diagnostic identifies generated `OpenCASCADEConfig.cmake:46`: compiler `/pathmap` strings are not escaped for the generated CMake quoted string, producing `Invalid character escape '\U'`. The next source repair must preserve compiler path mappings and correct their configuration export. No generated installed file is accepted as the sole repair.

Verified remote `main` is `d4d4bb13bb66eddd53eda9849fd18ec3d5ff0d47`. Reviewed combined source before this documentation commit is `de774e2f5a14de2b53b59a7292beb73b03df9fa4`. The current exact root producer started at `2026-10-06T08:58:03Z` against `05dd932d4de51384742375cb277d51d19e97039f`, passed application configuration/generation and DeviceWeb, and began native C++ compilation at `2026-10-06T09:08:30Z`. It remains active. Its source is pinned while integration continues in a separate checkout.

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

These focused results are not production-build or installer-execution evidence. The output fixtures mock the PE reader. Real package provenance, certificate-table absence, embedded versions, complete package contents, source identity and hashes must be checked after the root installer entrypoint succeeds.

The baseline bootstrap installed or selected Visual Studio Build Tools 2026, its CMake, Strawberry Perl, Python 3.13, .NET 10, and archive tooling through the supported bootstrap route. This records activity on the actual host, not a pristine-VM certification. wxWidgets completed installation at `2026-10-06T06:49:13.8497026Z`, but its umbrella custom-build then reported `MSB8066`, code `-1`. The historical transcript does not expose the underlying diagnostic. OpenCV separately had three already-applied patches and one individually applicable patch, a proven retry hazard now repaired. Neither fact is a successful root build. Completed dependency outputs are retained for the next exact entrypoint run.

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

- [x] Record the baseline production result: exit 1, with the diagnostic limits above.
- [x] Independently review and incorporate application-cache discovery repairs into the verified repair line.
- [ ] Reconcile the intended build candidate, then run its exact `build.bat` and `build-installer.bat`.
- [ ] Verify genuine unsigned Squirrel bytes, package versions, source identity, RELEASES contents and all output hashes.
- [ ] Integrate reviewed redesign source and complete its extraction/catalog and documentation requirements without claiming rendered acceptance.
- [ ] Continue the whole-application visual implementation inventory; preserve all existing functions.
- [ ] Obtain permitted built interaction and visual evidence before claiming appearance, accessibility or layout acceptance.
- [ ] Update this report with exact final receipts, remote proof and retained-work disposition.

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
