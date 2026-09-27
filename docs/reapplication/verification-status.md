# Official-source candidate verification

This is a progress record, not release approval. The reported saved-project crash remains open.

## Source identities

| Role | Exact commit |
| --- | --- |
| Official `v02.08.04.57` base | `f977235e6d736c4c0b650520ac5a5b72cbfe9244` |
| Existing published feature source | `c5df6199e1a83b1c94be12e999c0b322fded8730` |
| Fresh candidate submitted for build | `87e005deda7ce118612df67ea1f0a9ab8d3849be` |
| Final scoped loader source review | `e5fe62a1ed4f6516b79d5f755f89f62dd1e94977` |
| Latest completed published-package verifier | `05d5ce36358871f3756232a2b94f867b32a75a29` |

The loader review found no further source-proven defect in the scoped rollback changes at its pinned commit. This is not runtime evidence. The required hosted cases remain in [loader-adapters.md](loader-adapters.md).

## Hosted operations

At the 2026-09-27 19:29 UTC observation:

| Operation | Run | Verified state |
| --- | --- | --- |
| Unmodified official source build | [36341716651](https://github.com/Ding-Ding-Projects/BambuStudio/actions/runs/36341716651) | In progress at the native baseline build step, source `55713f72bbe4f909fde11fb652870a79e23d4ded` |
| Fresh feature candidate build and Squirrel packaging | [36344196934](https://github.com/Ding-Ding-Projects/BambuStudio/actions/runs/36344196934) | In progress at build/package, source `87e005deda7ce118612df67ea1f0a9ab8d3849be` |
| Official vendor portable observation | [36342520688](https://github.com/Ding-Ding-Projects/BambuStudio/actions/runs/36342520688) | Owned main window survived a public-fixture open attempt; model loading and pixels were not verified |
| Published `md3-v125` diagnostic | [36343988175](https://github.com/Ding-Ding-Projects/BambuStudio/actions/runs/36343988175) | Failed GUI phase; installation, encrypted transport, and restricted report review completed |

The diagnostic candidate packaging route does not publish a release. It enables native symbols, records source and binary identities, produces an SBOM, and keeps its diagnostic package version distinct from published release numbering. These outputs are pending until the hosted build produces them.

## Published-package diagnostic

The installed package version was `2.8.4124`. Its installed executable SHA-256 matched the package executable:

```text
f430f31a60486e27debc97cf0e41926072420dbf8683cd0503c6485510d9ef03
```

The encrypted report from run `36343988175` was opened only after validating source, verifier, release, run, binary, inventory, entry hash, and authenticated-encryption bindings. It contains one behavior report and zero images. The report SHA-256 is:

```text
03ae33b0b8d34005162c63bf89e72e91a611f7d039f2f20c806c6b305ad0c775
```

Window enumeration failed because the named hidden desktop no longer existed. The driver enumerated windows before inspecting process state, so this report does not establish the launched process's exit code, a relaunch, or a native crash cause. Its teardown did not overwrite the primary diagnostic. The next driver revision must retain an owned process handle and inspect process identity before relying on desktop enumeration.

The earlier run `36342361330` also retains encrypted partial evidence, but its original protected owner key is unavailable. New envelopes use a reviewed versioned public recipient. A new key cannot decrypt older ciphertext. No private key is stored in this repository.

## Evidence limits and next checks

- No fresh native capture has passed pixel review.
- No requested feature or file-opening fix is claimed verified by this progress record.
- The original private reporter project has not been uploaded. Hosted comparison uses a public calibration fixture.
- Actual native 125%, 150%, and 200% DPI results remain unavailable. Requested scale and browser zoom are not evidence of native scale.
- Live printer, camera, and provider behavior requires the corresponding access and remains explicitly unverified.
- The localized Features guide will be published only after applicable packaged behavior is verified. The detachable camera widget remains future work.
- All compilation, tests, installation, rendering, and GUI execution are hosted. Existing runs remain running; the current user installation is outside this verification route.

The private resumable ledger keeps source-input hashes, installation receipts, encrypted evidence, restricted owner review, and failed behavior phases separate. A completed transport or installation phase never upgrades a failed behavior phase.
