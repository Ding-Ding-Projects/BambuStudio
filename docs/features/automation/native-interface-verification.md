# Hosted native interface verification

`scripts/ci/Verify-HostedNativeInterface.ps1` installs a published Squirrel
package on a disposable GitHub-hosted Windows runner, binds it to the requested
source commit, and runs one native interface scope. The implementation is
awaiting its first hosted execution. No local runtime or capture evidence was
produced while adding it.

## Invocation

Run one scope in each fresh hosted job. Replace the example tag and commit with
the exact release candidate. The checkout must be at that source commit.

```powershell
./scripts/ci/Verify-HostedNativeInterface.ps1 `
  -Tag md3-v123 -ExpectedSourceCommit <40-character-candidate-sha> `
  -Repository Ding-Ding-Projects/BambuStudio `
  -OutputDirectory "$env:RUNNER_TEMP/native-interface-evidence" `
  -Scope menus -Language en -Theme light -Scale 1 -Viewport 1200x800
```

| Scope | Required observations | Limits |
| --- | --- | --- |
| `menus` | Empty-scene short context menu, literal search, no matches, regex matching, Tab to regex, Escape clear/dismiss, nested Add Primitive menu, parent focus restoration, layout measurements | Requires the actual native accessibility provider to expose menu rows and search fields. It never invokes a primitive action. |
| `vocabulary` | Preferences opened through the Edit menu, native JSON picker, synthetic label load, replacement, visible invalid-file rejection, preservation of the valid mapping and clear | Original native accessibility text must remain unchanged. Stable real title pixels prove change/preservation/restoration; exact painted wording still requires image review. |
| `vocabulary-persistence` | Load a neutral mapping, observe it after a fresh process, clear it and verify original display after another fresh process | Uses generated neutral JSON only. Does not test exports or graceful application shutdown. |
| `slice-controls` | Empty-model proof, visible disabled Slice and Print/Send controls, disabled clicks that open no dialog, named options buttons with real chevron labels, measured action layout | This is an empty-scene preflight, not proof of slicing or device continuation. |
| `combined-print` | Native cube import, installed presets, real Slice and Print click, ready output on plate zero, existing Send print job dialog, Escape dismissal | No submit or printer selection. Missing presets, account requirements, version preflight or an unavailable dialog fail the scope. |
| `combined-send` | Native cube import, installed presets, real Slice and Send click, ready output on plate zero, existing Send to Printer storage dialog, Escape dismissal | No transfer or physical printing. It verifies the confirmation boundary only. |
| `cancellation` | Up to three actual combined requests followed by native clicks on the freshly rendered cancel target | Requires in-flight identity, cancellation without continuation, and an actually delivered stale completion rejected by the receiver. A timing miss returns nonzero with `not_observed`, never success. |

The real-cube scopes copy the same checked-in STL used by `drive-automation.py`.
They use the packaged companion only for the read-only `project_inspect` and
`presets_list` operations. All product mutations use observed native controls.
The versioned diagnostic observation also binds completion and continuation
to the current native generation and plate. The separate cancellation scope
requires positive rejection evidence rather than inferring success from an
idle snapshot.

## Matrix and evidence

Parameters accept `en`, `yue_HK` and `bilingual_en_yue_HK`; `light` and `dark`;
scales `1`, `1.25`, `1.5` and `2`; and client areas `1200x800` and `1000x600`.
The caller provisions an actual display at the requested scale. This script
does not change system DPI or synthesize scaling. Both the live layout header
and each captured native window must report the requested DPI. Client dimensions
are measured and adjusted through native window sizing, with four attempts.
An unsupported minimum size or scale is a failure, never a passing skip.

The driver uses the pinned `lowlevel-computer-use-cheap` route on an owned hidden
desktop for clicks, text, keyboard input and PrintWindow captures. A short-lived
helper runs on that desktop to observe native UI Automation and target keyboard
input at the actual focused owned control. UI Automation patterns are read-only.
There is no InvokePattern, ValuePattern setter, WM_SETTEXT, `invoke`,
`menu-popup` or other application test-command mutation. The existing
`app.probe()` path does launch `send-layout-probe.py` for read-only observations;
it is not an input mechanism. Modifier shortcuts are
not used because the pinned background-key implementation documents limitations
for controls that inspect physical modifier state.

Every successful input step captures its resulting focused surface. Raw records
include native names, focus, enabled state, rectangles, elapsed observation time,
layout-probe overflow measurements, image dimensions and SHA-256. The enclosing
receipt binds the release, source commit, executable hashes, release asset hashes,
driver hash, hosted run and requested/observed tuple. Elapsed observation time
includes automation overhead and is not a UI latency benchmark.

Each scope permits at most 30 images plus `install.json` and `runtime.json`,
matching the existing 32-file, 64-MiB automation evidence boundary. The wrapper
retains the `bambu-automation-v2` envelope and dedicated
`hosted-automation-public-v1.pem` recipient. Use
`scripts/md3/Open-HostedAutomationEvidence.ps1` with the expected run, commit,
tag and executable hashes to decrypt and verify it.

Only `receipt.json`, `envelope.json` and `evidence.aesgcm` belong in a hosted
artifact upload. Native observations, file picker images, temporary profile
paths and unreviewed pixels remain inside the encrypted bundle. The public-safe
receipt includes counts and fixed scope/tuple values, not native labels or paths.
Decryption verifies integrity; it does not approve privacy or publication.
Review every image before publishing it. No screenshot in this document is
fabricated to stand in for that pending review.

The vocabulary and persistence scopes retain 23 and 25 images respectively.
The invalid fixture uses unsupported schema version 2 and must show the rejection
message while leaving the valid replacement's title pixels unchanged. Each
display checkpoint waits for two identical title crops, with at most six
observations, and retains the full raw image that produced the stable crop.
Transient stability samples remain restricted scratch data; all per-input
captures and accepted stable images are retained in the encrypted bundle.
Original native accessibility text is required throughout. Pixel differences
and equality prove display changes and restoration, not the exact text content;
human review of the retained images is still required.

The persistence scope stops the owned process, verifies teardown, starts a
different process with the same isolated profile, and requires the mapped title
pixels to remain equal. After a visible clear action, a second fresh process
must restore the baseline pixels and the Load JSON control. The driver never
copies or reconstructs the display cache between launches. This tests persistence
through process termination; it does not claim graceful application shutdown.

## Failure and recovery

An absent, ambiguous or disabled target fails the affected scope. Provider
timeouts, truncated observations, incorrect process ownership, missing original
labels, unexpected confirmation state and layout overflow remain failures.
Native helpers have a 22-second watchdog; the controller waits at most 25 seconds
for each helper result. The existing hosted process holder owns application and
desktop teardown. Teardown must be verified before a scope can pass.

Run baseline scopes first. Repair any concrete hosted failure at its recorded
commit, then rerun the affected scope and tuple. Do not relabel missing-printer
evidence as completed printing, and do not mark the full matrix complete while
the cancellation scope or any requested tuple remains unverified.

## 粵語說明

呢個流程只喺獨立嘅 GitHub 託管 Windows 執行環境測試正式安裝包。
每個工作只跑一個範圍，真實點擊、鍵盤操作同逐步截圖都要綁定同一個
來源版本、安裝包、語言、主題、DPI 同視窗尺寸。未支援嘅狀態會失敗，
唔會當通過。新增流程目前仲未有實際託管執行結果。

個人用詞測試只產生中性示例 JSON，唔會讀取私人用詞檔。檔案選擇器同
原始介面記錄一律加密，逐張檢查私隱之前唔可以公開。
無效版本檔案必須顯示拒絕訊息並保留原有有效替換；兩次新程序啟動分別
檢查替換仍然生效，以及清除之後原文仍然恢復，唔會偷偷重建快取充數。
切片連接操作只去到現有確認視窗，唔會傳送工作或者啟動打印。
取消範圍最多實際嘗試三次，用當前繪製嘅取消按鈕位置點擊；需要真實取消同
接收端拒絕過期事件嘅證據。未撞到時序窗口會回報 `not_observed` 並失敗，
唔會用一個閒置畫面冒充已經測過。實際託管執行仍然待驗證。
## Slice workflow observation contract (version 1)

With `BAMBU_AUTOMATION=1`, `capabilities.sliceWorkflow` advertises version 1 and
`project_inspect.sliceWorkflow` returns diagnostics from the UI thread. This is
an observation surface, not an event-injection or cancellation command. It adds
no printer submission route and does not change existing confirmation dialogs.

| Field | Meaning |
| --- | --- |
| `schemaVersion`, `enabled`, `diagnosticOnly`, `eventCapacity` | Version 1, enabled opt-in diagnostics, capacity 16 per event ring. |
| `requestGeneration` | UI slice-request counter. It is distinct from the companion automation job counter. |
| `nativeGeneration` | Background process generation, also advanced when cancellation invalidates earlier work. |
| `modelRevision` | Active undo snapshot timestamp, an opaque revision identity, not wall-clock time or a content hash. |
| `outcome` | `idle`, `running`, `completed`, `failed`, `cancelled`, or `unknown`, from the existing atomic background outcome. |
| `cancellationRequested` | Existing asynchronous cancellation-request flag; it does not imply worker termination. |
| `workerStateKnown`, `workerRunning` | Nonblocking synchronized ownership observation. `workerRunning` is null when the existing worker mutex cannot be acquired immediately, never false by default. A known true includes started, running, finished awaiting consumption, or cancelled awaiting consumption. False alone is not success. |
| `processingPlateIndex` | Index resolved against currently owned plates, or -1 when no current plate matches. |
| `pending` | Action `none`/`print`/`send`, plate index, UI request and native generations, and equality against current and processing plate identities. No pointers are exported. |
| `completionSequence`, `completionEvents` | Process-local monotonic sequence and the latest 16 actual completion-receiver entries, oldest first. Each includes event/current generations, status, accepted flag and rejection reason. |
| `continuationSequence`, `continuationEvents` | Separate monotonic sequence and latest 16 consumed Print/Send continuations, with action, plate and generation identities. Dispatch is not proof that a dialog opened or a device received data. |
| `cancelTarget` | Current rendered cancel intersection, or `{ "visible": false }`. No native handle, model name, file path or printer identity is included. |

The completion receiver records before its early exits. `accepted` means the
entry passed shutdown, generation and ignore checks. It is not a claim of
successful slicing; inspect `status` separately. Rejections are `shutting_down`,
`stale_generation`, or `ignored`; accepted entries use `none`. Two fixed arrays
avoid allocation in the receiver. Serialization allocates only on an explicit
read request. These are bounded observations, not a permanent audit history.
Consumers must detect sequence gaps or overwritten evidence and never treat
missing entries as proof that an event did not happen. Process restart resets
both sequences. `modelRevision` is explicitly separate from both generation
counters and may move backwards after undo.

`cancelTarget.rect` and `canvasRect` are `[left, top, right, bottom]` in native
screen pixels. The rendered ImGui hit area is clipped to its actual child-window
clip rectangle, then converted from canvas pixels through the current native
client dimensions and screen origin. `frame` is the actual ImGui frame count;
`ageMs` is measured with a monotonic clock. The observation is invalid unless
it belongs to the same canvas and native generation as the observation, matches
the current ImGui frame, is at most 500 ms old, is inside the
visible canvas, and the notification remains in progress with a callback.
Every notification-render pass clears the cached area before skipping hidden
or overflowed entries. No draw is requested by this getter. Another frame,
state change, hidden canvas, or delayed read may therefore produce no target.
The driver resolves the owned native GLCanvas under that point and sends real
mouse input through the existing low-level route. The usual race between an
observation and an input remains possible and is reported, never hidden.

## Cancellation evidence and limits

The bounded stress fixture uses the existing closed cube STL, with every vertex
scaled by 20 to make a 200 mm cube. It is imported through the native file picker
and uses the same bundled preset profile as the ordinary combined-action flow.
Its exact generated file hash is recorded. This widens the real slicing interval
without adding artificial delays to product code. Printers with a smaller build
volume or unavailable bundled presets may reject it; that is an unavailable
fixture, not successful cancellation evidence.

The driver makes at most three genuine requests, alternating Slice and Print
and Slice and Send. Before each
cancel click it re-reads the opt-in observation, requires a live matching native
generation and pending action, then uses the real current rendered hit area.
Afterward it waits at most 90 seconds for ownership release, requires the
cancelled outcome and empty continuation, and retains the observations. Each
subsequent trial uses the same imported model and a newer genuine slice request.
A successful cancellation scope additionally requires an actual old-generation
completion at the receiver, with `accepted=false` and `stale_generation`.
Neither injected events nor fabricated delayed work are used. A machine that
does not produce that timing window returns `not_observed` and a nonzero exit.
A missed click, changed generation, truncated event history, unavailable target,
failed slice, or hanging worker cannot be labelled a pass. A confirmation reached
because slicing finished too quickly is dismissed with Escape only.

These checks cover same-plate cancellation/retry and observed stale completion.
They do not yet prove model/config edits, plate switching, application closure
during active work, every platform scheduler interleaving, or cancellation of a
noninterruptible kernel. They do not submit a print or transfer to hardware.
The ordinary combined Print and Send scopes separately require a successful
current-generation completion and matching consumed continuation before the
existing dialog is accepted as evidence. A missing printer/account dialog
remains unavailable even if the telemetry says dispatch occurred.

Required hosted review, currently pending:

- [ ] Build this exact candidate and execute the versioned observation contract.
- [ ] Observe real cancellation with an empty pending action and unchanged continuation sequence.
- [ ] Observe an actual stale completion rejected before any current-run mutation.
- [ ] Inspect every retained image and encrypted native record before publication.
- [ ] Repeat the required language, theme, viewport and real DPI tuples.
- [ ] Independently review shutdown, event ordering, ring truncation and native target freshness.

No local build, test, native execution or capture was used to prepare this
extension. Source review and whitespace checks are not runtime verification.
