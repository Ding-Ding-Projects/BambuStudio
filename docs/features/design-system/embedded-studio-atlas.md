# Embedded Studio Atlas styling

This visual-only change applies the Studio Atlas porcelain/slate neutral palette
to product-owned embedded views. It follows the source design specification
`669af08fe25582d75a4775389eb53760fea39481` and the native palette revision
`b748affe0f687f6cfbe6068a82d32988047f790a`, including the corrected light outline
`#6d7e94`. The standalone website and third-party assets are excluded.

## Scope and preserved behavior

The DeviceWeb filament view, home, setup guide, device connection, project/model
views, login, filament manager and custom-filament styles use matching neutral
surfaces and supporting text. Existing semantic tokens are retained. Accent,
selection, warning, error and material/spool colors keep their existing values.
The original theme selectors and host-injected dark stylesheets are unchanged.
This does not introduce an OS-driven theme override to views controlled by the app.

The source changes contain no HTML, JavaScript, React, bridge, callback, data-store,
device-protocol or localization changes. Existing widths, heights, padding,
overflow, visibility, transforms, typography metrics and input geometry remain
unchanged. These constraints preserve the existing layout contract; they do not
prove that every existing layout is correct.

Controls receive a visible focus outline without moving their layout box.
New transitions affect only background and border color for 100 milliseconds.
They run only when reduced motion is not requested; reduced motion removes these
control transitions. Existing activity indicators and independent component
animations are outside this paint-only change and require their own verification.

## Verification and limitations

Run `node --test tests/embedded_atlas_styles.test.mjs` from the repository root.
The focused source checks compare all 23 changed stylesheets with the pinned
pre-refresh source, preserve original selectors and non-color declarations,
preserve accent/status variables, reject a deliberate geometry-change fixture,
and check default text and field-outline contrast numerically. The baseline
commit must be available locally for the history-based preservation check.

These checks do not exercise the CSS cascade in the embedded engine, user-defined
colors, forced-colors mode, keyboard interaction, translations, responsive layout,
motion timing or rendering. The default-palette contrast calculation does not
establish contrast for every composited state or customized color pair.

No application, browser, installer or printer was launched for this unit. No
screenshots or layout receipts were produced. Material Designer's live workflow
was unavailable under the task's explicit no-launch boundary; the checked-in
design is a source reference, not runtime evidence. The parent integration lane
owns production compilation and the later authorized runtime matrix.

## Reversal

Keep this unit in its own visual-only commit. Revert that commit to restore the
prior embedded palette without removing unrelated behavior, build repairs or
the native workflow work. No settings migration or user-data reset is involved.
