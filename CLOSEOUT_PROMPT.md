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
