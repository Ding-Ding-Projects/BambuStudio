# Themed surface colors on StaticBox cards

How a themed card actually gets its fill, and the two traps that made cards render a **light plate
in dark mode** even though every colour handed to them was correct.

## Behavior

`StaticBox` (`src/slic3r/GUI/Widgets/StaticBox.cpp`) is the card/plate primitive behind dialogs,
banners and the custom `Button`. It paints itself in `doRender()` using two `StateColor`s:

- `border_color` — seeded in the constructor with Outline / OutlineVariant.
- `background_color` — **empty until something sets it.**

A surface applies its theme by calling `SetBackgroundColorNormal(colour)` (typically from an
`apply_theme()` that also re-seeds child `Label` backgrounds).

## Failure modes

### 1. `setColorForStates()` is an update, not an insert

```cpp
bool StateColor::setColorForStates(wxColour const &color, int states)
{
    for (...) if (statesList_[i] == states) { colors_[i] = color; return true; }
    return false;   // no entry for this state -> nothing happens
}
```

Because `background_color` starts empty, `SetBackgroundColorNormal()` was a **silent no-op on every
card that never had an explicit `SetBackgroundColor()`**. `doRender()` then took its
`background_color.count() == 0` fallback and filled with `GetBackgroundColour()` — the plain
`wxWindow` colour. `SetBackgroundColorNormal()` now inserts the normal-state colour when it is
absent, and only then falls back to updating in place.

### 2. The plain window background goes stale

`StaticBox::Create()` seeds `wxWindow`'s own background once, from the parent, via
`GetParentBackgroundColor()`. Any card constructed **before** a theme is applied therefore caches
the light surface. That colour is not cosmetic: the MSW `render()` path clears its back buffer with
`GetBackgroundColour()` before `doRender()` draws, and the default erase path uses it too.
`SyncWindowBackground()` now re-seeds it from the themed normal-state colour on every
`SetBackgroundColor` / `SetBackgroundColorNormal`.

Both fixes are in the widget, so every themed card in the app is covered — not just the surface
where the symptom was first reported.

## Security considerations

None; this is a presentation-layer colour path with no I/O, no user data and no configuration.

## Verification

Reproduced and confirmed live on the real built binary through `.claude/skills/run-bambustudio/`
(headless Mesa llvmpipe + `PrintWindow`), in dark mode, on the Version history dialog:

- **Before** — [`history-dialog-dark--before-card-fix.png`](../../screenshots/version-history/history-dialog-dark--before-card-fix.png):
  card interiors sample `#F0F0F0` while the labels on top of them correctly sample `#202127`.
- **After** — [`history-dialog-dark.png`](../../screenshots/version-history/history-dialog-dark.png):
  card interiors sample `#202127`, matching their labels; all five labels legible.

Pixel sampling (x = 400, info card) is the diagnostic that separates "the label is wrong" from
"the plate under the label is wrong" — the two look identical in a thumbnail.

> [!NOTE]
> An earlier diagnosis in `HANDOFF.md` attributed this to `Label` caching its parent background at
> construction. That is a real trap, and `apply_theme()` does re-seed label backgrounds for it, but
> it was **not** the cause of this symptom: the labels were already painting correctly.
