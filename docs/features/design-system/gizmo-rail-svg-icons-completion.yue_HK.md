---
translation-of: gizmo-rail-svg-icons-completion.md
source-sha256: f9d6fdd81848836ddec1cc8a498032cffc84ab381ebeb8cd731cb2109853b17b
review-status: agent-drafted
---

> 英文原文：[Gizmo rail composite SVG completion](gizmo-rail-svg-icons-completion.md)

# 吉茲摩軌複合 SVG 完成

## 範圍

呢記錄涵蓋剩餘直接 SVG 載入名字喺 `gizmo-rail-svg-icons` 奇偶校驗警告喺提交 `8f2ba7047e385e2d28c8a64d5cd9d1b8689f507f`。

主要軌字形遷移已經存在。呢完成只替代剩餘語義複合同時保留佢哋已建立資源鍵同尺寸。

## 選擇關閉路徑

奇偶校驗警告明確識別 MD3 記號化藝術品作為一個可接受替代其中一個通用字形會移除意思或對比。呢替代係用喺呢裏因為：

- 適配攝像頭需要一個板喺 `NoBackground` 3D 場景疊加層上；
- X/Y/Z 對齐同分佈控制係一個語義矩陣、唔係一個通用圖示；
- 30×22 鍵盤提示係刻意非正方形；
- 一個本地決定向量係比擴展 GL 字型橋低風險用於四個隔離格式/提示資源。

## 記號映射

| 藝術品角色 | 淡 | 深 / 逆 |
|---|---:|---:|
| 休息板 | `SurfaceContainerLow` `#F4F2F9` | `SurfaceContainer` `#25262B` |
| 懸停板 | `SurfaceContainerHigh` `#E8E7EE` | `SurfaceContainerHighest` `#393A41` |
| 休息字形 | `OnSurfaceVariant` `#44464E` | `OnSurfaceVariant` `#CDCED8` |
| 強調字形 | `OnSurface` `#1A1B1F` | `OnSurface` `#E8E7EE` |
| 邊框 | `OutlineVariant` `#C5C6D0` | `OutlineVariant` `#4A4C54` |
| 鍵盤板 | `InverseSurface` `#2F3036` | 主題獨立 |
| 鍵盤字形 | `InverseOn` `#F1F0F7` | 主題獨立 |

視埠軸顏色係保留精確作為功能性資料顏色：X `#EA4335`、Y `#34A853`、Z `#4C8BF5`。

## 安全限制

SVG 包含只 `svg`、`rect`、`circle`、`path` 同 `polygon`。佢哋包含冇字型依賴、`<text>`、CSS、腳本、外部參考、影像、濾色器或動態顏色表達。呢保持佢哋相容帶著現有 SVG 到 OpenGL 紋理載入器。

## 驗證狀態

靜態庫存、XML、調色板、幾何、獨特性同 Inkscape 光柵化通過。原生 Windows wxWidgets/OpenGL 證據仍然需要喺改變奇偶校驗列從 `partial` 到 `done` 前。使用 `RUNTIME_SIGNOFF_BAMBUSTUDIO_MD3.md` 同倉庫嘅無頭執行技能。
