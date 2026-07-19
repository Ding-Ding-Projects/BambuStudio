# Roadmap

## Completed

- Merge the browser concept and native C++ Material Design 3 rewrite into `master`.
- Retarget native chrome and custom widgets to semantic light/dark tokens.
- Bundle and privately register Roboto on Windows.
- Retheme Home, Filament Manager, and Device webviews.
- Fix and verify the Windows `wxColour` lookup compile regression.
- Produce a native per-user NSIS installer from the green Windows payload.

## In progress

- Verify the push-triggered Windows workflow creates the first uniquely tagged GitHub Release and
  serves the stable landing-page download asset.
- Perform an isolated Windows Sandbox visual smoke test for light mode, dark mode, and fonts after
  execution permission is confirmed.

## Later

- Configure Authenticode signing and publish the signing policy.
- Add automated Windows screenshot comparison for representative native screens.
- Remove the now-unused public `StateColor::GetDarkMap()` API after downstream compatibility review.
