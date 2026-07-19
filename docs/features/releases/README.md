# Release features

- [Native Windows installer and release pipeline](windows-native-installer.md)

This fork intentionally publishes a Windows installer only. Automatic upstream WinGet and Homebrew
jobs are gated to the upstream `bambulab/BambuStudio` repository so fork releases cannot mutate those
external package feeds.

No Postman collection is applicable: this category exposes no HTTP API.
