# Integration readiness after the default branch advanced

## Scope and source identity

Read-only review on 2026-10-03 compared integration source
`538378cb66cb09310212594bbfa07d9c2b6d860f` with the fetched default branch
`0c967a55786c07ef639a2cbefbe922b619c157d3`. Their common ancestor is
`ce883543177ef7df46fa5b798dcc5c7f3d2f8020`. No actual merge or product execution
was performed during this review. Existing publication runs remain preserved.

The default branch independently received:

| Commit | Change |
| --- | --- |
| `6994caf6f1bf0ec0c6f41ca77f29aec8c0c19053` | Stacked Plate Settings labels and row-wide dropdowns |
| `ad910deb23247ad0f1eb1a2a3848677b4f3a8ffd` | Dialog-caption title following and explicit synchronization |
| `0c967a55786c07ef639a2cbefbe922b619c157d3` | Documentation and corrected counts |

These changes belong to the other task and must be preserved. Its production run
[37149337544](https://github.com/Ding-Ding-Projects/BambuStudio/actions/runs/37149337544)
was in progress at observation. No result or rendered behavior is inferred.

## Predicted conflicts and required resolution

| Path | Required preservation |
| --- | --- |
| `CLOSEOUT_PROMPT.md` | One current record retaining both tasks' unfinished verification |
| `src/slic3r/GUI/Widgets/MD3DialogChrome.cpp` | Existing dialog show binding and `OnDialogShow`, plus caption idle title following and title-sync methods |
| `src/slic3r/GUI/Widgets/MD3DialogChrome.hpp` | `OnDialogShow`, `FollowDialogTitle`, `SyncTitle`, entrance ownership and title-tracking members |

`HANDOFF.md`, `README.md` and `ROADMAP.md` combine textually in the comparison,
but their current-state claims require deliberate reconciliation. Automatic text
combination does not establish semantic compatibility or a current handoff.

## Acceptance remains bound to the actual source

The successful production package at `75770f71f59358514df9d5af42b38402e538116c`
does not contain the newer default-branch changes. Those include
`OG_CustomCtrl.cpp/.hpp`, `OptionsGroup.hpp`, `Tab.cpp`, `AMSItem.cpp` and the
caption implementation. Earlier evidence retains its original source scope.

The eventual combined source needs its own hosted production verdict, matching
package and installed checks. Verify caption title updates alongside show, hide
and reopen entrance ownership. Verify stacked Plate Settings geometry in the
applicable language, theme, normal/minimum-size and measured display-scale tuples.
Source-oriented checks cannot establish rendered behavior.

New feature work remains frozen. Continue installed acceptance through the existing
matching-release route; do not substitute an older release or invent build provenance.
No physical printing or transfer is authorized. No deletion is eligible from this
review alone: preservation, completed verification, integration, archive read-back,
ownership and ancestry proofs remain mandatory.
