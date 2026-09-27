# Official-source reapplication handoff

## Current state

The candidate starts at official Bambu Studio `v02.08.04.57` commit
`f977235e6d736c4c0b650520ac5a5b72cbfe9244`, followed by an independently
owned baseline diagnostic commit and grouped feature reapplication commits.
The pinned source for the existing fork features is
`c5df6199e1a83b1c94be12e999c0b322fded8730`.

`docs/reapplication/source-manifest.csv` gives the selected path inventory and
source/official blob IDs. `docs/reapplication/loader-adapters.md` records the
project-loading changes and focused hosted regression cases. The full old-tree
patch was not used as a completion verdict. Official macOS and Linux source
files were retained. Generated translation binaries, untracked fonts, old
captures, legacy NSIS source-repair files, and old build output were excluded.

## Verification

Current exact run and source identities, the restricted diagnostic result, and
its limits are recorded in
[`docs/reapplication/verification-status.md`](docs/reapplication/verification-status.md).
The candidate submitted for hosted compilation and diagnostic Squirrel packaging
is `87e005deda7ce118612df67ea1f0a9ab8d3849be`, run `36344196934`.
The final scoped loader source review was dry at
`e5fe62a1ed4f6516b79d5f755f89f62dd1e94977`; its runtime cases remain pending.

The feature reapplication lane did not run a local build, test suite, installer,
application launch, or screen capture. Source checks identified inherited
trailing whitespace in some old embedded web and test files. A new build and
installed runtime verdict must bind to the final candidate commit. In
particular, the cause of the previous installed-release crash remains unknown.

## Next actions

1. Review the grouped commits and source manifest against the official base.
2. Build the dependency tree, embedded device page, native application, and
   unsigned Squirrel.Windows package from one pinned candidate commit.
3. Resolve build or source-review findings without dropping requested features.
4. Compare official and candidate saved-3MF opening under isolated profiles,
   then verify the rollback cases in `loader-adapters.md`.
5. Drive the requested features in the built package, capture fresh evidence,
   and update release and distribution records with verified results only.
