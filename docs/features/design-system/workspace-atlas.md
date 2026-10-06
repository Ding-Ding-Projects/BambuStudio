# Studio Atlas workspace composition

This appearance and layout unit changes the existing native `WorkspacePanel` nested inside Project. It starts from source baseline `3c8fe2708ba03470c1b171e2fce12fcbdfaab0fc`. It does not change the Project host, shared controls, global palette, workspace engines or physical printing.

## Actual surface inventory

The panel has exactly five existing subviews, in the same order and with the same selection indices:

| Subview | Existing controls and revised composition |
| --- | --- |
| Overview | Existing title/count summary below a section heading, with separate Rename and Time zone/reminders actions in a wrapping row |
| Files | Existing three-column member/project/editable-source table inside a card, with a clear Open selected project primary action and retained add actions |
| Checklist | Existing checked list, stable Add/Edit/Move/Export actions, section heading and wrapping action footer |
| Notes | Existing `TextArea` and the same inner `wxTextCtrl`, with a persistent Notes heading, readable body font and a useful minimum editor height |
| Calendar | The same month selector and four-column agenda, beside each other when measured width permits and stacked otherwise, with all reminder/planning/export actions retained |

There is no History subview in this panel. `save_member` continues to accept and preserve a history-bearing 3MF through its existing staging and atomic bundle-save route. This unit does not invent a history browser or imply that the separate project-history surface has been redesigned.

## Layout and anatomy

Each real subview is now a keyboard-navigation-enabled native card inside a vertically scrollable page. The existing book and tab indices remain unchanged. Cards use the shared lowest-container role and outline-variant boundary, density-aware shape and spacing, and a 16-point-role section heading. Body text uses the shared comfortable/compact font roles.

The global New/Open/Save row and the overview, file, checklist and calendar action rows use native wrapping sizers. Save, Open selected project, Add checklist item and Add planned print receive primary emphasis; other actions retain tonal or outlined treatment. Labels and actual actions are unchanged. No action is hidden or moved to an invented menu.

Tables and the checklist keep a useful minimum body height; the outer page can scroll when long localized labels or wrapped action rows require more height. Existing inner table, list and text scrolling remain intact. The calendar breakpoint combines the actual month-control best width, current spacing and a 360-DIP agenda reading region. Reflow changes only layout orientation and margins, never selected dates or calendar data.

Size events are ignored until all five control trees have been created. Reflow also has a recursion guard. Theme and DPI notifications refresh local role colors, fonts, spacing, action sizing and tab metrics; no user data or draft text is reloaded by those callbacks. These source paths still need real event-loop, font, DPI and live-density verification.

## Preserved behavior

All thirty pre-existing behavior/lifetime method bodies are unchanged from the pinned baseline, including bundle open/save, staging ownership, member IDs, selected-member handoff, history-bearing member save/recovery, checklist edits and ordering, notes refresh, calendar dates and timezone handling, reminders, snooze/dismiss/enable, and JSON/CSV/ICS exports.

Every existing event binding and callback body is retained, including notes' immediate draft update and dirty flag, checklist completion, row activation, month selection and tab selection. The file and agenda column declarations, order and selection styles are unchanged. Every visible string reuses an existing translation key. The layout unit adds no new engine, file operation, persistence route, timer or printer action.

## Focused verification

Run:

```text
node --test tests/workspace/workspace_panel_atlas.test.mjs
```

The six source-preservation tests compare behavior and event boundaries against the pinned baseline, verify the five page identities and existing dispatch targets/table columns, reject user-data/date/draft mutation in visual lifecycle methods, and reject new translation keys. Deliberate negative fixtures alter the notes callback and inject a draft mutation into reflow; both must be rejected.

For a real failing test run, set `WORKSPACE_ATLAS_MUTATE_CALLBACK=1` for that command only. It changes an in-memory fixture, not the source file. The callback-preservation test must fail; clearing the override restores the passing run.

These checks do not establish native compilation, event-loop lifetime, keyboard traversal through the new containers, screen-reader output, text fit, actual scrolling, motion, or visual parity. No application, installer or printer was launched and no screenshot was captured for this unit. The complete normal/minimum size, language, theme, density and display-scale matrix remains pending.

## Remaining subview boundaries

- Existing text-entry and file-picker dialogs remain in their current controls. Guided input redesign, native date/time pickers and their validation are separate work.
- The shared `ListBox` still owns its row rendering, including its existing long-label shortening. This source unit does not claim that every checklist label is fully visible in the built application.
- `TextTabbar` still owns tab overflow, keyboard and rendering behavior. Wrapping action rows does not establish tab-strip correctness.
- Per-subview local search/regex, rich row controls, additional history navigation and any other missing canonical capabilities retain their existing incomplete inventory state. No new search engine or inert placeholder control is introduced here.
- The separate history browser, Project host, embedded project content and other canonical tools remain independently owned surfaces.

## Reversal

Keep this unit separate from functional workspace or printer changes. A reviewed reversal of its source commit restores the prior panel composition while retaining all preceding navigation, palette, Print, AMS and build work. Inspect later callers and documentation before reversing. No settings migration, workspace reset, bundle rewrite or automatic rollback is part of this change.
