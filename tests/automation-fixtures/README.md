# Packaged automation fixture

`cube.stl` is a public, author-created 10 mm cube. It contains only geometry,
no user project, account, network address, machine path or identifying metadata.
The hosted verifier copies it into a new workspace and creates a new profile.
Printer mutations are never exercised. Real printer hardware remains unverified.

The manually dispatched runtime workflow requires an existing `md3-vN` release
and its exact source commit. It checks the installed companion against the
digest-verified Squirrel package, then drives native operations on a named hidden
desktop. Missing presets or an incomplete slice fail verification rather than
becoming a passing skip. Screenshots and detailed diagnostics are encrypted
with the existing restricted-review public key before upload. Pixel review is
still required before any screenshot may be published.

The runner bootstraps Python 3.12, the pinned headless tool commit
`e6e42f2066d539256d6480401d7cef867f2b8dfe`, and Pillow 11.3.0 into job-local
directories. PowerShell 7, Git and GitHub CLI come from `windows-2025` and missing
tools fail bootstrap. The installed automation executable is self-contained;
verification does not install a .NET runtime or send real printer commands.
