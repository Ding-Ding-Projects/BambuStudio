# Personal vocabulary

Preferences > Appearance > Personal vocabulary loads a local version 1 JSON file.
The same control replaces a loaded file. Clear personal vocabulary restores the
original wording without restarting. Invalid input preserves the active mapping.

```json
{"schemaVersion": 1, "entries": {"Open": "Inspect"}}
```

The loader accepts at most 1 MiB, 2,048 entries, and 512 UTF-8 bytes per key/value.
Keys and values must be nonempty text. Duplicate keys, extra root fields,
unsupported versions, control characters, directional controls, nested values,
and unsafe object-key names are rejected. Matching is case-sensitive, longest
source first, at word boundaries, and nonrecursive. Empty entry objects are valid.

Only translated interface copy registered by the localization boundary is eligible.
The final shared label, button, native-menu and combo-popup display adapters apply
the mapping; editable input and source model values are never rewritten. URLs,
paths and format templates retain their original text. Preferences search reads
original unwrapped labels. The canvas display adapter uses the same interface.

Persistence uses the operating system's local application-data directory under
`private-display/vocabulary.json`, outside synchronized preferences. Replacement
uses a sibling temporary file and an atomic Windows rename. The selected input
path is not retained. No payload, mapped value or selected filename is logged or
sent over the network. Profile archives exclude the private cache, including when
a custom data folder encloses it. Shared labels retain their original unwrapped
text; buttons and menu models retain their original values.

## Verification

No local build, test or application execution was performed for this change.
On a configured hosted Windows build, build `personal_vocabulary_tests` and
`personal_vocabulary_probe`, then run the tests and the probe phases in order:
`load-first`, `restore-first`, `invalid`, `replace`, `restore-second`, `clear`,
`restore-empty`. Each probe invocation must be a fresh process. The probe uses
only synthetic neutral text in an isolated application-data namespace and emits
only a pass/fail result.

The native UI still requires hosted interaction evidence for load, applied wording,
restart, replace, clear and invalid input, plus original wording in exports/logs.
Exercise normal/minimum sizes, English/Cantonese/bilingual modes, light/dark and
100/125/150/200 percent scaling. The owner's private JSON must not be copied into
public hosted inputs, logs or captures; its private end-to-end verification is
separate and remains unavailable in that environment. Source coverage does not
claim all custom-drawn surfaces are verified.
