# Release features

- [Native Windows installer](windows-native-installer.md)
- [Build from source (Windows installer)](windows-build-from-source.md)
- [One-click local Windows build and installer](windows-one-click-build.md)
- [Windows CI and release supply chain](windows-release-supply-chain.md)
- [Release codenames](release-codenames.md) — the Hong Kong dish roster every release is named from

This fork intentionally publishes a Windows installer only. Automatic upstream WinGet and Homebrew
jobs are gated to the upstream `bambulab/BambuStudio` repository so fork releases cannot mutate those
external package feeds.

No Postman collection is applicable: this category exposes no HTTP API.
