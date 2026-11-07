# Portable companion publication

The dedicated `lan-setup-companion.yml` workflow builds on hosted Windows 2025 through `build-installer.bat /s -Companion LanDropSetup`. The ordinary native producer remains unchanged. The root producer uses its existing verified Node bootstrap, exact runtime and builder versions, npm lockfile integrity and publisher runtime checksums.

Branch pushes affecting the companion producer and manual dispatch select an exact source commit. Tag pushes do not trigger this workflow. Each run attempt receives a unique `lan-setup-v<version>-<run-id>-<attempt>` tag. Publication creates a normal, non-draft, non-prerelease release with `--latest=false`, leaving the native accepted update channel unchanged.

The four downloadable files are the single portable executable, its SHA-256 sidecar, `build-manifest.json` and `package-proof.json`. Publication compares source/index identity, clean source state, input hashes, exact package versions, the unpacked embedded manifest and executable bytes. It then reads back the normal release, exact lightweight tag commit and all downloaded asset hashes. This does not substitute for independently inspecting the embedded manifest inside the final executable or exercising the downloaded package.

Read-only API calls have three bounded attempts. Release creation is never blindly retried or overwritten: inspect any partial result first, then use a new run attempt. Source-bound build failures, missing credentials or unavailable downloads remain failures. Publication uses the existing release-token, organization-token and run-token fallback convention without printing credentials.

This workflow runs no tests or lint. It does not launch the companion, operate Docker, change a service or install an unrelated application. The portable setup companion has an explicit packaging exception; the native application's Squirrel installer requirements remain unchanged. An absent companion installation does not authorize installing another product.

## Remaining delivery evidence

This is a prepared publication route, not a published release. First dispatch, hosted bootstrap, release readback, downloaded one-file launch and source-bound runtime acceptance remain pending. Automatic hosting prerequisites, custom credentials, folder storage and complete required-feature coverage remain incomplete. Public catalogue release naming/photo references and release line-count presentation are not implemented by this initial dedicated publisher and require follow-up before claiming complete release-contract coverage.
