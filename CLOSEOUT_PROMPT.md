# Reliability repair candidate: 5 October 2026

This task repairs Prepare sidebar transitions and evidenced native lifetime defects. It does not resume the other paused responsive-workflow features. The preceding broader handoff remains available at commit `d851ff9a7f80b92eecc09d31d04eccd941c4df75`.

## Current state

- Main remains `d851ff9a7f80b92eecc09d31d04eccd941c4df75`; no task push, hosted build or release has occurred.
- Integration branch: `fix/ui-reliability-integration-20261004`.
- Integrated source commits: `90bc7d76b` (crash diagnostics), `98202d190` and `ed85e4993` (native lifetimes and slot mapping), `770841b88` and `13ddb59cf` (scroll ownership, keyboard and header sizing).
- Documentation commits: `aa8467f3a` and `a684b1285`.
- Nine symbol-workflow files are reviewed but uncommitted because automatic approval review rejected their combined staging/commit command before execution with only `blocked by policy`. Do not claim those files are committed or published.
- Current user constraint: builds and packaging run only on GitHub Actions. No new local compilation or bootstrap is authorized.

## Evidence and limits

The installed package `BambuStudioMD3-2.8.4811-full.nupkg` names source `0c967a55786c07ef639a2cbefbe922b619c157d3`. Its SHA-1 matches the local RELEASES index: `a5bfe3e5b4539b726324e7ced90144aac4f5b993`. Both installed executable and DLL match package bytes. DLL SHA-256: `fab270799da1026d37e118495b26f7ba59117a9562401a155d5bce6c68852a70`.

Two October 4 crash reports record access violations and lower address bits `0xffffffff`. The old reporter truncated addresses, so neither the complete fault address nor its source function is established. The new production stack-walker regression retained five frames without line symbols, preserved full-width addresses and resumed the inspected thread. Original matching sources failed the negative regression. These local native checks ran before the user prohibited local builds.

Earlier native lifetime tests passed 12 cases / 23 assertions, with a stale-generation negative failure. Latest integrated non-building checks pass 11 lifetime cases and 12 scroll/width cases. Three additional native Edit cases are uncompiled. Independent source review accepted and rechecked repairs for configuration-slot mapping and keyboard routing.

An installed-build capture and native probe measured TabPrint allocated 302 pixels against a 152-pixel natural minimum. The main subtree was disabled by a possible first-run modal, so this is geometry evidence, not normal interaction proof. The later synthetic configured-profile launch exited before a usable window was observed; it produced no crash report, and the exit cause remains unproven. Do not label it a reproduced application crash.

Symbol transport has source review plus synthetic identity/encryption tests, not hosted verification. The build workflow uses opt-in `debug-symbols` and a public RSA key input, disables both application and compiler caches in that mode, matches DLL/executable PDB identities and uploads encrypted symbols only. No private key was generated or supplied yet.

## Remaining work

1. Restore GitHub CLI authentication. The device flow expired without approval; no current valid device code is recorded. A separate non-interactive Git Credential Manager preservation push was attempted and failed before transfer: `fatal: Cannot prompt because user interactivity has been disabled.` followed by `fatal: unable to get password from user`. Remote main still resolves to `d851ff9a7f80b92eecc09d31d04eccd941c4df75`; the task branch has no remote ref.
2. Resolve the specifically rejected symbol-workflow commit operation without bypassing the approval boundary.
3. Verify translation metadata, scan public-bound changes, preserve the candidate remotely and prove the remote ref.
4. Dispatch the exact candidate through the existing Windows build/release workflow with encrypted symbols; collect its actual verdict and matching outputs.
5. Verify the downloaded candidate using a configured synthetic profile on the cheap hidden-desktop route. Exercise wheel/keyboard/search/focus, last-row access, repeated Ink/Process/Objects transitions, popup/preset cancellation and interleaved physical/mixed filament rows.
6. Complete supported language/theme/scale and stability coverage; report unavailable tuples rather than infer them. Establish original crash reproduction or retain it explicitly unresolved.
7. Update documentation with real runtime evidence, integrate verified work into main, push and prove main, verify release/install behavior, and perform only authorized task-owned cleanup with preservation and archive proofs.

The goal remains active and incomplete. Existing paused branches, unrelated worktrees, user profiles and physical printers remain untouched.
