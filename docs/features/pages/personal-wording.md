# Local personal wording

Settings includes an always-visible local JSON file picker, replace control and clear control. No file is loaded by default. A valid file affects only shipped interface copy and accessible names in this visitor's browser. Entered values, URLs, source code, identifiers, release facts and external records are not rewritten. Replacements are longest-first, boundary-aware and single-pass.

The neutral import contract is `{ "schemaVersion": 1, "entries": { "original text": "replacement text" } }`. No actual personal mappings are bundled. Limits are 262,144 UTF-8 bytes, depth 3, 1,024 entries, 160 UTF-16 code units per key and 320 per replacement. Entries must be nonempty strings. Duplicate keys, unsafe object keys, unknown fields, control characters, malformed input and unknown versions are rejected before application or caching. Rejected imports preserve the last valid file.

Only the validated payload is cached in per-visitor storage under a stable identity. The original filename and path are never retained. The cache is revalidated at every load; corruption restores original copy. Parsing, cache writes and replacements use no network. Personal data is excluded from exports, diagnostics, notifications and history. Clearing removes the cache and restores original copy immediately. If storage is unavailable, a valid file works for the current visit and the status explicitly says it is not persistent. A failed clear retains the current state and gives a browser-site-data recovery instruction.

The control is part of the existing settings search and its adjacent anchored regex builder. It has localized English, Cantonese and bilingual labels and states. Its surrounding tone follows each language's funny-level preference. Full command-palette indexing and shared School-mode suppression require their respective browser-surface increments and are not claimed by this increment.

Verification: `node --test ui-md3/tests/site-wording.test.mjs` exercises empty, valid, malformed, duplicate, schema/version/bounds, non-partial application, reload, replace, clear, corrupt cache, storage failure, Unicode boundaries and no-network behavior. Browser interaction and capture evidence remain required before this feature is declared complete.

## Suggested articles

- [Settings and appearance](settings-and-appearance.md)
- [Language and tone](language-and-funny-levels.md)
- [Regex builder](regex-builder.md)
