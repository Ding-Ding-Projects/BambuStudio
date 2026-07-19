# Handoff

## Current state

- Canonical repository: `https://github.com/codingmachineedge/BambuStudio.git`
- Branch: `master`
- Native MD3 implementation baseline: `e0f6b2f92`
- Windows CI evidence: run `29665576610`, Windows job successful
- Pages: `https://codingmachineedge.github.io/BambuStudio/`

The current checkout and a fresh clone have identical `master` commit and tree objects. All MD3
commits are ancestors of `master`.

## Remote branch reconciliation

- `remote_branch_v13` was fully merged, proved ancestral to `origin/master`, and deleted remotely.
- `SaltWei-patch-1`, `bambu-pomfret/web-conflict`, `release/20260417`, and `remote_branch_v12` are
  patch-equivalent to changes already on `master`.
- `feature/libnoise-deps` is superseded by the fuller official `bambulab/libnoise` integration.
- `copilot/fix-ams-spinning-icon-issue` conflicts with the current N3S tray-ID model and must not be
  merged without a current device-backed reproduction and test.
- `v1.*` branches are archival 2022–2024 release lines thousands of commits behind current code.

No valid remote branch change remains to merge.

## Verification evidence

- `master` and `origin/master` matched at `e0f6b2f92` before the release-automation work.
- Windows compiled, installed, packaged, and uploaded the portable artifact successfully.
- The portable artifact contains the executable, sibling DLLs, resources, and Roboto Regular/Medium/
  Bold.
- The native NSIS definition compiled locally from that artifact; archive validation covered 6,742
  entries with no errors.
- The hardened ownership generator produced 7,016 live destination reparse checks, 6,740 total
  explicit file deletions, and 276 non-recursive directory removals. The generated script contains no
  recursive removal command.
- Release tags include the workflow run number and attempt. Only the current `master` tip is marked
  latest; superseded and non-default refs remain non-latest. Build jobs have read-only repository
  permission and receive no inherited secrets.

## External dependencies and remaining work

- Interactive execution of the unsigned build requires action-time confirmation. Use a disposable
  Windows Sandbox with networking disabled, a read-only payload mapping, and a sandbox-local data
  directory.
- Full compliance with the shared three-mode language requirement is not yet implemented. The
  existing `zh_TW` catalog is formal written Chinese converted from `zh_CN`, not Hong Kong Cantonese.
  Native gettext alone contains 5,579 messages, and completion also requires embedded-web resources,
  bilingual progressive disclosure, CJK font validation, and human review of safety-critical copy.
- The global-memory checkout at `Documents/GitHub/agent-global-memory` supplied
  `memory/SHARED_INSTRUCTIONS.md`, but its managed bootstrap could not be synchronized because that
  folder has no `origin` remote and no `scripts/sync-agent-memory.ps1`.
