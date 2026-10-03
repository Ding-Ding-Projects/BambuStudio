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
| `cancellation` | Up to three actual combined requests followed by native clicks on the freshly rendered cancel target | Requires in-flight cancellation without continuation and either an observed disabled next-action overlap window or an actually rejected stale completion. The two evidence types remain separate; a timing miss returns nonzero. |

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

### Isolated startup diagnosis

The separate `hosted-startup-diagnostic.yml` workflow diagnoses startup of the
fixed `md3-v190` package from product commit
`35d1074faea221fa4f289f1db1e0ee428a90d701` using an independently identified
current verifier. It runs only on a disposable hosted Windows runner, with a
20-minute job limit. The fixed `startup-diagnostic` scope launches and stops the
owned application without feature input, screenshots, display provisioning or
minimum-size interaction. Normal verification still requires the verifier
checkout to match the product source exactly.

The diagnostic authenticates the product release, installer and executable
identities separately from the verifier commit. Encrypted `runtime.json` also
binds all 11 fixed verifier files listed by `startup_diagnostics.VERIFIER_FILES`,
using `sha256-lf-v1` hashes after CRLF-to-LF normalization. To open this evidence,
the administrative reader additionally requires `-ExpectedVerifierCommit` and
`-ExpectedVerifierManifestPath`. Prepare that manifest independently from the
exact verifier commit, never from the downloaded evidence. Its fields are
`source_commit`, scalar `hash_format` and `files`, with every fixed path mapped
to its SHA-256. The reader compares every entry before creating private output.

Bounded startup receipt and stdout/stderr data remain inside encrypted evidence
and are collected only after verified teardown with zero recorded operations.
Only the fixed receipt, envelope and ciphertext are uploaded. A completed
diagnostic retains public product status `failed` and `diagnostic_only=true`;
it cannot satisfy installed feature, cancellation, vocabulary or visual checks.
Run [37095786787](https://github.com/Ding-Ding-Projects/BambuStudio/actions/runs/37095786787)
was queued at this documentation checkpoint. No startup result or cause is
claimed here; the earlier zero-operation failures remain unresolved.

After the same verified holder identity and teardown checks, startup diagnostics
also inspect only the isolated launch profile's immediate `log` directory.
Collection rejects reparse ancestors, limits enumeration to 64 entries and
accepts at most three native logger filenames: the launched PID's
`studio_<timestamp>_<pid>[_enc[_cn|_dc]].log.<counter>` or
`crash_<timestamp>_<counter>.log`. Each file is capped at 512 KiB and the total at
1.5 MiB. Oversized or unsafe collections report unavailable without partial log
contents. No configuration files or other profile directories are collected.
Log bytes and filenames remain inside encrypted `runtime.json`; the existing
11-file verifier manifest already binds the changed collector. This is added
diagnostic coverage, not evidence of a startup cause or a successful launch.

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
That exact fresh input observation anchors native generation `G`, completion
sequence `S`, request identity, model revision, current/processing plate and
continuation sequence. Input requires `cancellationRequested=false`; counters
must be unsigned 64-bit integer values, not booleans or floating-point values.
The current `request_stop()` contract advances the cancellation epoch to `C=G+1`,
so post-cancel observations must use `C`, retain request/model/plate identity,
report `cancellationRequested=true`, clear the pending action and leave the
continuation sequence unchanged. A generation at the maximum unsigned value
cannot establish this next-epoch proof and is rejected before input.

Afterward the driver waits at most 90 seconds for both ownership release and a
new actual receiver entry with `sequence>S`, `accepted=true`, `rejection=none`,
`status=cancelled` and `eventGeneration=currentGeneration=C`. The terminal
observation must still belong to `C` and report known released ownership with
the cancelled outcome. An outcome without a delivered accepted event, an old
event, unknown ownership, sequence overwrite or identity drift cannot pass.
The receipt retains the anchor, accepted event and terminal observation. Each
subsequent trial uses the same imported model and a newer genuine slice request.
The driver attempts the next action before waiting for the prior completion.
The product intentionally disables both combined controls during worker ownership
(`MainFrame::update_slice_print_status`, `enable_output`) and independently
rejects another request in `Plater::priv::on_action_slice_plate`. When the
post-cancel observation still reports ownership, the driver requires the next
control to be disabled, clicks its real area, and checks that request identity,
pending action and continuation remain unchanged. Both positive ownership
observations must belong to the same cancellation epoch `C`, request, model and
plate to count this overlap invariant. A completion during
the click leaves that attempt `not_observed`.

A naturally delivered old-generation completion is separately checked for
`accepted=false` and `stale_generation`. A successful cancellation scope needs
explicit cancellation plus either that actual rejection or the observed disabled
overlap invariant. The receipt always reports stale completion as `not_observed`
when only the disabled-control invariant was observed.
Neither injected events nor fabricated delayed work are used. A machine that
produces neither evidence window returns `not_observed` and a nonzero exit.
A missed click, changed generation, truncated event history, unavailable target,
failed slice, or hanging worker cannot be labelled a pass. A confirmation reached
because slicing finished too quickly is dismissed with Escape only.

These checks cover same-plate cancellation/retry and whichever overlap or stale
completion evidence the receipt explicitly records.
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

## Synchronized ownership repair evidence

The earlier diagnostic called `BackgroundSlicingProcess::running()`, which reads
plain `m_state`. The worker writes `m_state` under `m_mutex` when starting,
finishing, cancelling and exiting. Repeated UI diagnostics therefore introduced
an unsynchronized reader. `automation_worker_running()` now acquires that same
mutex with `std::try_to_lock`, returns `std::nullopt` on contention, and evaluates
the state only while the RAII lock is held. It does not wait, notify, schedule,
change state or alter the existing worker lifecycle. Existing scheduler accessors
are unchanged by this diagnostic repair. Paint now reads only the existing atomic
native generation through `automation_slice_native_generation()`.

The serialized contract uses `workerStateKnown=false` with `workerRunning=null`
for contention. Native observations reject inconsistent known/null pairs; the
cancellation driver only treats exact `true` as owned and exact `false` as
released. Unknown results receive bounded retries, never idle/success credit.
This includes the native helper's fresh pre-click check and the post-cancel
ownership-release loop. A helper clears its candidate rectangle before each
attempt, so an unknown observation cannot reuse an earlier target.

Focused hosted regression requirements, not executed locally:

- Hold the actual background mutex from another thread while requesting the
  diagnostic: it must return unknown without waiting for release. Release it,
  then verify the subsequent known state matches the worker lifecycle.
- Run repeated `project_inspect` reads during actual slicing and cancellation;
  ensure each unknown worker state is serialized as null and never counted as
  release or a successful disabled-window observation.
- During actual cancellation, retain per-input images and bracket a disabled
  next-action click with known ownership reads, or report that window unobserved.
- Keep stale-event rejection and disabled overlap evidence separate. A disabled
  control result never substitutes for a claim that a stale completion occurred.

The first requirement still needs a native test fixture that owns the real
mutex; no public bridge command exposes or holds it. Source lock-lifetime
inspection is evidence for the repair, not an executed concurrency regression.

## Fresh target timeout regression

`select_cancel_observation` is the exact selection helper called by the native
cancel-input path. It initializes an empty target and a false freshness flag,
clears candidates on every iteration, and returns only after a complete valid
observation sets that flag. Unknown-only observations, an expired target followed
by unknown state, and a response arriving after the deadline cannot authorize
input. The target age includes the observation transport time, and native input
checks age again after resolving canvas ownership and geometry.

`scripts/md3/test_native_cancel_observation.py` loads the actual selector,
cancellation anchor, epoch validator and completion waiter from the driver's AST,
together with `require`. It executes production observation logic with a
deterministic clock, without importing native providers or loading an application.
The original seven cases cover unknown-only timeout,
stale-then-unknown, invalid coordinate space followed by unknown, fresh success,
read-deadline overrun, transport-age expiry, and changed generation. The tests
check that the caller's input boundary is never reached for rejected selections.
They do not claim native input delivery or real product execution evidence.

Eleven additional cases cover the `G` to `G+1` transition; already-cancelled
input and invalid counter types; generation/request/model/plate/continuation
drift; a cancelled outcome without a receiver event; rejected, unsuccessful or
wrong-generation events; pre-input events; delayed events and unknown ownership;
unknown-only ownership; overwritten history; late responses; and a deliberate
mutation of the actual epoch equality check. The mutation case first requires
the real validator to reject a changed terminal generation, then proves that
removing exactly that production comparison makes the same bad observation pass.
This is a focused negative regression, not synthetic product-event injection.
All 18 cases run through the existing hosted native-interface workflow command;
no local execution was performed for this repair and hosted results are pending.

Execute on the hosted verification runner only:

```powershell
python scripts/md3/test_native_cancel_observation.py -v
```

The disabled next-action path now resolves the actual native child beneath the
observed control point, including disabled children. It requires the child's
owning process, native screen rectangle and disabled state to match the observed
control before the low-level click. A missing native child or differing geometry
fails the observation; no parent-window input is substituted. The retained
per-input capture and subsequent unchanged workflow identity remain required.

These added regressions and native input changes are source-complete but have
not been executed locally. Hosted results remain pending.
