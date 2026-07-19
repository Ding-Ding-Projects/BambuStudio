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
- Add the native language-mode resolver, a curated 239-message Cantonese preview catalog, safe
  English fallback, format-before-bilingual-presentation APIs, full DeviceWeb/legacy-web resources,
  and persisted Pages language behavior.
- Add dependency-free language/resource checks, native language-mode C++ tests, installer execution
  tests, and a guarded native visual-capture script for disposable GitHub-hosted Windows runners.
- Add a per-file CycloneDX 1.6 payload inventory, installer provenance/SBOM attestations, exact
  three-asset draft validation, serialized publication, and an immutable-release precondition.

## In progress

- Run the complete candidate Windows workflow, fix any failures, inspect all three native capture
  artifacts, and record final commit/run/release/checksum/SBOM/attestation/immutability evidence.
- Enable and verify the repository immutable-release setting before the candidate release can be
  published; the workflow intentionally fails closed when it is disabled.
- Expand the partial native Cantonese catalog beyond the curated high-value flows and obtain
  independent human review of safety-critical print, account, networking, privacy, and destructive
  copy.
- Deploy and verify the three Pages language modes, then synchronize the repository wiki and handoff
  with final release evidence.
- If explicit action-time permission is provided, perform an additional local Windows Sandbox review
  with networking disabled. No unsigned installer or application has been run locally for the current
  candidate.

## Later or externally blocked

- Configure Authenticode with a trusted signing identity/provider and publish the certificate and
  rotation policy. GitHub artifact attestations are not Authenticode and do not satisfy this item.
- Add deterministic screenshot baselines, pixel-diff thresholds, OCR/glyph checks, and representative
  native screen traversal; the current three-capture smoke gate checks startup and nonblank output,
  not full visual equivalence.
- Complete native Cantonese coverage and ongoing linguistic QA; `zh_TW` must remain formal written
  Traditional Chinese and must never be used as a Cantonese substitute.
