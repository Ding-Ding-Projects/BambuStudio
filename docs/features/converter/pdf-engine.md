# Bundled PDF engine

The Windows x64 build stages the official qpdf 12.4.2 MSVC runtime into
`tools/pdf/` beside the application executable. `scripts/windows/local-pdf-tools.json`
is the trusted source manifest. It pins the release archive SHA-256, all 10 runtime
files, 74 SDK files, license text bytes, and component metadata. Runtime files total
9,113,552 bytes before notices and the manifest. No downloaded binaries are tracked.

## Build and package integration

```powershell
pwsh -NoProfile -File scripts/windows/Install-LocalPdfTools.ps1 `
  -Destination install-dir/tools/pdf -SdkDestination artifacts/pdf-sdk
pwsh -NoProfile -File scripts/windows/Install-LocalPdfTools.ps1 `
  -Destination install-dir/tools/pdf -SdkDestination artifacts/pdf-sdk -VerifyOnly
```

The parent build must invoke staging before packaging and verification against the
actual package staging directory. This script alone does not establish that an
installer contains these files. The SDK exposes `include/qpdf/qpdf-c.h` and the
MSVC import library `lib/qpdf.lib`. The runtime contains `qpdf30.dll`, `qpdf.exe`,
and the eight Microsoft runtime DLLs supplied in the official distribution.
Only the import library is staged, not the static library.

The bootstrap obtains the exact archive using `gh release download` from
`qpdf/qpdf`, tag `v12.4.2`, and verifies its recorded checksum before extracting
an explicit file allowlist. `-Offline` prohibits acquisition and requires a valid
cache; `-VerifyOnly` never downloads or executes anything. A warm invocation
verifies existing files. A mismatched existing destination fails without changing
it. New destinations are assembled in a unique sibling directory, verified, then
activated by one directory rename. Failed staging is retained for diagnosis.
No existing tree is deleted or replaced. A version change must stage a fresh tree.

## Runtime boundary

The application must resolve only its packaged `tools/pdf` directory, verify all
runtime files against trusted compiled pins, and load the exact DLL with restricted
DLL search. An adjacent self-authored manifest is not a trust anchor. Neither PATH
nor a developer installation can enable PDF tools. Build-time acquisition must not
be reachable from the runtime converter.

qpdf parses and transforms PDF structure; it does not rasterize pages, provide OCR,
validate digital signatures, or guarantee visually identical rendering. The adapter
must declare these limits, reject unsupported opaque features, and apply its own
page, byte, memory, CPU, time, and output limits. User document bytes must enter only
the declared isolated worker. Package verification is not sandbox verification.
Encrypted documents, signatures, embedded actions, attachments, and forms require
explicit adapter capability decisions before processing. qpdf warnings are not a
successful verified result.

## Licensing and provenance

The manifest embeds the unmodified Apache-2.0 `LICENSE.txt` and `NOTICE.md` from the
official qpdf source archive (SHA-256
`8a58af5b6141319287c1883bec8bd1bd545b7567b7fc5e6ce5d25a1c85f36397`).
The zlib 1.3.2#2, libjpeg-turbo 3.2.0#1, and embedded OpenSSL 3.6.4#1 notices come from the matching release's
`vcpkg.zip` (SHA-256
`82005252fee032135d07c85ad7626a38f26fb253c1e098d1d185f315bea15ebb`),
`installed/x64-windows-static/share` copyright records. These archives were used to
establish provenance; normal builds download only the runtime archive.
The component inventory includes Microsoft Visual C++ Runtime 14.51.36247.0 as
`LicenseRef-Microsoft-Visual-Studio-Redistributable`, with the official license-terms
URL. Microsoft DLL redistribution remains subject to the applicable Microsoft
license; the qpdf Apache license does not grant rights over Microsoft components.
The package includes `sbom.cdx.json`, a CycloneDX 1.6 component inventory with
runtime file SHA-256 values. It is not a claim of complete binary composition
attestation. Its component versions follow upstream package metadata.

## Verification

`pwsh -NoProfile -File tests/local_pdf_package/verify.ps1` exercises cold staging
from the verified cache, warm reuse, SDK and runtime validation, corrupt runtime,
missing notice, extra DLL, altered manifest, missing offline cache, corrupt archive,
and restored successful verification. It runs the packaged executable with only
Windows system tools on PATH against an owned content-free PDF, checks its exact
version, rotates a page, reopens the output, and verifies page count and rotation.
These checks do not read user documents and do not prove application sandbox,
installer inclusion, UI integration, or the entire PDF operation catalog.

Official behavior reference: [Running qpdf](https://qpdf.readthedocs.io/en/12.4/cli.html).
