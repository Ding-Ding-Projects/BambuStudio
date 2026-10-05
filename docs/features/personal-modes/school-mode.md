# Shared presentation mode

The proposed interoperable version 1 record lives at
`%LOCALAPPDATA%/DingDing/SharedPresentation/school-mode-v1.json`. It is shared among
applications for the same Windows user. This is an interface convenience lock, not
a security boundary. Deleting this shared record intentionally resets the mode to
off. The credential remains in the operating-system vault and is never serialized
into this record.

The JSON document has exactly six fields, a maximum encoded size of 4096 bytes,
and no nested containers or duplicate keys:

| Field | Type and constraint |
| --- | --- |
| `version` | Integer, exactly `1` |
| `enabled` | Boolean |
| `displayName` | Valid UTF-8, 1 to 128 bytes, visible content, no control or directional override characters |
| `revision` | Unsigned 64-bit monotonic revision, incremented on a successful write |
| `credentialGeneration` | Opaque printable ASCII vault-generation identity, at most 128 bytes, required when enabled |
| `updatedAt` | UTC timestamp in `YYYY-MM-DDTHH:MM:SSZ` form |

`SchoolStore` takes an exclusive handle on the adjacent `school-mode-v1.lock`,
reloads the record, compares the expected revision, and writes a uniquely named
temporary file before atomic replacement. A competing writer receives `Conflict`;
it must reload rather than overwrite. Disabling additionally verifies the exact
current credential generation while the file lock remains held. The adapter must
query and verify `LocalSecurity::shared_mode_account`, compare its generation before
and after verification, and keep credential replacement coordinated with this same
shared-record transaction. PIN/password material remains in `LocalSecurity::Secret`.
Passkey enrollment is not implemented by these services and must not be presented
as available without a genuine platform adapter.

`SchoolRuntime` watches the shared directory and polls every 500 ms as a bounded
fallback. Polling detects record creation, deletion, rename and credential-generation
changes even after a watch error. The owner exposes `native_watch_available()` and
`polling_available()` truthfully, and exposes read/corruption status using the current
chosen display name, or generic unavailable copy if it is unknown.

Missing means intentional reset/off. An unreadable or corrupt record retains the
last valid name and fails closed to suppressed presentation. Startup is also
suppressed until a valid or missing-record verdict exists. No unavailable state
silently implies that the mode is off.

`effective(base)` preserves the stored preference object and returns an English,
serious presentation with personal replacements and surprise content suppressed.
Leaving the mode restores the unchanged base preferences. `available(capability)`
is the shared decision for controls, search, palette routes and direct activation.
Applications must rebuild those surfaces; hiding a control alone is insufficient.
The mode control itself uses only `record().display_name`, including accessibility,
search text and descriptions. Attention accommodations remain independently usable.
