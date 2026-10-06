# Per-tray reading indicators

The native Device page uses `DevAmsTray::is_reading()` for each tray's reading
animation, the same state reader used by the device web view. This changes no
controls, layout, colours, icons or Material Design 3 rendering.

The previous native condition required a mixed AMS Lite tray's reading bit
(`24 + tray`) and an unrelated ordinary AMS bit to be set together. A tray
could therefore be reading while its native indicator stayed stopped.

## Protocol mapping

| Device type | Reading-bit index |
| --- | --- |
| AMS, AMS Lite, AMS 2 Pro | `unit * 4 + tray`, four trays per unit |
| AMS HT (`N3S`) | `16 + (unit - 128)`, one tray per unit |
| Mixed AMS Lite | `24 + tray`, four trays |

The canonical reader rejects negative indices, unsupported device types,
out-of-range slots and bit indices outside the current 32-bit telemetry field
before shifting. Intermediate arithmetic is widened before multiplication.
The existing unsigned right-shift utility reads valid bits, so the native
display no longer constructs signed left-shift masks.

Upstream development commit `f10ede8a3229b4ea306ac90e2ba415c867f57fd1`
proposes multiplying every unit offset above 127 by four. That expression is
not applied: the current device model identifies AMS HT as a single-slot
device, and unit 129 must retain bit 17 rather than move to bit 20.

## Verification and limits

Run `node --test ui-md3/tests/ams-reading-state.test.mjs`. The focused test
extracts the production C++ mapping, reader and native decision bodies and
executes their arithmetic in a JavaScript model. It covers per-tray isolation,
mixed Lite's high bit alone, ordinary AMS families, HT unit 129, idle state,
invalid slots/types and the bit-31 arithmetic boundary. The same test was run
before the repair and rejected the mixed-Lite predicate and invalid shifts.

This is source-derived regression evidence, not native compilation, rendering
or physical-printer evidence. The existing `stol` telemetry parser and string
identifier parsing are unchanged. In particular, a stored bit-31 arithmetic
test does not establish that the Windows parser accepts unsigned high-bit hex
strings. Device firmware/protocol behaviour still needs hardware verification.
No printer command is sent by this indicator calculation.
