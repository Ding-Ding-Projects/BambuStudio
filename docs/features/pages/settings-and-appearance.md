# Settings and appearance

Implementation: [`ui-md3/site/settings.js`](../../../ui-md3/site/settings.js), applied by
`applyAppearance()` in [`ui-md3/site/core.js`](../../../ui-md3/site/core.js).

Every control writes straight through to the preference store and takes effect immediately; nothing
waits for an Apply button. Preferences are stored under `bambuStudio.site.v1` in the visitor's own
browser — no account, no sync, no server. When a browser blocks local storage the site still works
for the session and says so once, in a notification, instead of pretending the setting was saved.

## Groups

| Group | Settings |
|:---|:---|
| Language and tone | Language mode (a wrapping radio group), funny level for English, funny level for Cantonese, each with a live preview of a real string |
| Appearance | Theme, density, accent seed swatches, custom seed colour |
| Typography | UI font from bundled and installed faces, text size 85–140%, body weight |
| Element appearance | Per-element corner radius, spacing, text size and text colour, with a per-element reset |
| Surprises and notifications | Dim sum surprise, show notifications |
| Stored data | Reset all settings |

## Per-element appearance editors

Four surfaces can be restyled independently — the tab strip, content cards, the hero headline and
notifications. Each element's values are stored under its own key and written out as
`--el-<element>-<property>` custom properties; every use site in the stylesheet supplies its own
default, so an unset property simply falls back. "Reset this element" removes that element's saved
values and the corresponding custom properties without touching the others.

The size control stores a **unitless ratio** and only displays a percentage. It is consumed as a
bare multiplier inside `calc(44px * var(--site-font-scale) * var(--el-hero-size, 1))`, and
`calc(44px * 100%)` is invalid at computed-value time — the whole declaration is dropped, which
collapsed the hero headline to the inherited body size and persisted it. Length × percentage is not
a length.

## Typography

The font list contains the bundled Roboto and Roboto Mono plus faces confirmed present through
`document.fonts.check`. Whatever is chosen, a CJK-capable stack is appended automatically
(`Noto Sans HK`, `PingFang HK`, `Microsoft JhengHei`, `Microsoft YaHei`), because a Latin face alone
would render Cantonese copy as fallback soup. The size control scales every text style through one
`--site-font-scale` multiplier rather than restyling each rule.

## Search

The settings search is the shared regex-capable component. It filters every group by label,
description and current value in both languages, and it also indexes the controls that live on
**other tabs** — theme, density and accent on Material You; the changelog's search and date fields;
the regex lab's flags and sample. When a match is somewhere else the result says which tab it is on
and offers to go there, instead of returning nothing.

## The one blocking dialog

"Reset all settings" opens a modal `alertdialog` — the only blocking dialog on the site. That is
the rule, not an exception to it: a modal is reserved for a decision the user must make before
anything happens, and everything that merely informs is a non-blocking notification. The dialog
traps focus, restores it to the button that opened it, closes on <kbd>Escape</kbd>, and names in its
body exactly what will be deleted: language mode, both funny levels, theme, density, accent, fonts,
per-element styling, tab order, pinning and grouping — with no undo, and nothing outside this site
touched. That list survives at every funny level.
