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

## Restricted evidence reader

Use PowerShell 7 on the maintainer's Windows account with the existing
DPAPI-protected hosted GUI evidence key. The reader derives the local key path
from the checked-in public key fingerprint. It never accepts key material in
arguments, prints it, or places it in an environment variable.

```powershell
scripts/md3/Open-HostedAutomationEvidence.ps1 `
  -ReceiptPath ./download/receipt.json `
  -EnvelopePath ./download/envelope.json `
  -BundlePath ./download/evidence.aesgcm `
  -OutputDirectory ./restricted-review `
  -ExpectedRunId <run-id> `
  -ExpectedCommit <released-source-sha> `
  -ExpectedTag md3-vN `
  -ExpectedExeSha256 <verified-native-executable-sha256> `
  -ExpectedCliSha256 <verified-companion-executable-sha256>
```

The dedicated schema-v2 reader checks expected run, release, source, native and
companion identities, public-key fingerprint, ciphertext hash, authenticated
metadata and bounded ZIP inventory before extracting any entry. It rejects
undeclared, duplicate, traversing and symlink entries, verifies every length and
hash, and cross-checks the decrypted installation/runtime receipts and image
inventory. Schema-v1 automation envelopes lack the required binding and are not
accepted. Extracted `review-state.json` records integrity as verified while
pixel review, privacy review and publication remain unverified or unauthorized.
An unavailable protected local key or invalid bundle blocks extraction.
