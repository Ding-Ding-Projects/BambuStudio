# Changelog completeness

The viewer keeps the existing date calendar, typed date validation, composed text/regex search, language-aware copy and full commit links. Export snapshots the same filtered Markdown text through the common export dialog. The snapshot includes range, search, version and change counts, dates and full commit references. Format loss descriptions belong to the shared exporter.

The exporter checks every referenced commit locally and paginates published releases through the existing release builder. A release-service failure now fails the freshness command; it never reports a successful freshness check from unavailable evidence.

Build integration must run `node scripts/changelog/export-app-changelog.mjs --check` before packaging, after release tags are fetched. This requires authenticated `gh` access. Refresh through `node scripts/changelog/export-app-changelog.mjs`. `--offline --check` verifies only the checked-in site snapshot and does not establish current released-version coverage. Build files are owned by the integration lane and are not changed here.

Verification: Node tests exercise malformed dates, invalid/absent commits, an empty factual release, fail-closed source wiring and the shared viewer export connection. Built calendar, copy, export and editor interactions remain unverified until the wxWidgets build is available.
