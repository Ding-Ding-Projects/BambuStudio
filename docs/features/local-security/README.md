# Local security components

These components implement local credential storage and verification, standard
TOTP, the six ordered toy-lock policies, bounded unlock sessions, attempt budgets,
and authenticated snapshot encryption. They are service components, not evidence
that every application control is connected or that the complete feature has shipped.

## Storage and identity

Windows Credential Manager stores opaque values under
`DingDing.LocalSecurity.v1/<stable-account>`, with local-machine persistence for
the current Windows user. Display names never form account keys. A missing or
unavailable vault produces an explicit failure; there is no plaintext fallback.
Other platforms currently report the native vault unavailable.

`Credentials` stores PBKDF2-HMAC-SHA256 verifiers using 600,000 iterations,
16-byte random salts, 32-byte derived keys, and independent 16-byte random
generation identifiers. PINs accept 4 to 32 decimal digits and passwords accept
8 to 1,024 UTF-8 bytes. Replacement and reset require the current answer.
Consumers must rate-limit verification, including shared-mode verification.
Generation identifiers are safe coordination metadata and contain no answers.

The shared presentation-mode credential account is
`org.dingding.shared.school.v1`. Its owner persists only `configured` and
`generation` metadata in the shared settings record. The credential backend never
writes settings and never changes presentation mode. A record can be intentionally
reset through local application data; this is a user-experience feature, not a
security boundary against someone controlling the computer.

## Authenticator and locks

TOTP implements RFC 6238 over RFC 4226 with SHA-1, SHA-256, or SHA-512, six to
eight digits, and periods from 1 to 86,400 seconds. Default parameters are SHA-1,
six digits, and 30 seconds. URI import rejects duplicate/unknown parameters,
unsupported algorithms, malformed encodings, conflicting issuers, and HOTP URIs.
No network access is used. The returned verification step lets a caller prevent
code replay; a lock must retain its own last accepted step.

The exact policies are PIN, password, PIN plus password, password plus TOTP,
PIN plus TOTP, and password plus PIN plus TOTP. They have explicit ordered factors.
Partial factor progress lasts at most 120 seconds. Cancellation and a wrong answer
clear all partial progress. Every newly constructed session starts locked.
Unlock lasts for the current surface, a chosen 1 to 1,440 minutes, or the process
lifetime. The owner must call `leave_surface()` and intercept every protected
action, shortcut, and programmatic activation before executing it.

Five incorrect attempts start a 30-second wait. Consecutive waits double up to
900 seconds. Ordinary expiry restores five attempts. The
[unlock ladder](unlock-ladder.md) service is the only caller of `clear_wait`. It
generates and grades its own challenges against single-use nonces, can clear
three waits per rolling hour, restores the same five attempts, and never unlocks
a session. Lock and
attempt state must be persisted by the owner if restart-resistant throttling is
required; the current session model is deliberately in-memory.

## Snapshot encryption

Snapshots use OpenSSL AES-256-GCM, a fresh 96-bit random nonce, a 128-bit
authentication tag, and a stable record identifier as associated data. A key
belongs only in the operating-system vault. Ciphertext can be stored in isolated
local history; plaintext secrets, PINs, passwords, codes, QR payloads, and usable
credentials cannot. Encryption alone does not provide a history manager or an
append-only transaction. Those require the identity-history integration.

## Build and registration hooks

The integration owner adds `add_subdirectory(LocalSecurity)` to
`src/libslic3r/CMakeLists.txt`, links `libslic3r_local_security` to the GUI and
consumer targets, and adds `add_subdirectory(local_security)` to the test tree.
The target uses the existing OpenSSL package and `advapi32` on Windows.

The behavioral suite also configures independently:

```sh
cmake -S tests/local_security -B build/local-security
cmake --build build/local-security --config Release
ctest --test-dir build/local-security -C Release --output-on-failure
```

Core verification currently covers 223 behavioral assertions, including all eighteen
RFC 6238 eight-digit vectors and their six-digit truncations, the ten RFC 4226
vectors, malformed imports, vault errors, generation changes, all policies,
factor ordering, expiry, independent locks, wait-skip limits, tampering, and a
real Windows Credential Manager round trip using one random test-owned account.
No credentials or codes are printed by the test executable.

## Remaining integration evidence

Native UI registration, per-element interception, keypad parity, pairing QR,
camera/image/clipboard import, history transactions and manager, localization,
clock-skew reporting, and real rendered accessibility/layout verification are
not established by these service tests. Missing evidence remains incomplete.
No HTTP API exists, so a Postman collection is not applicable.

See [native integration and remaining evidence](native-integration.md) and
[encrypted identity history](identity-history.md) for the exact composition hooks
and the separate unverified native UI/history scope.
