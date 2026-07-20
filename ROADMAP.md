# Roadmap

## Completed in code

- Merge the browser concept and native C++ Material Design 3 rewrite into `master`.
- Retarget native chrome and custom widgets to semantic light/dark tokens.
- Bundle and privately register Roboto on Windows; route Cantonese/Traditional CJK labels to Microsoft
  JhengHei UI and the Traditional Chinese ImGui glyph range.
- Retheme Home, Filament Manager, and Device webviews.
- Fix the Windows `wxColour` lookup regression and remove the unused public
  `StateColor::GetDarkMap()` API.
- Produce a fixed-path, per-user NSIS installer with ownership-scoped, non-recursive uninstall and
  synchronous upgrade cleanup.
- Implement installer language choice and first-launch hand-off for `en`, `yue_HK`, and
  `bilingual_en_yue_HK` while preserving existing Bambu Studio locales.
- Add the native language-mode resolver, a curated 242-message Cantonese preview catalog, safe
  English fallback, format-before-bilingual-presentation APIs, full DeviceWeb/legacy-web resources,
  and persisted Pages language behavior.
- Add dependency-free language/resource checks, native language-mode C++ tests, installer execution
  tests, and a guarded native visual-capture script for disposable GitHub-hosted Windows runners.
- Add a per-file CycloneDX 1.6 payload inventory, installer provenance/SBOM attestations, exact
  three-asset draft validation, stable idempotent reruns, serialized publication, pinned Actions, and
  an immutable-release precondition.
- Refresh both DeviceWeb lock graphs to zero-advisory results on 2026-07-19, regenerate the shared
  route tree, and gate the pinned pnpm production graph on high-severity audit findings.
- Deploy and browser-verify the Pages landing/app in English, Cantonese preview, and bilingual mode,
  including query retention, persisted selection, URL override, canonical assets, and zero browser
  errors (run `29709187022`).
- Enable repository immutable releases and retain the workflow's fail-closed precondition.
- Fix the `libnest2d` callback signatures exposed by run `29677702628`.
- Make `libslic3r` own its HTTP/encryption sources, curl/OpenSSL/BCrypt dependency closure, and NanoSVG
  parser implementation so standalone C++ tests do not rely on GUI-library side effects
  (`5b3520072`).

## In progress

- Push `5b3520072` with this handoff update, run the complete candidate Windows workflow, and prove the
  corrected `libnest2d_tests` link and execution. Runs `29708683379` and `29709187257` already prove
  the production application build but failed at the earlier static test link boundary.
- Finish the fresh local dependency build, configure/build/install the Release application, build and
  run `libnest2d_tests` plus `language_mode_tests`, and smoke the installed GUI with a fresh isolated
  `--datadir`.
- Inspect all three hosted native capture artifacts and record final commit, run, release, checksum,
  CycloneDX, attestation, and immutable-release evidence.
- Expand the partial native Cantonese catalog beyond the curated high-value flows and obtain
  independent human review of safety-critical print, account, networking, privacy, and destructive
  copy.
- Synchronize the repository wiki and replace interim failure evidence in the handoff/release docs with
  the final successful candidate evidence.
- A low-level/headless desktop MCP is not available. Keep the guarded deterministic capture on the
  GitHub-hosted runner; Windows Sandbox remains an optional additional review and has not been run.

## Later or externally blocked

- Configure Authenticode with a trusted signing identity/provider and publish the certificate and
  rotation policy. GitHub artifact attestations are not Authenticode and do not satisfy this item.
- Add deterministic screenshot baselines, pixel-diff thresholds, OCR/glyph checks, and representative
  native screen traversal; the current three-capture smoke gate checks startup and nonblank output,
  not full visual equivalence.
- Complete native Cantonese coverage and ongoing linguistic QA; `zh_TW` must remain formal written
  Traditional Chinese and must never be used as a Cantonese substitute.
