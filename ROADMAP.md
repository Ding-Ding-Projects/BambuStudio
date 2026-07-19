# Roadmap

## Completed

- Merge the browser concept and native C++ Material Design 3 rewrite into `master`.
- Retarget native chrome and custom widgets to semantic light/dark tokens.
- Bundle and privately register Roboto on Windows.
- Retheme Home, Filament Manager, and Device webviews.
- Fix and verify the Windows `wxColour` lookup compile regression.
- Produce a native per-user NSIS installer from the green Windows payload.
- Generate ownership-scoped uninstall commands and validate the native installer locally.
- Configure Windows-only push/manual CI with least-privilege build permissions and one unique release
  per successful run.

## In progress

- Perform an isolated Windows Sandbox visual smoke test for light mode, dark mode, and fonts after
  execution permission is confirmed.
- Design and translate persisted English, Hong Kong Cantonese, and bilingual modes. Existing `zh_TW`
  is formal written Chinese and is not an acceptable Cantonese substitute; full native and embedded-
  web coverage requires dedicated translation and human safety review.

## Later

- Configure Authenticode signing and publish the signing policy.
- Add automated Windows screenshot comparison for representative native screens.
- Remove the now-unused public `StateColor::GetDarkMap()` API after downstream compatibility review.
