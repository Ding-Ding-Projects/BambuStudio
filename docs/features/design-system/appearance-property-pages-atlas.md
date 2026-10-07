# Studio Atlas appearance property pages

## Scope and composition

This source-only unit changes the four property-page builders in `Appearance/AppearanceEditorPopover.cpp`. It starts from `8fb54ada88eb9d92afc13b0712d412ba3bb55aa7`. No shared catalogs, confirmation controls, regex evaluation or export dialog sources change.

- Typography: Size, Weight, Letter spacing and Line height use a persistent wrapping label above a measured horizontal value/reset row. The actual control minima remain authoritative; reset targets are retained. Italic, Underline and Strikethrough each keep their checkbox, label and reset together as one wrapping group.
- Colours: Text, Background, Highlight and Border use the same stacked label plus swatch/reset anatomy. The original color picker, chosen value, contrast calculation and reset route remain intact.
- Shape and spacing: Border width, Corner radius, Padding and Margin use stacked labels and measured value/reset rows. Numeric limits remain 12, 64, 64 and 64 respectively.
- Presets: active-name text wraps instead of ellipsizing. Apply/Save/Delete and Export/Import each use their own wrapping action group, with width supplied by the page sizer and spacing between wrapped rows. Existing order, enable rules and callbacks remain intact.
- Existing explanatory notes use body-13 supporting text. No new visible copy or controls were introduced. The font-family list/search and its existing preview remain unchanged.

The earlier independently scrolling page shell remains the overflow owner. This source work does not prove that every combination of custom font, bilingual text, live density change or very narrow viewport has the correct computed height or scroll range.

## Preset content reflow repair

Review of `c17599d086a7b5cc5c0e1f6e47fcdb48678bf1b9` found that applying or saving a longer active preset name at unchanged editor dimensions updated the label but could leave the owning page's layout and virtual extent stale. An outer `Layout()` did not guarantee a size event for that page.

`refresh_preset_list()` now explicitly reflows the Presets page after refreshing its content. A page size handler invokes the same helper when its actual client width changes. The helper protects against recursive layout notifications, lays out the page, wraps the active name at its allocated width, invalidates the page best-size cache, and updates the virtual extent with `FitInside()`. A second pass incorporates a scrollbar-induced width change. The saved scroll position is restored within the new range; no focus request is introduced. The header adds only the helper declaration, reentry flag and last measured width. Existing preset callbacks, registry operations and focus-return methods remain unchanged. No shared widget or catalog source changes.

## Preserved behavior and verification

`ui-md3/tests/appearance-property-pages-atlas.test.mjs` fingerprints the actual bound child-event call bodies and ten registry/focus/helper methods from the starting revision. It also fingerprints property declarations, numeric ranges/defaults and the color-property table. A deliberate in-memory mutation makes the font-weight callback write font size instead; the callback-preservation check must reject it. Focused geometry checks require stacked labels, retained reset rows, complete decoration groups, expanding preset wrappers and active-name wrapping. They are source contracts, not native execution tests.

```sh
node --test ui-md3/tests/appearance-property-pages-atlas.test.mjs ui-md3/tests/appearance-decimal-field.test.mjs
```

**22 checks passed**, including the three existing decimal-field checks. Four new checks execute the actual reflow statements, size-handler body and content-refresh tail through a deterministic non-window adapter. They exercise short-to-long-to-short content at unchanged page dimensions, post-scrollbar width, scroll preservation and clamping, a changed width, and recursive fit notifications. Removing the content hook fails the extent-growth assertion; removing the reentry condition fails the recursion bound. The one new presentation callback is excluded by exact body from the original callback fingerprint and is exercised separately. The adapter supplies simplified font metrics and scroll allocation; it does not execute wxWidgets or prove native layout.

No new translation keys were introduced. `git diff --check` and the new-content public-boundary scan passed. No full build, application launch, screenshot or installer execution occurred. Native compilation, visible focus, text fit, contrast and runtime behavior remain unverified.

## Nested follow-ups

- Exercise every appearance property, reset, font preview, preset protection, Save, Delete, Import and Export in the built application, including long labels and active names, English/Cantonese/bilingual, light/dark, comfortable/compact, custom fonts and 100/125/150/200% scale. Verify page scroll ranges after registry-driven changes and that wrapping does not move focus or lose an action.
- Regex diagnostics are separately owned by `build`, `buildReference`, `addSection`, `fitPopup` and `evaluate` in `RegexBuilderPopup.cpp`. The status/reference-status labels, fixed-width sample/results editors, body-11 monospaced result text and generated match/diagnostic content remain outside this unit.
- Export details are separately owned by `create_ui`, `update_format_details`, `update_archive_panel`, `update_hints` and `set_status` in `ExportDialog.cpp`. Fidelity/loss/schema details, archive notes, 7-Zip availability, cost/encryption notes, status, long checkbox copy and action groups remain separate work. Existing serialization, passwords and execution routes are not altered.
