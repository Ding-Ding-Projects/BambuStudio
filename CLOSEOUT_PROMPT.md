# Continuation: responsive workflows and local history

Recorded 2026-10-04T22:18:27.172Z. The owner requested preservation and cleanup, then explicitly asked to pause the unfinished implementation goal after preservation. This record is a continuation handoff, not a completion or release claim. Tracking: [issue 56](https://github.com/Ding-Ding-Projects/BambuStudio/issues/56) and [discussion 57](https://github.com/Ding-Ding-Projects/BambuStudio/discussions/57).

## Main and preservation state

Main was safely fast-forwarded to `0c967a55786c07ef639a2cbefbe922b619c157d3` before implementation. The commit containing this file changes handoff documentation only. Feature code remains on eight isolated preservation branches. A push and exact remote-reference verification are the closing operation; confirm these references with `git ls-remote origin` before continuing.

| Area | Preserved branch | Source checkpoint | Preservation tip |
|---|---|---|---|
| Menu lifetimes and printer switching | `codex/bambu-native-lifecycle` | `4e4d192b9cdb0ca222b8c9da768fa24586f1822f` | `67a8822a5489b464cce066742333bbe9ebe7c3d7` |
| Independent settings drafts | `codex/bambu-draft-tabs` | `5010d13b6325abf5015e426252a2d13bad193553` | `8cd7f8dcdefcff79c1db4d2a69ef8f3935b03a3b` |
| Prepare scroll ownership | `codex/bambu-sidebar-scroll` | `e6453379bb78c9cad7a673325651f9c635e0a8d0` | `b7c71450930a3648e79b7bfd8673cca4f07c68d0` |
| Readable date formatting | `codex/bambu-readable-dates` | `47f0c14452587654a8cce274d1e992e6fc9880ae` | `18e9728c3b4e15063128d90ebcbb352f339e5c21` |
| Detached import and automatic simplification | `codex/bambu-import-simplify` | `33dad3b7ed58071eb4c7c8e8301b29dcfdaa71d7` | `bf99b138f0c55396869b50a567ac214fad04361d` |
| Quiet workflow defaults | `codex/bambu-quiet-prompts` | `d2804f1e2098bf088877c8b69e24755edceee42d` | `ddb86c434d7a4989ccaaad217c9350e87df1d5bf` |
| Camera continuity and digital view controls | `codex/bambu-camera` | `cc3dfe92e61a3b45ae29aed96bd19ff6c8bf991e` | `ba8abf2f6e83d58e7962fd30e1721b7f6f56a300` |
| Unified local history and printer incidents | `codex/bambu-local-history` | `f58ed09c69931fbc28975ccc572f11165483ef1c` | `768ec340f127403098365f77360c4fbf770eac4e` |

Each branch has its own current CLOSEOUT_PROMPT.md with changed paths, completed checks and remaining work. The preservation tips intentionally include [skip ci] to avoid starting new build/release workflows during the requested closeout. This does not establish any verification result. No feature branch is ready for deletion or a completed merge.

## Scope and verification

### Menu lifetimes and printer switching

Production-body lifetime doubles: 16 assertions in 8 cases; source contracts: 7/7; stale-generation mutation failed as expected, restored version passed.

Remaining: Full native build, wx popup dispatch, both Ink deletion paths, canceled/missing presets, P1S/H2C and multi-plate runtime reproduction remain unverified.

### Independent settings drafts

Source boundary and whitespace checks passed. Authored native tests were not compiled or executed.

Remaining: Undo must bind stable project-tab identity rather than filename and include attachments in memory accounting. Connect LocalConfigHistory recording/restoration. Fix duplicate host activation and removed-key handling. Verify close/restart, supported field controls, conflicts, no-activation Save as preset, localization, accessibility and layout.

### Prepare scroll ownership

Scrollbar contracts: 10/10; sidebar-width contracts: 4/4; scroll-owner contracts: 4/4. Removing the owner relationship made the negative contract fail.

Remaining: Independent interaction candidate S-INT-1: wx wheel delivery may be consumed by the zero-rate inner scroller. Refute against the pinned wx source and a native wheel test. Keyboard/focus/search, last-row access, resizing and scale matrix remain unverified.

### Readable date formatting

JavaScript tests: 29/29 across human-date (3), site (24), changelog-date-row (2); seven syntax checks. Numeric-month mutation failed, intact version passed.

Remaining: Three authored native Catch cases remain unrun. Native compilation, timezone boundaries, visible date coverage and wider labels at all required language/theme/scale tuples remain unverified.

### Detached import and automatic simplification

Source boundary and whitespace checks passed. Six native Catch cases were authored but not run. Source is not compiled.

Remaining: Scene and texture publication remain synchronous/unbounded. Prove transactional cancellation and exception rollback after publication starts. Reader/hull cancellation is coarse. Archive parsing uses a modal polling owner. Extent rescaling can alter angles, volume and deviation beyond the simplifier error setting. Review that tradeoff, saved metadata and recoverable originals. Finish localization, documentation and measured native responsiveness.

### Quiet workflow defaults

Eight source contracts and eight negative mutations passed. Source correctness review found no changed-line defect.

Remaining: Independent quiet-scope candidate Q-INT-1: the retained H2S PPA-CF/PPS-CF first-use Wiki modal still interrupts slicing. Refute scope and convert unsolicited educational flow to inline advice if accepted. Fresh/migrated profile, manual-help/update and actual native slicing verification remain pending.

### Camera continuity and digital view controls

MSVC standalone playback assertions: 18 passed; geometry assertions: 16 passed. Deliberate retry-cap and zoom-cap mutants failed as expected.

Remaining: Full native build, actual rendering/input, explicit Stop, hidden/minimized continuity beyond old cutoffs, reconnect authentication states, resize/fullscreen, pointer anchors and pan bounds remain unverified.

### Unified local history and printer incidents

Ten standalone printer lifecycle scenarios passed using history-engine stubs. Source boundary and whitespace checks passed.

Remaining: Real libgit2 transitions, submitted-query tests and full native build are unverified. Add automatic preset hooks and draft record/restore adapter. Only model/preferences restore exists. The graph is a textual parent-edge list; comparison needs a proper per-key delta. Complete incident refresh, validated preference restoration, appearance/date integration and selection retention. No native UI evidence exists.

## Build and evidence

The isolated supported one-click baseline build is pinned to `0c967a55786c07ef639a2cbefbe922b619c157d3`, not to the new feature branches. The dependency build was still running at handoff with at least 23 completed targets, including Boost, OpenCV, OpenSSL and libgit2. No main application compile, runtime or installer verdict has arrived. Retain the verification checkout and its build-logs/baseline-receipt.json in the owning workspace, which identifies the process, exact paths and resume command. Do not start a second build or change the pinned checkout while that process is active.

Existing MSVC 19.44, Windows SDK 10.0.22621 and CMake 4.4 were reused. Bootstrap installed Strawberry Perl, 7-Zip and user-scope Python 3.13. Node 22.22.2 MSI returned 1619; an official SHA-256-verified portable Node 22.22.2 was supplied in the build workspace without downgrading global Node 26.10.0.

No genuine built screenshots were produced for these changes. The existing capture manifest describes older source 2dcc26658 and lacks the per-image privacy/freshness proof required for a new public gallery. Supplied printer/camera images were not published. No gallery, release, installer verification or new deployed behavior is claimed.

A draft helper accidentally wrote four undo-related files in the primary checkout. The complete tracked patch and both new files were copied into the draft branch and a separate recovery copy. SHA-256 checks and reverse-apply validation proved preservation, after which only those exact primary edits were removed. Primary returned clean at the original baseline. The draft branch retains the source; do not repeat the recovery.

## Continuation order

1. Read this handoff and each branch's CLOSEOUT_PROMPT.md, verify remote tips, then inspect source changes. Preserve unrelated worktrees and branches.
2. Read the pinned baseline build's terminal result. Reuse its cache only after the process exits.
3. Refute and repair the named scroll/quiet candidates. Complete draft identity/undo/history and transactional import publication. Finish history hooks, comparison, graph and restoration.
4. Integrate only coherent candidates into a new pinned verification checkout. Resolve shared MainFrame/Plater/Preferences/CMake overlaps explicitly. Run focused behavioral tests and negative regressions, then compile the native application.
5. Perform real hidden-desktop interactions and capture source/hash-bound evidence for the requested languages, themes, minimum/normal layouts and 100/125/150/200% scales. Do not provoke physical printer faults or issue print/heater commands.
6. Integrate completed, verified work into main, push and prove the remote reference. Publish and verify the normal unsigned Squirrel release only after its requirements hold.
7. Remove only task-owned, clean, fully merged and remotely proved candidates after a verified complete archive. Keep unfinished branches and active build caches.

## External limitations

The authenticated status enrollment route was unavailable, so the local session status is a fallback and not evidence of remote delivery. The Project query lacked read:project scope; no Project was mutated. Material Designer's required native flow was unavailable in the inspected local installation; new-surface design/runtime evidence remains pending. Existing unrelated backlog was not adopted.

## 廣東話交接

今次按擁有人要求先保存，再暫停未完成嘅實作目標。八條功能分支各自保存程式同交接，main 呢次只更新文件。原生應用程式未完成編譯，亦未有實際介面擷圖或者新版本發佈。已通過嘅小範圍測試唔等於整個功能完成。下次要先核實遠端分支，再處理上面列出嘅接駁、取消、Undo、歷史同互動驗證。未完成嘅分支同建置快取必須保留。

---

# Historical interface continuation

The earlier record below is retained for its exact evidence and unfinished work. Its prior task restrictions and moving-reference claims do not replace the current continuation above.

# Continue BambuStudio native interface verification

## Objective and constraints

Obtain a green combined hosted result for the repaired candidate, verify the
matching installed package, integrate completed verified work into main, and
perform only archive-backed, ownership-proven cleanup. New features remain frozen.
All product compilation, tests, packaging, installation, slicing and UI execution
must run on hosted Windows runners. No local product execution, physical printing
or transfer. Preserve the final printer confirmation. Review genuine encrypted
captures before accepting visible behavior.

## Preserved source and exact evidence

- Integration branch: `feature/ui-integration`. Source before this evidence-only
  update: `667ae19a1d2447b9e9b1e33d612611ec8e52b747`, pushed and verified.
- Main: `0c967a55786c07ef639a2cbefbe922b619c157d3`, fetched and independently
  inspected after another task added stacked plate rows and caption title following.
  The common ancestor remains `ce883543177ef7df46fa5b798dcc5c7f3d2f8020`.
  Completed-main integration and
  the final combined-main result remain outstanding.
- Latest user request also asks to apply upstream updates. The fetched
  `bambulab/BambuStudio` default `master` tip is
  `da8b44ee34dd349f2ae0df3f1cbae366df482354`. Explicit ancestry checks succeed
  against both the integration source and remote main; both missing-commit counts
  are zero. No upstream product merge is needed at this observed tip.
- Const repair: `e8c1614d150b62366d6c02f72c020ee0131c6e3b`, integrated at `335a73234`.
  Only the declaration and definition of `StateColor::colorForStates(int)` gain
  `const`; the algorithm and StateHandler const-reference semantics are unchanged.
- Production build/package job `111252033608` in run `37139881377` passed
  at `75770f71f59358514df9d5af42b38402e538116c`, completing at
  `2026-10-03T18:13:49Z`. Native compilation, Cantonese catalog, imported DLLs,
  Squirrel creation, package validation and installer upload succeeded.
- Installer archive `11281486108` has API-reported size 1,542,022,595 bytes and
  SHA-256 `d6973e970b968b8fff1f5342ba5cad9a8cf440509d5d85374f817f6697299668`.
  It is not expired. Archive bytes have not been independently downloaded here.
- The complete run remains pending publication in the preserved serial queue.
  The earlier `37138948887` build at `68dabac8f16b83065c0e58a05e687a0708f3ecde`
  also passed compilation/packaging and waits publication; its managed job passed
  27/27 cases. Its observation ended after 55 minutes. The later observation ended
  at `2026-10-03T18:15:45Z` with publication pending. Resume on a terminal result
  or matching release, without restarting an unchanged observation.
- Hosted contracts `37139896660` at `75770f71f59358514df9d5af42b38402e538116c`
  passed nine Python cases, four invalid-desktop cases, four invalid-executable
  cases, actual absence and persistent lifecycle. Four CRLF source hashes match.
- Authenticated diagnostic `37139898854` at that verifier targeted historical
  `md3-v190` / `35d1074faea221fa4f289f1db1e0ee428a90d701`. Transport succeeded,
  driver exit 2. Thread ownership, contextual exit-state observation and closure
  passed; `GetThreadDesktop` returned null/native code 0. No name query or
  continuation. Four source hashes and the 6,382-byte log hash match. Teardown
  verified. `STILL_ACTIVE` is not unconditional liveness proof. No initializer
  cause is identified. Further historical-package diagnosis is held.

## Next safe steps

1. Resume when existing release publication changes state. Read the exact combined
   verdict and repair only a concrete failure with bounded independent review.
2. Verify a non-draft release whose immutable tag and recorded full source SHA
   match the accepted candidate. Verify Setup.exe, its hash, RELEASES, full package,
   SBOM and installed executable/companion identity on the hosted runner.
3. Run installed `slice-controls`, vocabulary import/replace/malformed/clear and
   persistence, root/nested menus, temporal checkbox, combined print/send and real
   cancellation scopes. Preserve source/package identities, actual cancel-control
   evidence, generation checks and final confirmations. No hardware submission.
4. Complete English, Cantonese and bilingual, light/dark, normal/minimum and
   measured 100/125/150/200% display coverage. Standalone display provisioning
   evidence does not establish product layout or the minimum-size contract.
5. Integrate only completed verified task-owned work, push main, verify its remote
   SHA and obtain the final combined verdict. A read-only comparison with the new
   main predicts conflicts in this record and `MD3DialogChrome.cpp/.hpp`. Preserve
   both entrance show/hide ownership and caption title following/SyncTitle.
   Reconcile HANDOFF, README and ROADMAP even where they combine textually.
   The older package cannot verify these combined product changes. Include title
   updates, show/hide/reopen and stacked Plate Settings geometry in the resulting
   exact-source hosted acceptance. See `docs/features/automation/integration-readiness.md`.
   Before deletion, inventory every
   branch, checkout and stash, create/read back the complete eligible archive,
   record bytes/entries, and prove ownership, inactivity and merged ancestry.
   Active, incomplete, unmerged or uncertain candidates remain retained.

Evidence index: `docs/features/automation/README.md`. Latest exact receipts:
`hosted-verification-37138948887.json`, `hosted-startup-contracts-37139896660.json`
and `hosted-startup-creation-37139898854.json` under that directory, plus
`hosted-production-build-37139881377.json`. Runtime and
rendered claims remain unverified. No archive or deletion has occurred. Limit
unchanged attempts to three and each external observation to one hour; record
pending state and retry condition when the bound is reached.

Preservation inventory: nine clean checkouts, eleven local branches and no stashes.
Eight non-main checkout tips are not ancestors of remote main and remain retained.
The prepublication native-verification route was assessed but not implemented: it
would require coordinated authenticated source/identity changes across the installer,
driver and evidence reader. Continue through the existing supported release route.

## Adopted responsive continuation records

- [Native lifecycle](docs/integration/continuations/native-lifecycle.md), source `67a8822a5489b464cce066742333bbe9ebe7c3d7`, integrated for combined verification; native acceptance remains pending.

- [Settings drafts](docs/integration/continuations/draft-tabs.md), source `8cd7f8dc`, integrated for combined verification; named functional gaps and native acceptance remain pending.

- [Sidebar scrolling](docs/integration/continuations/sidebar-scroll.md), source `b7c71450`, reconciled; native wheel/input acceptance remains pending.

- [Readable dates](docs/integration/continuations/readable-dates.md), source `18e9728c`, reconciled; native formatting and visual acceptance remain pending. Combined scrollbar source check identified the draft panel native-scroller construction, now repaired to the existing shared scroller.

- [Import and simplification](docs/integration/continuations/import-simplify.md), source `bf99b138`, reconciled; transactional publication, cancellation and native acceptance remain pending.

- [Quiet workflow](docs/integration/continuations/quiet-prompts.md), source `ddb86c43`, reconciled; progress-card wrapping and stack geometry retained while automatic daily tips are removed. Native acceptance remains pending.

- [Camera](docs/integration/continuations/camera.md), source `ba8abf2f`, reconciled. Standalone playback and geometry executables passed under the available GCC compiler; native media/backend interaction remains pending.

- [Local history](docs/integration/continuations/local-history.md), source `768ec340`, reconciled; preset/draft hooks, comparison, restoration and native acceptance remain pending.
