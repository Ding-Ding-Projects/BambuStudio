# Native review tuple inventory

[繁體粵語](native-review-tuples.yue_HK.md)

`scripts/md3/review-tuples.py` prepares the nine boundaries in the
[built-review queue](../../../design/workflow-refresh/built-review-queue.md).
It does not launch a process, change preferences or host settings, send input,
capture a screen, accept a review, or promote evidence. Every output carries
pending status or a specific mismatch; **no comparison produces a passing review**.

## Requested values and observations

The inventory contains 192 requested combinations per boundary, 1,728 in total:

| Field | Requested values |
| --- | --- |
| `language` | `en`, `yue_HK`, `bilingual_en_yue_HK` |
| `theme` | `light`, `dark` |
| `scalePercent` | 100, 125, 150, 200 |
| `density` | `comfortable`, `compact` |
| `motion` | `full`, `reduced` |
| `geometry` | `normal`, `minimum` |

These are boundary-level intentions, not fixtures or individual state coverage.
Expand each boundary's actual reachable states according to the queue. Inventory
generation checks the exact boundary order against `remainingVisualCoverage` and
stops if that contract changes. It never updates that source or its completion flags.

With an available Python 3 interpreter:

```text
python scripts/md3/review-tuples.py inventory
python scripts/md3/review-tuples.py compare --requested requested.json --probe native.jsonl --pid 123 --hwnd 456 --tag owned-observation
```

Use the `requested` object from one inventory row as `requested.json`, not the entire
row. PID, HWND and tag are independently known identities from that observation's
driver session, not values discovered by trusting the input dump. Omitting `--probe`
returns pending with `nativeProbe` unavailable. Standard output is JSON; the helper
writes no files. Exit 0 means inventory generation only; 1 means invalid input;
2 means a known mismatch; 3 means pending observations. None means visual acceptance.

The comparison reads the existing native NDJSON format documented in
[Runtime layout probe](../design-system/layout-probe.md). It requires exactly one
header, one matching shown top-level target, the final `end` record, exact PID/tag
agreement, finite positive scale and positive integer client allocation. It retains
requested and observed values separately. Duplicate JSON keys and non-finite JSON
constants are rejected. Files are bounded to 16 MiB. No raw labels are copied to output.

The comparison directly reuses `local-native-review.py::validate_probe`, passing the
independently requested `expected_language`, `expected_theme` and `expected_density`
keyword arguments. Its unchanged defaults are `en`, `light` and `comfortable`, so the
existing initial-shell driver still requires that exact profile. The raw header is
never rewritten to make the validator pass. A mismatching mode is reported separately
from a matching native observation; neither produces full-tuple acceptance.

This output is **not a version-1 layout receipt**. The existing shared
`validate-layout-probe.mjs` contract requires a source-bound artifact, screenshot,
ownership/privacy assertions and computed element measurements on its specified
headless route. A native dump alone cannot satisfy those requirements, and this helper
does not fabricate DOM measurements, a screenshot, source identity or a receipt.
The native report's selected identities are correlation checks, not live ownership proof.

## Geometry and currently unavailable observations

Normal means 1200 × 800 DIP. The helper compares the observed native client allocation
with that target multiplied by the observed scale, allowing only half a native pixel
for integer rounding. An allocation clamped by the application's minimum is a mismatch,
not permission to silently change the requested normal size.

Minimum comes from `GUI_App::get_min_size()`:
`max(1000,76*em)` × `max(600,49*em)` in native units. `minimum_geometry(measured_em)`
implements that calculation and rejects booleans, zero, negative, non-finite and
overflowing values. For example, a separately measured native `em` of 20 gives
1520 × 980, not 1000 × 600. The helper does not derive `em` from a filename, guessed
font size, display scale, screenshot or requested value.

The current native probe exports no measured native `em`, actual minimum allocation
policy or effective motion policy. `measuredNativeEm`, `minimumGeometry` and
`effectiveMotion` therefore remain explicitly unavailable even when all observable
fields match. Extra similarly named JSON fields are not accepted as new measurements.
The pure minimum calculation is useful to a future measured adapter, but is not wired
to acceptance until that adapter has an actual observation contract.

The probe's DPI is taken from its first top-level window. Comparing a different
monitor's dialog requires independent target-specific DPI evidence; the current
helper does not prove it. Native client bounds also establish neither embedded DOM
overflow nor ImGui interior geometry, text fit, focus, state transitions or privacy.
Missing account/device/fixture data remains a boundary-specific availability limit.

## Future local-driver integration

Integration belongs inside the owned session in `inspect_shell()`, after provenance
and current process/window identity checks and before its `finally` teardown. The
initial-shell command does not leave a resumable process. A later driver must obtain
separate authorization for actual interactions and observe current localized control
names and current target identities. Do not replay coordinates from a historical capture.

1. Open the real Preferences language selector, choose the required language, and
   observe the resulting native header and controls. Handle any actual reload prompt
   without assuming that a saved configuration value proves applied wording.
2. Use Appearance's Theme and Density controls for the requested values. Observe the
   actual dark-mode and density fields after layout settles. Restore original settings
   in the disposable session when appropriate; never edit the user's profile.
3. Use the real `Interface motion` control. Its choices are `System` and `Reduce motion`.
   `System` does not guarantee full motion because the operating system may reduce it.
   A future adapter must observe the effective policy; selecting a row is insufficient.
4. Measure the target window's DPI and native `em` before requesting client geometry.
   Use real window operations and read the resulting allocation back. If the scale is
   not the requested one, report mismatch or unavailable. Do not change host display,
   theme or accessibility settings to fill the matrix.
5. Obtain a fresh complete native dump at each actual queue state, then compare it.
   Preserve source/build binding, native action history and any separately authorized
   captures through their existing contracts. Matching tuple fields alone never close
   a boundary or the larger review.

## Verification

`python -m unittest discover -s scripts/md3/tests -p test_review_tuples.py -v`
checks the 1,728-row inventory, reuse of the current validator, numeric refusal,
identity/completeness checks, individual mismatches and unavailable measurements.
All fixtures are synthetic, offline inputs. No runtime, visual or hardware evidence
is produced by those tests.
