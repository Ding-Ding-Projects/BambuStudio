---
translation-of: windows-only-platform.md
source-sha256: 53142ddf38e30eb0aa122a92b426eb59abeb1a7023f162d4acec53e64af480d3
review-status: agent-drafted
---

> 英文原文：[Windows-only platform policy](windows-only-platform.md)

# Windows 專用平台政策

呢個分叉構建、發運同測試確切一個平台：**Windows x64**。macOS 同 Linux 支援被從樹中移除，而唔係保留未構建，因為未構建平台代碼靜默腐爛同邀請冇人喺呢度能夠重現或修復嘅錯誤報告。

## 哪啲被移除

| 區域 | 移除 |
| --- | --- |
| 構建腳本 | `BuildLinux.sh`、`BuildFedora.sh`、`BuildMac.sh`、`DockerBuild.sh`、`DockerEntrypoint.sh`、`DockerRun.sh`、`Dockerfile` |
| 平台樹 | `src/platform/osx/`（Info.plist、授權）、`src/platform/unix/`（`.desktop` 檔案、AppImage / Linux 圖像構建器、`fhs.hpp.in`） |
| 源 | 所有 10 個 Objective-C(++) `.mm` 檔案（Retina 幫手、Mac 暗模式、Mac IME、Mac 實例檢查、Mac 3D 滑鼠、Mac 可移除驅動器、Mac 攝像頭全屏、Mac 媒體控制、`MacUtils`、`Format/ModelIO`） |
| CMake | mac/linux 分支喺根、`src/`、`src/slic3r/`、`src/libslic3r/`；`SLIC3R_FHS` 選項同佢生成嘅標題；GTK、webkit2gtk、GStreamer、Wayland 同 DiskArbitration 配線 |
| CI | 僅 macOS Homebrew 部署工作流程，同可重複構建同 deps 工作流程中嘅每個 macOS/Ubuntu 步驟 |

## 行為

喺非 Windows 系統上配置立即失敗：

```
CMake Error: BambuStudio (this fork) builds on Windows only.
Detected CMAKE_SYSTEM_NAME='Darwin'.
```

呢嘅係有意嘅。一個部分配置，後來喺編譯深處失敗，或更糟、產生一個二進制針對未測試代碼路徑，比明確拒絕喺第一行上更難診斷。

資源目錄現在被直接解析（`<install>/resources`）
而唔係通過 `BambuStudio.cpp` 中嘅四路平台 `#ifdef` 鏈。

## 哪啲保留

`__APPLE__` 同 `__linux__` 塊交錯喺共享源檔案內部仍然存在。佢哋喺 Windows 上編譯出同帶冇執行時成本；移除佢哋觸及 ~200 檔案同冒無功能增益靜默斷裂嘅風險。如果該掃描曾經完成，佢應該係機械、逐檔案，同喺每步驟都被驗證通過完整重建。

## 驗證

Windows Release 配置同完整重建喺移除後本地執行（`build_win.bat` 樹、MSBuild `BambuStudio_app_gui.vcxproj`），同 CI 喺 `windows-latest` 上構建每推送。冇其他平台要驗證。

## 上游

跨平台構建仍然可用嚟自 [Bambu Lab 嘅上游發佈](https://github.com/bambulab/BambuStudio/releases/)。
