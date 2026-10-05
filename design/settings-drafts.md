# Settings draft state inventory

The implementation retains the existing section-strip and configuration-field design system. This state inventory describes the new transitions and is not runtime capture evidence.

| State | Entry | Visible content | Live settings effect |
| --- | --- | --- | --- |
| Page picker | Prepare or Preferences plus | Searchable existing pages and three draft types | None |
| Independent draft | New or duplicate | Typed fields, search, Apply, Undo Apply, Save as preset, Duplicate | None until Apply |
| Diff confirmation | Apply | Previous and proposed values for changed keys | None |
| Conflict | Target or baseline moved | Explanation retaining the draft | None |
| Applied draft | Confirmed unchanged target | Rebased draft and available Undo/Redo state | One complete transaction |
| Preset save | Save as preset | New-name input and save result | Selection unchanged |
| Protected close | Close changed draft | Explicit discard confirmation | None |
| Restored draft | Application restart | Owned snapshot and stored view state | None |

The draft action row wraps at narrow widths. Prepare expands the existing advanced sidebar width while displaying a draft. Every draft has a distinct tab identity. Runtime evidence remains required before asserting geometry, accessibility, localization, or theme conformance.
