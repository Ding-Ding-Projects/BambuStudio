# Release features

- [Native Windows installer](windows-native-installer.md)
- [Build from source (Windows installer)](windows-build-from-source.md)
- [Windows CI and release supply chain](windows-release-supply-chain.md)

This fork intentionally publishes a Windows installer only. Automatic upstream WinGet and Homebrew
jobs are gated to the upstream `bambulab/BambuStudio` repository so fork releases cannot mutate those
external package feeds.

No Postman collection is applicable: this category exposes no HTTP API.
