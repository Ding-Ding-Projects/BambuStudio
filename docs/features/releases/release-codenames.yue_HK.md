---
translation-of: release-codenames.md
source-sha256: 027d1576194100ea93493a63d1ab48ef5b9f53d44bdb0327d95d34322e12249a
review-status: agent-drafted
---

> 英文原文：[Release codenames](release-codenames.md)

# 發佈代名

每個發佈都得到一個香港菜式作為佢嘅代名：`Bambu Studio MD3 v27 Water
Chestnut Cake 馬蹄糕`。花名冊坐喺 [`.github/workflows/build_all.yml`](../../../.github/workflows/build_all.yml) 嘅發佈步驟中，同 [`scripts/ci/Test-ReleaseCodenames.ps1`](../../../scripts/ci/Test-ReleaseCodenames.ps1) 喺每個 CI 執行上將佢握住到下面嘅協議。

## 行為

發佈號係順序嘅：發佈步驟計數現存 `md3-v<N>` 標籤同採取下一個整數。代名然後係該號嘅純函數，喺三個制度中：

| 發佈範圍 | 代名形狀 | 範例 |
| --- | --- | --- |
| 1 … 217 | 一個裸菜式 | `Har Gow 蝦餃` |
| 218 … 15,624 | 風格 × 菜式 | `Golden Har Gow 黃金蝦餃` |
| 超越 | 風格 × 菜式 + 奉客計數器 | `Golden Har Gow 黃金蝦餃 2th serving 第2籠` |

**217 菜式 × 71 風格**，所以組合範圍單獨涵蓋 15,407 發佈先至奉客計數器曾經被需要。計數器然後冇約束地擴展佢。

每個項目係雙語，一個拉丁名稱、一個空間、然後粵語名稱作為最終令牌。發佈步驟喺該最後空間上分割構建組合形式，這就係形狀係一個協議而唔係慣例嘅原因。

## 配置

冇。代名被衍生，絕唔會被配置。要添加菜式或風格，**附加**到工作流程中嘅陣列同重新執行：

```powershell
.\scripts\ci\Test-ReleaseCodenames.ps1 -UpdateBaseline
```

## 失敗模式

> [!WARNING]
> **絕唔好插入任一陣列嘅中間，絕唔好重新排序或刪除。** 代名被按索引分配，所以喺位置 *i* 嘅插入重命名從 `v(i+1)` 開始嘅每個發佈。已發佈發佈係不可變嘅，所以工作流程然後會不同意無法再被更正嘅歷史。

`Test-ReleaseCodenames.ps1` 通過針對 `scripts/ci/codename-roster.json` 的陣列做偏差強制執行呢嘢，同喺任何現存索引變化時失敗。該基線只用 `-UpdateBaseline` 再生，這係一個有意行為。

測試捕獲嘅其他失敗：

- 一個冇 Han 字符嘅項目，或 Han 字符喺佢嘅拉丁半部分，發佈步驟嘅分割會產生一個格式不當嘅標題；
- 一個包含撇號嘅項目，會終止佢坐在單一引用 PowerShell 字面量；
- 重複項目，或兩個菜式分享 Han 名稱（佢哋會渲染相同，即使帶唔同英文）；
- 任何發佈號喺檢查地平線中產生重複或空代名。

## 安全考慮

冇。花名冊係靜態字面文字、分配係一個發佈號上嘅算術、同冇任何嘢讀用戶輸入或網絡內容。代名被寫到 `$GITHUB_ENV` 同到發佈標題同註釋中。

## 驗證

- `scripts/ci/Test-ReleaseCodenames.ps1`，連接到 `build_bambu.yml` 嘅 `Test Windows release inputs` 步驟。最新本地執行：**217 菜式、71 風格、15,674 唯一代名跨所有三個分配制度驗證**，僅附加檢查通過。
- 前綴保證當花名冊從 97 增長到 217 菜式時被直接檢查：所有 97 原始菜式同 40 原始風格係同一索引處位元組相同，同索引 26 仍然解析到 `Water Chestnut Cake 馬蹄糕`，匹配發佈嘅 `md3-v27`。

## 相關

- [發佈飛濺藝術](../windows/release-splash-art.md)，每發佈點心 SVG 標記、一個獨立同獨立播種菜式庫。
