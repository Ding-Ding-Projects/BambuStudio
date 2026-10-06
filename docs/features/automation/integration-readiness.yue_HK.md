---
translation-of: integration-readiness.md
source-sha256: 7ecbeb5dac9357adef0755669711ae1ae4c072147ed68de8a180fa864ad55bd1
review-status: agent-drafted
---

> 英文原文：[Integration readiness after the default branch advanced](integration-readiness.md)

# 預設分支推進後嘅整合準備狀態

## 已要求嘅上游同步

最近套用上游更新嘅要求，已對照 `https://github.com/bambulab/BambuStudio` 檢查；其預設分支係 `master`。擷取到嘅頂端同之後遠端讀取，都指向 `da8b44ee34dd349f2ae0df3f1cbae366df482354`。針對整合原始碼 `667ae19a1d2447b9e9b1e33d612611ec8e52b747` 同遠端 main `0c967a55786c07ef639a2cbefbe922b619c157d3`，`git merge-base --is-ancestor` 都返回 0。相應兩次 `git rev-list --count <source>..refs/remotes/upstream/master` 結果都係 0。目前上游歷史已包含在內，所以呢個同步要求唔構成產品合併或新建置嘅理由。呢點唔會滿足另外仍待完成嘅已安裝版本驗收，亦唔代表未來預設分支整合已完成。

## 範圍同原始碼身分

2026-10-03 嘅唯讀審閱比較整合原始碼 `538378cb66cb09310212594bbfa07d9c2b6d860f` 同已擷取預設分支 `0c967a55786c07ef639a2cbefbe922b619c157d3`。共同祖先係 `ce883543177ef7df46fa5b798dcc5c7f3d2f8020`。審閱期間冇實際合併或執行產品。現有發佈執行保持保留。

預設分支獨立收到以下改動：

| 提交 | 改動 |
| --- | --- |
| `6994caf6f1bf0ec0c6f41ca77f29aec8c0c19053` | 上下排列嘅 Plate Settings 標籤同全列寬下拉選單 |
| `ad910deb23247ad0f1eb1a2a3848677b4f3a8ffd` | 對話框標題追蹤同明確同步 |
| `0c967a55786c07ef639a2cbefbe922b619c157d3` | 文件同更正後計數 |

以上改動屬於另一項工作，必須保留。其正式建置執行 [37149337544](https://github.com/Ding-Ding-Projects/BambuStudio/actions/runs/37149337544) 喺觀察時仍進行中。唔會推斷結果或實際畫面行為。

## 預計衝突同所需解決方式

| 路徑 | 必須保留 |
| --- | --- |
| `CLOSEOUT_PROMPT.md` | 一份目前紀錄，同時保留兩項工作未完成嘅驗證 |
| `src/slic3r/GUI/Widgets/MD3DialogChrome.cpp` | 原有對話框顯示綁定同 `OnDialogShow`，加上閒置時標題追蹤同標題同步方法 |
| `src/slic3r/GUI/Widgets/MD3DialogChrome.hpp` | `OnDialogShow`、`FollowDialogTitle`、`SyncTitle`、進場動畫擁有權同標題追蹤成員 |

比較中 `HANDOFF.md`、`README.md` 同 `ROADMAP.md` 可以喺文字層面合併，但當前狀態聲明需要刻意協調。自動合併文字唔能夠證明語意相容或交接內容仍然有效。

## 驗收仍然綁定實際原始碼

`75770f71f59358514df9d5af42b38402e538116c` 嘅成功正式套件唔包含較新預設分支改動，包括 `OG_CustomCtrl.cpp/.hpp`、`OptionsGroup.hpp`、`Tab.cpp`、`AMSItem.cpp` 同標題實作。早期證據保留原有原始碼範圍。

最終合併原始碼需要自己嘅託管正式建置結果、相符套件同安裝後檢查。驗證標題更新時，同時檢查顯示、隱藏同重開嘅進場動畫擁有權。按適用語言、主題、正常／最小尺寸同實測顯示縮放組合驗證上下排列嘅 Plate Settings 幾何。原始碼檢查唔能夠證明實際繪畫行為。

新功能工作仍然凍結。沿用現有相符版本路徑繼續安裝後驗收，唔可以換成較舊版本或捏造建置來源。冇授權實體列印或傳送。單憑呢次審閱，冇任何項目符合刪除資格：保留、完成驗證、整合、封存讀回、擁有權同祖先證明仍然必須完成。
