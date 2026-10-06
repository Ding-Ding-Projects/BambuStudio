# Native motion lifetime verification

The focused native-interface workflow has an opt-in `motion_runtime_only`
dispatch. It builds the production `MD3Motion.cpp` against the same cached static
wxWidgets dependency prefix as the application, then exercises real wx timer
events and actual wx window destruction on a newly created, non-input Windows
desktop. It never switches the input desktop, installs the product, connects a
printer, or launches the product executable.

`MD3MotionPreference.cpp` contains the unchanged production preference lookup.
Separating that translation unit avoids linking all of `GUI_App` merely to
exercise `Anim`. The fixture supplies `reduced()` with controlled inputs to the
production `reduce_motion()` policy. AppConfig persistence and the operating
system API are therefore **not verified by this fixture**.

## Fourteen runtime cases

| Case | Required observations |
| --- | --- |
| Completion | Initial zero, intermediate callbacks, monotonic progression, final one, exactly one completion, stopped timer and no later callbacks |
| Explicit stop | Stop immediately ends the timer; neither completion nor owner cleanup fires later |
| Reversal | The new visual path starts at the current value, decreases to zero, and cancels the superseded completion |
| Restart from initial callback | Only the replacement animation completes; the first animation cannot restart its timer afterward |
| Owner deletion in initial callback | Real weak-reference invalidation, no further visual callback, cleanup once and no completion |
| Owner deletion in final callback | Final visual update occurs while the owner exists, cleanup follows deletion, completion is suppressed |
| Owner deletion between callbacks | No visual callbacks after deletion; cleanup once and timer stopped |
| Animator deletion in initial callback | Detached run state allows the callback to return safely, with no later callback |
| Animator deletion in final callback | No completion after animator destruction |
| Animator deletion between callbacks | No later visual or completion callback |
| Hidden owner | The next real timer event settles to the final value once and stops |
| Initially reduced preference | One synchronous final update and completion, without starting a timer |
| Preference changed during motion | The next timer event adds only the final update and completion |
| System reduction precedence | A controlled system request settles immediately under the system preference |

The fixture records bounded numeric callback samples in CSV. Those are temporal
observations of the production animator, not screenshots or proof of the painted
interface. Timing tolerances allow normal hosted scheduling delays: each wait is
bounded to two seconds, an independent watchdog terminates the fixture after
30 seconds, and the existing process-job helper bounds each invocation to
45 seconds and requires the complete process tree to exit. The workflow step is
bounded to three minutes and the full cached build job to twenty minutes.

Assertions are explicit runtime conditions, unaffected by Release `NDEBUG`.
Both fixtures use `wxDEBUG_LEVEL=0` to match the cached wx ABI. This does not
disable the fixture conditions.

## Deliberately broken cancellation

CMake creates a separate build-directory copy of the production translation
unit, removing only the `wxTimer::Stop()` call in `Anim::Stop()`. The same real
event-loop fixture must exit with code 1 specifically at `explicit_stop`, after
the ordinary completion case passed. It must also restore its thread desktop
and close its owned desktop. A compile failure, crash, timeout, wrong failed
case, or missing receipt does not count as rejection of the mutation.

The workflow then runs the unchanged production translation unit. All fourteen
cases and desktop teardown must pass. The final receipt binds the source commit,
run/attempt, source and executable hashes, both process results, and the bounded
numeric output hashes. Only five fixed JSON/CSV files are retained. No account
identity, desktop name, local path, raw process output, or screenshot is included.

```powershell
gh workflow run native-interface-verification.yml --ref main -f motion_runtime_only=true
```

The other contract-only switches must be false. Default workflow behavior is
unchanged. A missing native dependency cache fails explicitly instead of building
the entire dependency stack or substituting a pure model.

## Current verification limits

This change has not been compiled or executed locally. Hosted compilation,
negative rejection, all fourteen runtime cases, and temporal records remain
pending until an exact-source run is inspected.

This fixture does not verify timer-start exhaustion, every widget adapter,
saved-preference round trips, actual OS accessibility settings, reduced-mode
notification readability, contrast, animation appearance, frame scheduling in
OpenGL, or an installed application. Those require their own installed flows.
Real per-input screenshots and temporal image sequences still need normal and
minimum viewports, English/Cantonese/bilingual modes, light/dark themes, and
measured 100/125/150/200 percent scale. Callback samples cannot satisfy that matrix.
