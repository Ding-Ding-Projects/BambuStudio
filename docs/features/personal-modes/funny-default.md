# Funny-level defaults and existing preferences

English and Cantonese independently default to level 5. A missing or invalid stored
value uses that compiled default. Resetting a slider uses the same default.

Existing explicit choices remain unchanged, including an explicitly stored level 2.
No migration rewrites a valid stored value merely because it equals the old default:
the application cannot distinguish that value from a deliberate user choice. Values
outside 1 through 5 retain the existing clamping behavior. Explicit caller-provided
fallback values also remain authoritative.

`tests/personal_modes/funny_default_contract.mjs` compiles the actual production
constants and parsing-function bodies under MSVC. It checks missing/invalid values,
every explicit level, surrounding whitespace, out-of-range input, and an explicit
fallback, then changes the default back to 2 in an isolated copy, requires a failing
verdict, and restores the passing source. The wxWidgets language and dim-sum tests
also assert the canonical default directly rather than comparing only to another
potentially wrong constant.
