# Handoff

## Current state

- Canonical repository: `https://github.com/codingmachineedge/BambuStudio.git`
- Branch: `master`
- Last synchronized remote commit before this handoff update:
  `a8bc8d24d25ec1a89c2b977148324dbdaf40424e`
- Current candidate code commit:
  `5b35200729921b4f98a91061440a4735585eb03b`
- Last fully published baseline: commit `1f1ecb960a19aaba18b619210ddca63629198a70`,
  Actions run `29671557311`, release `md3-windows-v02.08.01.55-r6.1`
- Last published installer SHA-256:
  `c66137776687585240e757ac33baa61d3693737c5aac73c145bf2d08d783a89d`
- Candidate runs `29708683379` (`0d077cd3`) and `29709187257` (`a8bc8d24d`) each passed
  the complete application build, then failed while linking `libnest2d_tests.exe` with the same 17
  missing static-library symbols. Neither run produced artifacts or a release.
- Commit `5b3520072` makes `libslic3r` own its HTTP/encryption sources, curl/OpenSSL/BCrypt link
  closure, and NanoSVG parser implementation. A new Windows candidate run is required to validate it.
- Repository immutable releases are enabled (`enabled: true`).
- Pages: `https://codingmachineedge.github.io/BambuStudio/`; deployment run `29709187022` passed,
  and live English, Cantonese-preview, and bilingual modes passed query, persistence, override, asset,
  and error-free browser checks.
- Local Windows prerequisites are installed and verified. The fresh dependency superbuild is active in
  `deps/build-local`; the application, targeted C++ tests, and isolated local launch remain to complete.

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

The candidate implements these Windows features. They are not yet represented as a passed final
release because the new link-closure commit still needs local and hosted validation:

- Three canonical UI modes: English (`en`), Hong Kong Cantonese preview (`yue_HK`), and compact
  bilingual preview (`bilingual_en_yue_HK`), with existing Bambu Studio locales retained.
- A 242-message curated native Cantonese preview catalog, full 178-key DeviceWeb and 168-key legacy
  local-web resources, Pages language persistence/query override, English fallback, safe remote-service
  routing, and Traditional CJK font/glyph selection.
- Installer language selection and first-launch registry hand-off, including silent `/LANGMODE=`
  validation and preference persistence across uninstall.
- Native language-mode/catalog C++ tests, resource/placeholder checks, an installer execution matrix,
  and a deterministic three-scenario native text/image/JSON evidence gate on disposable
  GitHub-hosted Windows runners.
- CycloneDX 1.6 per-file payload inventory, GitHub installer provenance/SBOM attestations, exactly
  three release assets, draft digest validation, stable same-run retry tags, serialized latest
  selection, pinned Actions, and a required repository immutable-release setting.
- Refreshed npm and production pnpm DeviceWeb locks with zero-advisory audit results on 2026-07-19,
  plus the route tree generated identically by both refreshed graphs. Hosted production builds also
  fail closed on high-severity pnpm audit findings.
- Removal of the unused public `StateColor::GetDarkMap()` accessor.
- Correct one- and two-item `libnest2d` test callback signatures and a self-contained static
  `libslic3r` dependency boundary, without unresolved-symbol bypasses or GUI overlinking.

The canonical `agent-global-memory` checkout is
`C:\Users\Administrator\Documents\GitHub\agent-global-memory`. Its `origin` points to the canonical
repository, `scripts/sync-agent-memory.ps1 status` reports managed targets current, and
`memory/SHARED_INSTRUCTIONS.md` supplied the active cross-agent requirements.

## Verification already established

- The `1f1ecb960` baseline compiled, installed, packaged, and published successfully in Actions run
  `29671557311`; its checksum is recorded above. It predates the new candidate gates.
- Commits `0d077cd3` and `a8bc8d24d` both completed the production Windows application build on a
  GitHub-hosted runner. Their failure is isolated to the separately configured `libnest2d_tests`
  executable link step.
- The first failure exposed incorrect callback signatures; `0d077cd3` fixed those signatures. The
  later link failure consistently reported BCrypt, OpenSSL MD5, `BBL_Encrypt`, `Slic3r::Http`, and
  NanoSVG symbols. Commit `5b3520072` assigns those implementations and link dependencies to
  `libslic3r`, the static library that consumes them.
- `scripts/ci/Test-WindowsRelease.ps1` passes locally after that fix. It validates PowerShell, JSON,
  browser JavaScript, immutable Action pins, DeviceWeb behavior, deterministic uninstall generation,
  CycloneDX generation, 242 native translations, 178 DeviceWeb translations, 168 legacy-web
  translations, and all 13 language-mode tests.
- Pages run `29709187022` is deployed and live three-mode browser QA passes with no console, page,
  request, or HTTP failures.
- Visual Studio 2026, MSVC 19.51, CMake 4.4, PowerShell 7.6.3, verified Strawberry Perl 5.42.2,
  and pkg-config-lite 0.28 are available locally. The dependency build uses an isolated prefix and
  does not modify the user's installed Bambu Studio profile.

Do not copy the baseline or failed run numbers into final release evidence. After the complete
workflow passes, record all of the following:

- final commit and clean `master`/`origin/master` equality;
- Actions run ID/URL and the successful Windows job names;
- reviewed `light-en.png`, `dark-yue_HK.png`, and `light-bilingual.png` plus their JSON evidence;
- unique release tag and latest/non-latest decision;
- installer SHA-256 and exact three release asset names;
- CycloneDX component count, version, and commit binding;
- successful `gh attestation verify` result for the downloaded installer;
- repository/release immutable state; and
- final branch, worktree, stash, wiki, and Pages audit.

## External dependencies and remaining risk

- The user authorized an ordinary local application build and isolated `--datadir` smoke launch. That
  launch is not complete yet. No candidate installer has been executed locally, and no Windows Sandbox
  session has been run.
- No low-level/headless desktop MCP is callable in this environment. The deterministic native capture
  script remains hard-restricted to a real disposable GitHub-hosted runner and must not be spoofed;
  an ordinary targeted local window smoke is separate evidence.
- Authenticode remains externally blocked on a trusted certificate/provider and secure credential
  configuration. SHA-256 checksums and GitHub attestations are not Authenticode and do not establish a
  Windows trusted publisher.
- The repository immutable-release setting is enabled. The candidate workflow still verifies it and
  fails closed if it changes before publication.
- Native Cantonese is intentionally partial. Missing copy falls back to English; broader independent
  human review is still required for print safety, destructive actions, account/privacy, networking,
  and recovery text. Formal `zh_TW` is not a Cantonese substitute.
- The native capture gate proves the compiled wxWidgets evidence surface's exact scenario text, CJK
  contract, light/dark luminance, contrast, and distinct output. It is not a golden-image comparison,
  accessibility audit, font introspection, or traversal of ordinary product screens.
- The file-level CycloneDX document inventories packaged bytes but is not a vulnerability or complete
  license analysis.

No Postman collection is applicable because this work exposes no HTTP API.
