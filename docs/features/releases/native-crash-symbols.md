# Encrypted native crash symbols

The normal Windows release workflow leaves native debug information disabled.
For a crash investigation, dispatch `build_all.yml` on the exact published candidate
branch with `debug-symbols=true` and `symbol-public-key` containing a reviewed
RSA SubjectPublicKeyInfo PEM public key of at least 3072 bits. Keep its private
counterpart in protected local storage. Never supply a private key to the workflow.

This mode keeps Release optimization, enables compiler PDB generation and `/FS`,
and bypasses application build-cache restoration and saving. Compiler launchers
are cleared and sccache is disabled for this mode, so symbol-bearing compiler
outputs cannot be written into its cache. The dependency prefix
remains reusable. Expect a full native rebuild. The workflow still produces the
normal unsigned Squirrel release; symbol mode does not add tests or lint.

The collection step reads each binary's RSDS record and each PDB's MSF identity
stream. Both `BambuStudio.dll` and `bambu-studio.exe` must have a unique PDB with
the same GUID and age. The receipt records names, binary/PDB SHA-256 hashes,
source SHA, run ID and attempt, without absolute paths. A missing or mismatched
PDB fails collection rather than claiming usable symbols.

Only the encrypted ZIP, envelope and safe receipt are uploaded as
`encrypted-native-symbols-<run-id>-<attempt>` with seven-day retention. The ZIP
contains the matching binaries, symbols and receipt. It is limited to 1 GiB,
encrypted with a random AES-256-GCM key and authenticated receipt bytes. RSA-OAEP
SHA-256 wraps that key. The envelope supplies the nonce, authentication tag,
wrapped key, exact base64 AAD, public-key hash and ciphertext hash. Raw PDBs never
enter public release attachments or application cache archives.

Download through `gh run download`, verify the ciphertext hash and public-key
identity, unwrap the key locally, and authenticate/decrypt with the envelope's
exact AAD. Reject failed authentication, receipt mismatches or differing binary
hashes. Symbols for one candidate cannot explain another binary's exact source
lines. Crash reports from earlier releases still require their original symbols.

Configure and compilation logs are retained separately on success or failure,
with hosted workspace and temporary-directory prefixes replaced by placeholders.
These logs establish build results, not runtime or crash-resolution evidence.
Symbol transport requires its own completed hosted run before it is verified.
