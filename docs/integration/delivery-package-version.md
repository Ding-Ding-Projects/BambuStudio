# Delivery package version

Squirrel compares package versions independently of the application version. Packaging accepts `-PreviousPackageVersion` with exactly three nonnegative numeric components supported by `System.Version`. `BAMBU_PREVIOUS_PACKAGE_VERSION` supplies the default. Invalid components or an exhausted patch component stop packaging.

The ordinary candidate remains `major.minor.(patch*1000+ReleaseNumber)`. If that candidate does not exceed the previous package version, packaging increments the previous version's third component. Product `2.8.4`, release `228`, and previous package `2.8.4813` therefore produce `2.8.4814`, rather than the decreasing `2.8.4228`.

The one-click build queries every non-draft release through authenticated `gh api --paginate`, reads matching `BambuStudioMD3-<version>-full.nupkg` asset names, and uses the maximum of the published versions and the explicit floor. This lookup runs only while preparing a package, never inside the installed application or updater. It does not pass credentials as arguments.

For reproducible offline packaging, supply the last published package version explicitly:

```powershell
./scripts/windows/Invoke-OneClickBuild.ps1 -ReleaseNumber 228 -PreviousPackageVersion 2.8.4813
```

If release discovery is unavailable and no explicit floor exists, the one-click package preparation stops rather than assuming no earlier package exists. With an explicit floor, offline preparation continues with a warning that published-version verification was unavailable. A successful lookup with no matching full packages uses the ordinary product/release candidate.

This change was implemented under the requested speed mode. No tests, lint, static analysis, runtime checks, or screenshots were run. Package generation and published update delivery remain separate evidence duties.
