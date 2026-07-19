# Handoff

## Current state

- Canonical repository: `https://github.com/codingmachineedge/BambuStudio.git`
- Branch: `master`
- Last synchronized code commit before the current candidate fixes: `12c713c111d4c9667799feaa95d4f4a1f089e428`
- Final candidate commit: pending final commit and push
- Last fully published baseline: commit `1f1ecb960a19aaba18b619210ddca63629198a70`,
  Actions run `29671557311`, release `md3-windows-v02.08.01.55-r6.1`
- Last published installer SHA-256:
  `c66137776687585240e757ac33baa61d3693737c5aac73c145bf2d08d783a89d`
- Candidate Actions run/release/checksum/SBOM/attestation evidence: pending
- Pages: `https://codingmachineedge.github.io/BambuStudio/`; final three-mode deployment verification
  is pending

The current candidate is Windows-only. macOS and Linux source support remains upstream but is not part
of this fork's acceptance or release work.

## Repository and branch reconciliation

The checkout and a fresh clone were previously proved to have identical `master` commit and tree
objects. All MD3 commits were ancestors of `master`. The remaining branch audit concluded:

- `remote_branch_v13` was fully merged, proved ancestral to `origin/master`, and deleted remotely.
- `SaltWei-patch-1`, `bambu-pomfret/web-conflict`, `release/20260417`, and `remote_branch_v12` are
  patch-equivalent to changes already on `master`.
- `feature/libnoise-deps` is superseded by the fuller official `bambulab/libnoise` integration.
- `copilot/fix-ams-spinning-icon-issue` conflicts with the current N3S tray-ID model and must not be
  merged without a current device-backed reproduction and test.
- `v1.*` branches are archival 2022–2024 release lines thousands of commits behind current code.

No valid remote branch change remained to merge. The final handoff must repeat the branch, worktree,
and stash audit after all candidate commits are pushed.

## Candidate code state

The candidate implements these Windows features, but they are not represented here as a passed future
release:

- Three canonical UI modes: English (`en`), Hong Kong Cantonese preview (`yue_HK`), and compact
  bilingual (`bilingual_en_yue_HK`), with existing Bambu Studio locales retained.
- A 239-message curated native Cantonese preview catalog, full 178-key DeviceWeb and 168-key legacy
  local-web resources, Pages language persistence/query override, English fallback, safe remote-service
  routing, and Traditional CJK font/glyph selection.
- Installer language selection and first-launch registry hand-off, including silent `/LANGMODE=`
  validation and preference persistence across uninstall.
- Native language-mode C++ tests, resource/placeholder checks, an installer execution matrix, and a
  guarded three-scenario native capture test on disposable GitHub-hosted Windows runners.
- CycloneDX 1.6 per-file payload inventory, GitHub installer provenance/SBOM attestations, exactly
  three release assets, draft validation, serialized latest selection, and a required repository
  immutable-release setting.
- Removal of the unused public `StateColor::GetDarkMap()` accessor.

The canonical `agent-global-memory` checkout is
`C:\Users\Administrator\Documents\GitHub\agent-global-memory`. Its `origin` points to the canonical
repository, `scripts/sync-agent-memory.ps1 status` reports managed targets current, and
`memory/SHARED_INSTRUCTIONS.md` supplied the active cross-agent requirements.

## Verification already established

The `1f1ecb960` baseline compiled, installed, packaged, and published successfully in Actions run
`29671557311`. Its per-user NSIS archive and ownership-scoped uninstall generation were validated, and
the published installer has the checksum recorded above. That evidence predates the new language,
visual execution, SBOM, attestation, immutable-publication, and expanded installer-test gates.

Do not copy the prior run number into the candidate evidence. After the complete workflow passes,
record all of the following:

- final commit and clean `master`/`origin/master` equality;
- Actions run ID/URL and the successful Windows job names;
- reviewed `light-en.png`, `dark-yue_HK.png`, and `light-bilingual.png` artifact;
- unique release tag and latest/non-latest decision;
- installer SHA-256 and exact three release asset names;
- CycloneDX component count, version, and commit binding;
- successful `gh attestation verify` result for the downloaded installer;
- repository/release immutable state; and
- final branch, worktree, stash, wiki, and Pages audit.

## External dependencies and remaining risk

- No unsigned installer or application was executed locally for this candidate. Local Windows Sandbox
  execution remains behind explicit action-time confirmation. The automated scripts are instead
  hard-restricted to a disposable GitHub-hosted runner.
- Authenticode remains externally blocked on a trusted certificate/provider and secure credential
  configuration. SHA-256 checksums and GitHub attestations are not Authenticode and do not establish a
  Windows trusted publisher.
- The repository immutable-release setting must be enabled by an administrator and observed by the
  workflow before publication. The candidate workflow fails closed when it is disabled.
- Native Cantonese is intentionally partial. Missing copy falls back to English; broader independent
  human review is still required for print safety, destructive actions, account/privacy, networking,
  and recovery text. Formal `zh_TW` is not a Cantonese substitute.
- The native capture gate proves startup and nonblank output for three scenarios, not pixel-perfect
  theme fidelity, OCR/glyph correctness, accessibility, or every screen.
- The file-level CycloneDX document inventories packaged bytes but is not a vulnerability or complete
  license analysis.

No Postman collection is applicable because this work exposes no HTTP API.
