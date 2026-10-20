# Branch integration, 20 October 2026

Integrated the remaining source contributions into `main`, starting from `07e3b8d206f3e87be6743cca7756237ea9f49102`. The tested integration candidate is `b07eaf67478dbdcdf485de1010845c44a7b9751a`.

## Source coverage

| Source branch | Source head | Integration commit |
| --- | --- | --- |
| `integrate/lan-drop` | `e352f8ed1b7a0380fd0a99a04763c43392cd92a8` | `a423b7a7bcc4a957f69c9870b4a1ff9da03bfd91` |
| `gap/motion-tab-w5` | `0a8b9107703a406b0c90aec6e7eccb1ee0de5ee0` | `6a2b9829be88cfc2a934d3d3adc2c04d4060bcc7` |
| `gap/vocab-limits-w5` | `e2a3cf6d436061d1a250e5bb57ce49ca769a4552` | `970473c8a800ab21109f1193cce2474f37bf5d9f` |
| `recovery/reinstall-startup-tracing-20261013` | `864ccf78118d5b656c97fced0ac6a25ae4fe7334` | `b07eaf67478dbdcdf485de1010845c44a7b9751a` |

The three older LAN branches are covered by the existing newer squash, rather than added to main's ancestry:

- `gap/lan-drop-app-w1`, head `486546f2eaf3699bd6e7eb3026b0008ddc70fbd1`.
- `gap/lan-drop-site-w1`, head `5698fc9bb4032d82e9cc9c7366c5eec6bd0f5377`.
- `gap/lan-drop-integrate-w1`, head `4e0a7a2a59b486af7dfc270683b4504d0310686e`.

The older LAN integration contains both older component heads. Its complete `lan-model-drop`, `src/slic3r/GUI/LanModelDrop`, and `tests/lan_model_drop` trees match the newer squash exactly. Other differences retain the newer translation extraction, command palette ordering and updater correction. These original branches remain unchanged. The other 52 non-main branch heads were already ancestors of the original main.

## Merge resolutions

- Retained both Melody and LAN menu actions, the existing Express print mode and the newer build and updater changes.
- Added opt-in LAN model receiving, per-element motion controls, Unicode-aware personal wording validation and opt-in startup diagnostics.
- Merged translations by message identity and regenerated extraction, translation coverage and the documentation bundle with the repository tools.
- Corrected one test's hard-coded URL-case count for Node versions that normalize a caret to its encoded form; actual service/station agreement remains asserted.

## Verification

| Check | Original main | Integration candidate |
| --- | --- | --- |
| Node tests | 996 total: 979 passed, 7 failed, 10 skipped | 1,132 total: 1,112 passed, 7 failed, 13 skipped |
| CMake source contracts | 11 passed, 1 failed | 11 passed, 1 failed |

The failing test identities are unchanged. Four Node failures require unavailable wxWidgets headers/configuration. The other failures cover existing appearance history, release-date language and non-ASCII narrow string contracts. The existing CMake failure is the Home Assistant UI performance contract. Neither `offline-render.test.mjs` nor `dim-sum-catalog-online.test.mjs` was included in either Node run because they require unavailable browser/external-service routes.

Additional focused evidence: 528 motion assertions and 256 wording-schema assertions passed; 15 startup diagnostic unit tests and 9 native wording observation tests passed. The CMake-dependent LAN container installation contract passed separately with CMake available. All 9,578 translations validate; the documentation bundle is current; Express start G-code guards pass their check.

Native Windows compilation, GUI interaction, live container/browser operation and physical-printer behavior remain unverified in this Linux session. The startup tracer was not launched on a desktop. A successful merge or source check is not runtime or release acceptance.
