---
translation-of: dockable-sidebar.md
source-sha256: d59987c8960509fcfaef61d7d6946a78a0d4a16f2beaf7aae8fa165b488ea22b
review-status: agent-drafted
---

> 英文原文：[Dockable Prepare sidebar](dockable-sidebar.md)

# 可停靠準備邊欄

## 行為

準備邊欄（打印機、床同墨水控制）可以停靠到工作區嘅任何邊：左、右、上或下。停靠係由 `wxAuiManager` 驅動。用戶從偏好設定中嘅「準備面板位置」控制改變邊同邊欄重新停靠實時，冇重啟。已儲存邊係權威超過一個恢復佈局透視，所以右、頂或底部偏好活過一個佈局重置、重啟同 DPI 改變。

當邊欄停靠到頂部或底部時佢變成一個全寬度水平帶寬其高度係接近 40% 工作區（帶著一個樓周圍 260 px）同 3D 畫布採取剩餘豎直空間；邊欄保持佢自己內部滾動。左同右停靠保持豎直邊欄佈局。現有摺疊行為同可浮動邊欄動力用戶選項被保留。

## 配置

- 應用設定鍵：`prepare_sidebar_dock`、其中一個 `left`、`right`、`top`、`bottom`。
- 預設：`left`。呢個刻意覆蓋設計工具組件嘅右放置，根據要求嘅預設。
- 用戶控制：偏好設定 → 「準備面板位置」（「停靠準備面板喺工作區嘅左、右、頂或底。」）。

## 失敗模式

- 一個未設定或無效 `prepare_sidebar_dock` 值係正規化到 `left` 喺設定載入同又一次當停靠被應用。
- 一個先前浮動邊欄係尊重除非用戶明確挑選一個邊（或可浮動選項係禁用）；一個明確邊挑選重新停靠浮動窗格。
- 當停靠方向翻轉（豎直到水平或背面），窗格嘅最佳尺寸係重新播種喺正確軸；否則已保持尺寸係保持所以一個用戶調整大小邊欄活過重啟同 DPI 改變。

## 驗證

- 實現喺 `src/libslic3r/AppConfig.cpp`（預設同驗證）、`src/slic3r/GUI/Plater.cpp`（`apply_sidebar_dock`）、`src/slic3r/GUI/Plater.hpp` 同 `src/slic3r/GUI/Preferences.cpp`（控制）。
- 喺發行版配置本地構建。呢功能係部分本地提交 `8d727d49d`，喺本文件寫作時仍未推到託管 CI，所以冇託管執行證據用於佢。
