# Versioned hosted evidence recipient recovery

The from-creation startup diagnostic selects `hosted-gui-public-v3.pem` from its
exact verifier checkout. Other callers retain recipient version 2 by default.
Only recipient versions 2 and 3 are accepted by the producer, and a missing
selected PEM fails rather than falling back to another key. Recipient version
is independent of the evidence schema: schema 2, authenticated manifest, run,
product source, verifier source, executable hash and ciphertext checks are
unchanged. The envelope identifies the actual recipient by its SPKI SHA-256.

The administrative reader recognizes the archived version 1 and version 2 public
keys as well as version 3. It selects by exact SPKI identity, rejects unknown or
duplicate identities, and uses the existing current-user DPAPI custody route.
Version 1 retains its historical slot; versions 2 and 3 use distinct hash-named
slots. No old public key or protected slot is replaced or relabeled.

An explicitly authorized local administrator can create the additive recipient:

```powershell
pwsh -NoProfile -File scripts/md3/Initialize-HostedGuiEvidenceKey.ps1 -Initialize -Version 3
```

Initialization is rejected on GitHub Actions. It creates a new RSA recipient,
writes only DPAPI-protected private bytes to the current user's local custody
directory, and creates the new public PEM exclusively. It rejects existing
destinations and reparse ancestors. Private and protected buffers never appear
in output, command arguments, repository files or workflow uploads. Only the
public PEM and its public identity are reviewable source material.

Run `37100674583` transported partial encrypted diagnostic evidence after
verified worker and desktop teardown. Its matching local recipient was
unavailable, so its contents remain unopened. A new recipient cannot recover
that earlier ciphertext. A new exact-source hosted diagnostic is required after
the version 3 public key is published and independently reviewed. Successful
decryption of that new evidence still proves neither product startup success
nor a particular initializer failure until its authenticated contents are read.

No product, debugger, UI or tests were executed locally for this change. Key
initialization is an administrative custody operation only. Retain the previous
encrypted bundle unchanged and report its unavailable review state honestly.

Version 3 initialization completed successfully. Administrative readback of its
new protected slot matched the generated public identity. This establishes local
custody for the new recipient, not decryption of any earlier bundle. Independent
source review and a new hosted diagnostic remain required.
