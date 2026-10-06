# Current continuation, 6 October 2026

The build objective is active and incomplete. See the
[verification report](docs/integration/build-verification-20261006.md).

## Objective and current direction

Make the exact root `build.bat` and `build-installer.bat` succeed with supported
automatic dependency setup, then independently verify unsigned Squirrel.Windows
outputs, exact source identity, versions and hashes. Preserve the original
`9a55b7aa1e900f85c2ded1389854beefabff159f` baseline and reviewed
`d048cfc3040a1566b78e03334f3871f1dd0144bb` integration, both ancestors of current
main candidate `cc059003d362b87d53786152ee8c28621d9c2813`.

The latest design direction remains a complete new appearance for the entire
application, preserving every function and feature. Design, implementation and
review use `gpt-6-astra`. A selective design-reversal note remains documented.
This larger objective is incomplete and must not be closed by build-only proof.

## Exact source and production state

- Local main and the fetched and directly queried remote main matched
  `cc059003d362b87d53786152ee8c28621d9c2813` before this report update.
- The isolated `task/main-build-verification-20261006` checkout remains pinned
  to that source. Its exact root build started at `2026-10-06T13:55:51Z`, using
  the supported read-only `-DependencyCacheDirectory` route. Application build
  and staging directories were absent before invocation. Native compilation is
  active; no terminal success is claimed.
- Generated `routeTree.gen.ts` has the exact committed bytes, but reused index
  metadata retains a porcelain modification. The live source and index remain
  unchanged. A disposable fixture proved single-file staging clears only that
  metadata when whole-index-tree and raw-byte equality are checked. Genuine
  content changes still fail the production source check. Wait for the terminal
  result before considering production metadata recovery.
- The separate redesign build source
  `a28944e3c14b2066ee63d14151c8aca23066d743` passed both root entrypoints.
  Independent verification matched all 13,639 SBOM files, 310 bundled articles,
  compiled catalogs, source provenance, RELEASES contents and unsigned outputs.
  Those receipts are candidate-specific and do not verify main.
- The combined redesign and reviewed local inspection route remain preserved
  at `e9dd8e0a7c0200f212cb777a4bc7d5918f2ccecc` on
  `task/upstream-development-20261006`. Local inspection has 26 passing
  non-window checks; nine built visual-review boundaries remain unverified.
- The main-only report update belongs to `task/main-build-report-20261006`.
  Its own commit identifier and verified remote reference will be supplied by
  the delivery receipt, rather than fabricated inside the commit itself.

## Next safe steps

1. Observe the current exact-main build without mutating its source, index or
   output directories. Preserve the real transcript and exit result.
2. If the final source check rejects only the proved metadata state, verify raw
   file and whole-index-tree identity, refresh only that exact file's metadata,
   prove no staged change, and retry the same root command. Do not weaken the
   source check or infer success from compilation alone.
3. On actual build success, run the exact root installer entrypoint with the
   same read-only dependency cache and an isolated output directory.
4. Independently verify every package payload byte, SBOM entry, source identity,
   version, feed row, hash and unsigned PE state. Installer execution is excluded.
5. Update this record and the linked report, integrate completed repairs into
   main and prove the remote reference. Retain incomplete redesign work.

## Boundaries and retained work

No application or browser launch, installer execution, physical printing, release
or other manual publication is authorized by the current build goal. The later
request for the complete redesign is retained; the explicit visual-verification
scope clarification remains unanswered. No new screenshots or rendered success
are claimed. Existing-host dependency reuse is not fresh-environment proof.

Use the existing isolated build-repair lane; allow at most three materially
unchanged attempts per failure and one hour of observation per external
operation. Preserve coherent work before the selected goal budget boundary.
Do not mark the goal complete from a running process, successful candidate-only
package or documentation delivery. No cleanup or release closure is claimed.
