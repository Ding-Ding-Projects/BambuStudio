# Studio Atlas readers and overlays

## Scope and evidence boundary

This source-only pass updates eight auxiliary native surfaces at baseline `be5e1205dcdad8f372d2ba63f367dbd7977c29c4`. It does not establish native compilation, rendered geometry, interaction completion, accessibility, theme contrast, or screenshot parity. Application launch and capture were prohibited for this work unit. All existing feature-delivery obligations retain their prior evidence status.

The notification source anchor is **`NotificationCenterPanel.cpp`**, not `NotificationHistory.cpp`. The latter is a GUI-independent persistence/model implementation and remains unchanged. The implementation-scope ledger should use the panel for its visual anchor.

## Per-surface composition

| Surface | Source changes | Preserved behavior |
| --- | --- | --- |
| Appearance editor | Density-based outer inset and radius; wrapping section/reset groups; each of the four pages becomes an independently scrolling kit surface beneath the persistent navigation | Registry subscriptions, element targeting, property writes, presets, reset confirmation, keyboard section switching, focus return, anchor tracking and existing entrance motion |
| Command palette | Page heading, title/description hierarchy, wrapping row text, density-based row inset, selected accent marker, paired selected foreground/background, separate live quick-setting controls | Menu registry traversal, existing rich controls, filtering, size preference, arrow selection, geometry-based scroll visibility, execution and dismissal |
| Regex builder | Distinct low-container shell and lowest-container fields, readable engine/reference/supporting copy, section headings, density-based inset/gap, measured tab widths | Worker engine, bounds/timeouts, pattern insertion, flags, match evaluation, copy, code-help handoff and existing entrance motion |
| Notification centre | Wrapping filter, selection/paging and bulk-action groups; wrapping status and empty-state copy; density-based search separation | Model, storage, paging, selection, dismiss/export/delete callbacks, danger styling, bulk review and independent destructive confirmation |
| Local history | Reduced page-title scale, wrapping introduction and safety copy, density-based cards, wrapping bulk selection/actions | Search, compare, pin/rerun/delete search, append-only restore, export, retained-failure retry and all existing confirmation routes |
| Documentation browser | Clear title/status hierarchy, wrapping article title, density-based navigation card; bounded reading sheet with a narrow-width CSS treatment | Local bundle loading, article routing, browser fallback, links, image policy, navigation history, CSP and reduced-motion behavior |
| Changelog | Density-based release card shape/gap; stacked, wrapping date/tag metadata and separate release link; readable category labels | Date validation, calendar, filters, copy/export, release/commit destinations and content |
| Export | Persistent labels above full-width values; page/section hierarchy; density-based option/card spacing and radius; wrapping dataset title | Dataset/format selection, serializer, encoding, archive methods, passwords, validation, destination handling and export execution |

No new translation keys were introduced. Construction-time density values do not prove live reflow of an already-open surface after a density change.

## Nested inventory and remaining work

These are explicit limits, not implied coverage from the shared palette or the eight source files:

- Appearance: typography family search/list, numeric steppers, weight/alignment controls, preview, color picker, shape/spacing rows, preset list/import/export/save, global-reset confirmation, row context menus and anchored placement still require built interaction and bounds evidence. Existing fixed-width internal rows remain independently scrollable and need review at the smallest supported display.
- Palette: every rich setting, menu-derived action and disabled state needs its own registration/enable/accessibility evidence. The existing 120-result cap remains. The palette does not register every nested control merely because it can open its containing page.
- Regex: Build and Reference tabs, chip groups, pattern/literal editors, flags, sample/matches, worker diagnostics, code help and every caller-owned search field need independent interaction proof. Tab label fitting at minimum displays remains unverified.
- Notifications: live toasts, canvas notifications and their placement belong to `NotificationManager.cpp`. The notification model, bulk preview and destructive controls were not restyled by this pass.
- History: recovery banner, category/status/store/date/device filters, timeline/detail, compare, label, bulk export, restore preview and printer/draft variants remain separate proof rows. Native choices and text inputs in the filter grid still need a dedicated conversion/geometry pass.
- Documentation: tree nodes, in-article links, anchors, tables, code, images, fallback and every article-specific embedded control need individual evidence. This changes the product-owned reader, not a hosted website.
- Changelog: calendar popup day grid, filter chips, long version names, bilingual entries, links, empty states, validation and export picker need built proof.
- Export: format table, fidelity/loss notes, encoding, archive-dependent controls, compression hints, output path, password confirmation, validation and every platform file picker need built proof. Existing serializers and archive execution are untouched.
- Shared menu/dialog and canonical-tool follow-ups remain outside this source unit: `MD3Menu`, `SuperConfirmGate.cpp`, `MsgDialog.cpp`, `TabStripDialogs.cpp`, live notifications and tool implementations. A static reference is not an implementation of a missing control.

## Verification

The following existing source tests passed, 7 checks total:

```sh
node --test ui-md3/tests/appearance-decimal-field.test.mjs ui-md3/tests/changelog-date-row.test.mjs ui-md3/tests/notification-bilingual-links.test.mjs
```

A separate baseline comparison found no new `_L(...)` keys, confirmed 40 selected behavior-bearing method bodies unchanged, and confirmed `NotificationHistory.cpp` and `CommandPalette.hpp` unchanged. Referenced semantic role names were checked against the existing token header. `git diff --check` passed. These are source observations, not compilation or runtime tests.

Next verification must compile the reconciled revision through the supported entrypoint, then, when launch is authorized, inspect every listed state at normal/minimum size, English/Cantonese/bilingual, light/dark, comfortable/compact, 100/125/150/200% scale and reduced motion. Retain genuine captures and measured control/text bounds. Check keyboard focus return and scrolling, especially the appearance pages, palette multiline rows, changelog metadata, notification wrapping groups and export fields. Do not mark any rendered obligation complete from these source checks.
