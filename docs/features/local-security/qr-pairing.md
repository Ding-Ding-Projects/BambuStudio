# Local authenticator QR pairing

`PairingQr::encode` uses the already bundled Nayuki QR encoder, with medium error
correction and a mandatory four-module white quiet zone. It encodes the same
standard `otpauth://totp/` URI whose issuer, account, digest, digits and period the
registration service validates. Payloads are bounded to 2,048 bytes. The native
view always draws black modules on white, in both application themes, and exposes
a neutral accessible name plus a manual registration key alternative.

The lock wizard generates a fresh secret locally using OpenSSL's random source.
The QR and manual key stay hidden until the user explicitly reveals them. Hiding
the registration view removes its matrix and clears the displayed manual key.
The seed is not enrolled until a typed current code verifies. These are temporary
registration controls, not a way to reveal an already stored secret. The shared
sensitive-control registry must exclude them from captures, history, diagnostics,
logging, analytics and exports. No generated QR image is written to disk.

## Verification

The pure matrix and bundled encoder were compiled with MSVC. A separate Python
ZXing 2.3.0 decoder read a public synthetic fixture directly from an in-memory PGM
stream. The check verified scannability, all decoded enrollment parameters and
the four-module quiet zone:

```text
PASS independent ZXing decode, exact enrollment parameters and four-module quiet zone
```

This proves the encoder output, not the actual rendered native view or a physical
camera scan. Native rendering and device evidence remain outstanding.

## Exact image-import packaging hook

The repository had a bundled encoder but no identified bundled QR decoder.
`QrImport.cpp` is an explicit worker-side adapter for **ZXing-cpp 2.3.0**, using
the documented `ReadBarcodes`, `ReaderOptions`, `ImageView` and `Barcode` APIs.
Enable `SLIC3R_LOCAL_SECURITY_QR_DECODER` only after the supported dependency build
provides the exact `ZXing::ZXing` CMake target. The option fails configuration when
that exact package is absent. It never discovers a developer Python module or
PATH tool as a shipped capability.

The packaging owner must link `libslic3r_qr_import` into the existing isolated image
worker, package its complete offline runtime, and verify the installed binary.
The worker accepts normalized luminance only: positive dimensions up to 4,096 per
axis, at most 4,194,304 pixels, and an exact matching buffer length. File and
clipboard decoders must validate file bytes, frame count and decoded dimensions
inside the same isolated worker before allocation. Parent process limits must
bound CPU, memory, runtime and cancellation. Pixel buffers and decoded payloads
travel through protected local pipes, never command arguments, logs or files.
No network is permitted. Multiple QR symbols, structured-append fragments,
unsupported payloads and invalid enrollment parameters are rejected.

Only after packaged proof exists should the host populate these `Hooks` callbacks:

- `decode_qr_file(path)` for a user-selected PNG/JPEG, returning a wiping URI buffer.
- `decode_qr_clipboard()` for a user-selected clipboard image.
- `scan_qr_camera(completion)` for a local consented camera flow; completion runs on
  the wx main thread and is cancelled before the hosting surface is destroyed.

The native controls are visible but disabled with a specific capability explanation
when their callback is absent. Successful decoding fills the hidden registration
URI, preserves its parameters, and still requires a typed confirmation code.

The optional C++ decoder adapter has not compiled against ZXing in this lane and
is not a shipped or verified import capability. The test-only Python ZXing wheel
used for independent encoder verification does not satisfy packaging.

Related articles: [native integration](native-integration.md), [service overview](README.md).
