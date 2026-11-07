# State and verification inventory

All states below are production requirements. The prototype's scenario selector supplies representative simulated states only.

| Screen | Stable states | Primary action and recovery |
| --- | --- | --- |
| discover | inspecting, existing-owned, fresh, unrelated-conflict | Continue after exact ownership inspection; preserve unrelated service |
| requirements | checking, ready, missing-runtime, engine-stopped, missing-compose, unsupported-host, installer-cancelled, license-required, restart-required | Check again or supported installation; cancellation is incomplete |
| configure | existing-preserved, fresh-defaults, fresh-folder, invalid-port, port-in-use, nonempty-folder, unsafe-path, custom-credential | Validate before review; clear confidential input on departure |
| review | ready, stale-environment | Show intended changes and preserved data; revalidate before start |
| execute | pulling, starting, local-health, lan-health, cancelling, cancelled-retained, failed-retained | Show real phase events; cancel owned work, then reconcile |
| ready | lan-verified-locally, second-device-unverified, running-after-cancel | Open actual verified URL; copy URL; recheck; logs; finish |

## Deterministic references

`prototype.html?step=N&scenario=S&lang=L&theme=T` selects the reference. N is 0 through 5. S is existing, fresh, missing, conflict, failed or cancelled. L is en, yue or both. T is light or dark. Unknown values fall back to documented defaults. The reference controls are demonstration-only and never call a host API.

Required tuples: each state at 960 × 720 and proposed 640 × 520 DIP; English, Cantonese and bilingual; light and dark; actual operating-system display scaling at 100%, 125%, 150% and 200%. Also verify enlarged text, high contrast and reduced motion. CSS viewport resizing alone is not display-scaling proof. For each actual built tuple record source, executable hash, viewport, scale, theme, language, exact state, capture method and privacy verdict. Measure overflow and control bounds; retain genuine captures separately from these references.

## Keyboard and assistive behavior

- Logical Tab order: demonstration controls, content controls, Back, Next. Do not trap focus in the step rail.
- Step changes move focus to the screen heading. Announce progress politely; alert only actionable failures.
- Back retains nonconfidential values. Escape requests cancellation during execution, with clear consequences before confirmation; it does not silently stop service or erase data.
- Every input has a visible label and associated help/error. Do not rely on color or icons alone.
- Keep the navigation footer visible with scrollable content at minimum size and all language modes. Long paths and URLs wrap; copying preserves exact values.
- Respect reduced motion. No progress animation implies measured completion. Loading and disabled controls include a reason.

## Decisive production checks

1. Existing service restart preserves queue, named volume and station credential; unrelated and remote configurations remain unchanged.
2. Fresh service accepts direct model upload without a drop code; management endpoints still reject absent/incorrect station credentials.
3. Nonempty folder, overlapping destination, invalid port and occupied port cannot reach start.
4. Cancellation during pull/start checks actual process and service identity; no unrelated process is terminated and retained service/data are reported truthfully.
5. Missing prerequisite, declined native consent, license requirement and pending restart never return success.
6. Protected values never appear in renderer diagnostics, process arguments, logs, resume files, URLs or captures.
7. A portable executable launches independently, performs real setup and verifies a real LAN URL. Same-host reachability and second-device reachability have separate verdicts.
8. Resume revalidates stored nonconfidential drafts and does not restore a custom credential.

Independent review and every production check are pending. The design author's source inspection cannot substitute for either.
