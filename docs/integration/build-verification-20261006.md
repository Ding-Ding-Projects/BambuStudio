# Windows build and redesign verification, 6 October 2026

**Status: in progress. Neither root production entrypoint has a successful result for the reconciled candidate yet. No new installer or application-runtime acceptance is claimed.**

## Scope and preserved source

The current work makes `build.bat` and `build-installer.bat` acquire missing supported prerequisites automatically, preserves useful caches, verifies unsigned Squirrel.Windows outputs, and delivers reviewed repairs to `main`. The maintainer subsequently resumed a complete native interface redesign, preserving existing functions and features. All design and redesign implementation/review uses explicitly selected `gpt-6-astra` workers. Build-script implementation remains in one isolated `gpt-6.1-sol` lane.

The baseline production build reads the unchanged source at `9a55b7aa1e900f85c2ded1389854beefabff159f`. The verified remote `main` receipt before this report is `6cfe889fb85f7fa0e9a21dd9411f990f9d57aa9d`. These are intentionally different: an active build must not read source being changed underneath it.

Application launches, installer execution, physical printer actions, and manual release publication remain excluded. Source-based tests and design references do not override those limits. The broader redesign is incomplete and has no current rendered acceptance.

## Delivered repair evidence

| Source receipt | Change | Focused verification |
| --- | --- | --- |
| `3fa5dd4a48ba21e5b286f29035fb9b0e892abbe3` | Strawberry detection, bounded recovery without blanket removal, producer exit forwarding | 9 bootstrap assertions and 6 entrypoint assertions |
| `d048cfc3040a1566b78e03334f3871f1dd0144bb` | Integration of reviewed bootstrap and mixed AMS Lite reading-state corrections | 8 tray-state cases plus the bootstrap/entrypoint checks |
| `48f9df9b8e797a4373fdece63960ffe189946693`, `97a6790b1a89e7068d4b3ae40cdfaf3c4a952b6d` | Bounded nested MSBuild/compiler pools; OpenSSL preserves the selected budget | 21 script-mode CMake and stub assertions, independently rerun |
| `c660e846fa42e8e0d18f95bac3c7de16a84777ed`, `12a986b201ff59ecd9afea5cb68a3e3eb26cd13d` | Source identity pinned before compilation; tracked and nonignored untracked inputs checked; build-only staging rechecked | 11 mocked source-state assertions, independently rerun |
| `f41adbf1e212e6afcc25eeec8bd06a301ee73d6d`, `6cfe889fb85f7fa0e9a21dd9411f990f9d57aa9d` | Pinned warm Squirrel tool bytes, hidden-file inventory, same-volume staging, complete RELEASES hashes/lengths, recoverable output promotion | 12 cache and 11 output-set fixture assertions, independently rerun under Windows PowerShell |

These focused results are not production-build or installer-execution evidence. The output fixtures mock the PE reader. Real package provenance, certificate-table absence, embedded versions, complete package contents, source identity and hashes must be checked after the root installer entrypoint succeeds.

The baseline bootstrap installed or selected Visual Studio Build Tools 2026, its CMake, Strawberry Perl, Python 3.13, .NET 10, and archive tooling through the supported bootstrap route. This records activity on the actual host, not a pristine-VM certification. The dependency superbuild is still active; application compilation and packaging remain pending.

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

- [ ] Complete the active baseline production run and record its actual exit result.
- [ ] Independently review and incorporate application-cache discovery repairs.
- [ ] Reconcile the intended build candidate, then run its exact `build.bat` and `build-installer.bat`.
- [ ] Verify genuine unsigned Squirrel bytes, package versions, source identity, RELEASES contents and all output hashes.
- [ ] Integrate reviewed redesign source and complete its extraction/catalog and documentation requirements without claiming rendered acceptance.
- [ ] Continue the whole-application visual implementation inventory; preserve all existing functions.
- [ ] Obtain permitted built interaction and visual evidence before claiming appearance, accessibility or layout acceptance.
- [ ] Update this report with exact final receipts, remote proof and retained-work disposition.

