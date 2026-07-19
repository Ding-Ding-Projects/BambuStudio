# Handoff

## Current state

- Canonical repository: `https://github.com/codingmachineedge/BambuStudio.git`
- Branch: `master`
- Last pushed implementation commit before release automation: `e0f6b2f92`
- Windows CI evidence: run `29665576610`, Windows job successful
- Pages: `https://codingmachineedge.github.io/BambuStudio/`

The current checkout and a fresh clone have identical `master` commit and tree objects. All MD3
commits are ancestors of `master`.

## Remote branch reconciliation

- `remote_branch_v13` is fully merged.
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
- The native NSIS definition compiled locally from that artifact; archive validation covered 6,743
  files with no errors.

## Remaining external gates

- Push the release-automation/documentation commit and verify the Windows workflow and first release.
- Interactive execution of the unsigned build requires action-time confirmation. Use a disposable
  Windows Sandbox with networking disabled, a read-only payload mapping, and a sandbox-local data
  directory.
- The global-memory checkout at `Documents/GitHub/agent-global-memory` supplied
  `memory/SHARED_INSTRUCTIONS.md`, but its managed bootstrap could not be synchronized because that
  folder has no `origin` remote and no `scripts/sync-agent-memory.ps1`.
