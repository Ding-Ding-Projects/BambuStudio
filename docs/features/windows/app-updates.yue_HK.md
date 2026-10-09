---
translation-of: app-updates.md
source-sha256: f9cda573446a9f032ee4fac3aa230290b1160b5ae535d162ffb9d048635e90ed
review-status: agent-drafted
---

> 英文原文：[App updates from this fork's releases](app-updates.md)

> [!NOTE]
> 背景更新只會顯示唔阻住你嘅通知：新版本準備好就出準備好橫幅；新嘅發佈準備唔到，就每個發佈出一次失敗通知。只有你自己撳檢查更新，先會開下載對話框或者「已經係最新版本」訊息。詳情睇[安靜工作流程](quiet-workflow.md)。

# 來自呢個分叉發佈嘅應用程式更新

## 行為

- **真實來源**：`Ding-Ding-Projects/BambuStudio` 嘅 GitHub 發佈（標籤 `md3-v<N>`），經 `https://api.github.com/repos/Ding-Ding-Projects/BambuStudio/releases/latest` 讀取。應用程式更新唔再問 Bambu Lab 雲端來源，所以應用程式永遠唔會提出用上游原裝構建替換自己（以前試過：2.8.2.61 嘅提示）。
- **較新即係較遲發佈，用 UTC 計**：一個發佈嘅 `published_at` 要比 `SLIC3R_BUILD_TIME_UTC` 遲超過三個鐘，先算係較新。`SLIC3R_BUILD_TIME_UTC` 係編譯嗰陣以 `%Y-%m-%dT%H:%M:%SZ` 蓋落去嘅構建時刻。兩個時間都係 UTC，`AppUpdateCheckPolicy`（`src/slic3r/GUI/AppUpdateCheckPolicy.hpp`）用純粹嘅曆法運算將佢哋變成秒數，所以喺任何本地時區得出嘅結論都一樣。三個鐘嘅寬限涵蓋由編譯一個發佈到發佈出去之間嘅時間。程式唔比較發佈編號，所以比最新發佈更新嘅本地開發構建永遠唔會被催更新。
- **兩條路**：用 Squirrel 安裝程式裝、而 **自動更新** 開咗（預設開）嘅副本，每次檢查都會執行 Update.exe，見下一節。Update.exe 比較套件版本，所以就算時間比較唔同意，佢都會裝較新嘅發佈；來源入面係已安裝嘅版本，佢就乜都唔裝。其他所有副本（可攜式 zip、開發者構建，或者關咗呢個偏好設定嘅已安裝副本）喺手動檢查而發佈較新時顯示下載對話框；呢類副本嘅背景檢查乜都唔顯示。
- **對話框提供乜**：發佈嘅 `Setup.exe` 資產（冇列出資產就係發佈頁面）。對話框係 MD3 `UpdateVersionDialog`（`ReleaseNote.cpp`）：套件標題圖塊、捲動內容入面嘅發佈名稱同說明文字，同埋下載／跳過呢個版本／取消三粒頁腳按鈕。下載會喺預設瀏覽器打開資產。
- **跳過呢個版本** 將準確標籤存入 `app_config` 嘅 `app/skip_version`；手動檢查（說明 ▸ 檢查更新）會無視呢個跳過。已安裝副本嘅背景檢查唔會為被跳過嘅標籤執行 Update.exe。
- **測試頻道**：`check_beta_version()` 乜都唔做；呢個分叉冇測試頻道。

## 已安裝副本嘅自動更新

1. **偵測。** 已安裝副本喺 `%LOCALAPPDATA%\BambuStudioMD3\app-<version>\bambu-studio.exe`，Squirrel 嘅 `Update.exe` 喺上一層資料夾。只有當載住執行緊嘅可執行檔嘅資料夾名以 `app-` 開頭，而嗰個 `Update.exe` 又存在，副本先算已安裝。其他情況都唔行呢條路。
2. **更新。** 應用程式喺背景執行緒（永遠唔係 UI 執行緒）啟動 `Update.exe --update=https://github.com/Ding-Ding-Projects/BambuStudio/releases/latest/download`，唔開控制台視窗，等佢最多 30 分鐘。GitHub 會將呢個位址轉去最新發佈。Update.exe 讀 `RELEASES`，下載完整套件，用 `RELEASES` 記錄嘅 SHA-1 核對，再喺執行緊嗰個資料夾旁邊準備新嘅 `app-<version>` 資料夾。同一時間只會有一次更新。
3. **準備好。** 退出代碼 0 加上磁碟上有較新嘅 `app-<version>` 資料夾，先算準備好（冇嘢裝嗰陣 Update.exe 都會以 0 退出，所以亦要檢查資料夾）。無論係手動定係背景檢查，應用程式之後都會顯示一條唔阻住你嘅橫幅，一直留到你處理為止：「Bambu Studio `<tag>` 已準備好。下次開啟應用程式時會啟動。呢個分叉嘅更新冇數碼簽署。」，附兩條連結：**重新啟動以安裝更新** 同 **發佈說明**（喺瀏覽器打開嗰個發佈嘅頁面；橫幅會留低）。閂咗橫幅即係「遲啲」：新版本會喺下次開應用程式時啟動。
4. **重新啟動以安裝更新。** 主視窗經正常關閉流程關閉，所以未儲存項目嘅提示照樣出現，撳取消會令應用程式保持開住，亦唔會留低重新啟動嘅要求（每次關閉開始時會收返呢個要求，只有關閉被接受、或者由項目頁面重播時先會再交出去）。應用程式真正退出嗰陣，會唔開控制台視窗咁啟動 `Update.exe --processStartAndWait bambu-studio.exe`；Update.exe 等應用程式退出之後，就啟動最新安裝嘅版本。
5. **手動檢查。** 說明 ▸ 檢查更新行同一條路。發佈較新嘅話，佢會先顯示一個短通知「喺背景下載 Bambu Studio `<tag>`。」，等大型套件下載緊嗰陣，檢查唔會睇落好似乜都冇做。發佈唔係較新、而 Update.exe 又冇準備到嘢嘅話，檢查會顯示「已經係最新版本」訊息。
6. **運行期間。** 開咗偏好設定嘅已安裝副本每六個鐘會再檢查一次，所以開咗幾日嘅工作階段都搵到新發佈。關咗偏好設定就會停止呢啲檢查，直到下次啟動。
7. **冇準備到嘢。** 發佈較新但 Update.exe 冇準備到嘢，即係更新冇完成。手動檢查會開下載對話框。背景檢查會顯示一個唔阻住你嘅通知「Bambu Studio `<tag>` 嘅背景更新未能完成。」，附 **去發行頁面下載** 連結；通知會一直留到你閂咗佢，每個發佈只會出一次。發佈唔係較新嘅話，即係本來就冇嘢要裝，背景檢查唔會出聲。

## 呢次修正之前構建嘅副本

修正之前嘅構建將 `published_at` 同 `SLIC3R_BUILD_TIME` 比較；後者係構建主機嘅本地時間、冇時區偏移，而且係用執行應用程式嗰部電腦嘅本地時區去讀。如果構建主機用 UTC（雲端 Windows 構建就係咁），喺 UTC 以西三個鐘嘅寬限會變長（夏令時間期間喺多倫多大約變成七個鐘），所以喺已安裝構建之後唔耐發佈嘅版本唔會被提出。喺 UTC 以東寬限會跌到零以下（香港），所以副本可能會被提出佢自己嗰個發佈。已安裝副本亦都要等呢個比較話較新，先會執行 Update.exe。呢啲副本冇相容方案：請用最新發佈嘅 `Setup.exe` 重新安裝一次，之後就會用修正咗嘅檢查。

## 捷徑同安裝事件

除非套件入面有一個可執行檔標明知道 Squirrel，否則 Squirrel 會幫套件入面每個可執行檔都整一個捷徑，仲會逐個啟動。喺呢個改動之前建置嘅套件都冇呢個標記，所以正規表示式輔助程式 `bambu-regex-worker.exe` 攞咗一個用套件標題命名嘅捷徑 **Bambu Studio MD3**，撳落去啟動一個乜都唔顯示嘅輔助程式；而應用程式自己嘅捷徑就叫 **BambuStudio**，放喺開始功能表「Bambu Research」資料夾（[issue #52](https://github.com/Ding-Ding-Projects/BambuStudio/issues/52)）。

而家 `bambu-studio.exe` 嘅版本資源喺 `040904B0` 區塊（Squirrel 淨係讀呢一個）寫住 `SquirrelAwareVersion` "1"，產品名係 **Bambu Studio MD3**，公司名係 **codingmachineedge**。咁 Squirrel 就淨係為佢嘅事件啟動啟動器，啟動器處理完每個事件就退出，唔會載入應用程式任何部分：

| 事件 | 啟動器做乜 |
| --- | --- |
| `--squirrel-install` | `Update.exe --createShortcut=bambu-studio.exe --shortcut-locations=Desktop,StartMenu`，喺桌面同開始功能表「codingmachineedge」資料夾整（或者換走）`Bambu Studio MD3.lnk` |
| `--squirrel-updated` | 一樣，不過淨係做仲有呢個應用程式捷徑（新或者舊）嘅位置，所以用家刪咗嘅捷徑唔會再出現 |
| `--squirrel-uninstall` | 兩個位置都用 `--removeShortcut`，之後「codingmachineedge」開始功能表資料夾變空就一併刪走 |
| `--squirrel-obsolete` | 乜都唔做 |
| `--squirrel-firstrun` | 等 Update.exe 做完之後，將第一次啟動交俾安裝根目錄嘅 `bambu-studio.exe`（見下面）；呢個參數永遠唔會去到應用程式 |

互動安裝嘅時候，Squirrel 會趁安裝程式仲未收尾，自己用 `--squirrel-firstrun` 啟動 `app-<version>\bambu-studio.exe`，而且唔會等佢。有人報告咁樣啟動之後應用程式冇出現，但係捷徑（佢哋啟動嘅係安裝根目錄嘅 `bambu-studio.exe`）就用得。所以啟動器而家唔再自己做呢次啟動：如果佢嘅父程序係 `Update.exe`，就先等佢結束（最多 60 秒），然後用正常視窗、喺 Windows 准許嘅情況下脫離安裝程式嘅 job object，唔帶任何參數啟動安裝根目錄嘅 `bambu-studio.exe`，之後自己退出。搵唔到安裝根目錄或者 stub，又或者嗰次啟動失敗，就照舊正常啟動。每一步都會寫入 `%TEMP%\bbs-launcher-trace.log`，而家每行開頭都有本地時間同程序 ID。靜默安裝（`--silent`）乜都唔會啟動，所以從來冇一個託管安裝檢查行過呢條路：互動安裝之後應用程式係咪真係會出現，只有[安裝程式首次啟動診斷](../automation/installer-first-run-diagnostic.md)同真機安裝先驗證到，合約測試驗證唔到。

新捷徑整好之後（同埋解除安裝嗰陣），佢亦會刪走舊套件整嘅兩個捷徑：桌面同「Bambu Research」入面嘅 `BambuStudio.lnk`，但淨係喺佢哋指住呢個安裝嘅時候先刪。由舊套件第一次更新嗰陣，輔助程式嗰個指錯咗嘅 `Bambu Studio MD3.lnk` 會原地被換走，因為新捷徑同佢同名、同資料夾。

用 Squirrel 套件之前嗰個安裝程式裝嘅副本（喺 `%LOCALAPPDATA%\Programs\Bambu Studio MD3`）係另一個安裝。Squirrel 永遠唔會更新佢；請用佢自己嘅解除安裝程式移除。

## 設定

- `auto_update`（偏好設定 ▸ 一般 ▸ **自動更新**，預設開）：已安裝副本會唔會自己更新。關咗佢，每個副本都會喺手動檢查後顯示下載對話框。呢個開關對可攜式或者開發者構建冇作用，重設偏好設定會還原預設值。
- `enable_beta_version_update` 已經唔再改變行為。

## 故障模式

- 冇網絡、API 錯誤或者內容格式唔啱：自動檢查乜都唔顯示；手動檢查會顯示「已經係最新版本」提示而唔係錯誤，原因會寫入記錄。
- `published_at` 或者構建時間唔係準確嘅 `YYYY-MM-DDTHH:MM:SSZ` 格式，或者日期根本唔存在：寫入記錄，發佈唔算較新。已安裝副本照樣會執行 Update.exe。
- Update.exe 啟動唔到、以非零代碼退出、以 0 退出但冇準備到較新嘅版本，或者 30 分鐘內做唔完：每一步都會寫入記錄（以 `auto update:` 開頭嘅行）。如果發佈較新，即係更新冇完成：手動檢查會退返去下載對話框，同冇自動更新嘅副本一樣；背景檢查會顯示唔阻住你嘅失敗通知，每個發佈一次，所以每六個鐘嘅重新檢查唔會重複出。咁樣，壞咗嘅更新永遠唔會收埋新發佈。如果發佈唔係較新，冇準備到嘢就唔算失敗。自動更新會喺下次檢查時再試。等待喺 30 分鐘後放棄，但 Update.exe 本身永遠唔會被終止，因為準備到一半殺咗佢可能會留低唔完整嘅資料夾。
- 下載更新期間應用程式關閉：等待會停止，Update.exe 會留低自己做完，所以下次啟動就會開新版本。應用程式唔理會期間另外啟動嘅第二個 Update.exe（例如下次啟動嗰個）；嗰次執行報告乜，都會跟返上面嘅路線。
- 喺未儲存項目提示取消「重新啟動以安裝更新」（或者任何其他否決關閉嘅情況）：乜都唔會重新啟動，之後無關嘅退出亦唔會重新啟動。
- 重新啟動時 Update.exe 啟動唔到：應用程式照樣退出，新版本會喺下次開啟時啟動。
- Squirrel 源排序：由 md3-v106 起套件版本係 `2.8.<patch*1000+N>`（`2.8.2106`），所以 Squirrel 源更新程式排得到發佈次序；md3-v104 同 md3-v105 都係 `2.8.2-build61`，用套件版本分唔到。改用執行編號之前構建嘅套件，`N` 係構建嗰陣讀到嘅最大發佈編號加一，所以前後排隊嘅構建可能用同一個版本：md3-v155 同 md3-v156 都係 `2.8.4155`，而由嗰陣已經排緊隊嘅構建發佈出嚟嘅版本都可能再撞。源入面嘅版本同已安裝嘅一樣時，Squirrel 乜都唔裝，所以兩個咁樣嘅發佈之中後嗰個永遠唔會自己裝到；程式會好似處理任何冇準備到嘢嘅更新咁，退返去下載對話框。而家雲端構建用 Windows 構建與發佈工作流程嘅執行編號做 `N`，佢只會跟推送次序增加；而一個構建只有喺領先而家嘅最新發佈時先會成為最新發佈，所以源嘅版本一定向上走。第一個咁樣構建嘅套件會跳一次，大約去到 `2.8.4607`。

## 安全考量

- 匿名讀取公開 API；唔會傳送任何權杖。速率限制（每個 IP 每個鐘 60 個請求）遠遠高過應用程式每次啟動一次、已安裝副本每六個鐘一次，再加手動檢查嘅用量。已安裝副本每次檢查亦會畀 Update.exe 讀一個細細嘅 `RELEASES` 檔案；只有來源有較新版本時佢先會下載套件。
- 自動更新用 HTTPS 連去 GitHub 同 GitHub 轉介嘅發佈資產主機。來源位址係原始碼入面嘅常數；發佈 JSON、偏好設定或者用家輸入嘅任何嘢都唔會去到 Update.exe 嘅命令列，重新啟動嘅命令列亦係固定。發佈說明連結會開呢個分叉 `md3-v<N>` 形式標籤嘅發佈頁面；其他標籤就開發佈列表。
- 應用程式自己唔會下載或者執行任何嘢。做呢啲嘢嘅係 Squirrel 嘅 `Update.exe`，佢會用 `RELEASES` 入面嘅 SHA-1 核對套件。咁樣核對嘅係完整性，唔係作者身份：按政策套件冇簽署，所以信任建基於去 GitHub 嘅 HTTPS 同對發佈嘅控制。可攜式或者開發者副本永遠唔會執行 `Update.exe`。
- 對話框路線上嘅安裝程式按政策都冇簽署；發佈說明附有 `Setup.exe` 嘅 SHA-256，應用程式會將下載交畀瀏覽器，而唔係自己攞落嚟再執行。

## 驗證

- 政策：`tests/app_update_check_policy_test.cpp` 係 `AppUpdateCheckPolicy` 嘅獨立 C++ 斷言執行檔。佢涵蓋嚴格嘅 UTC 解析（閏日、跨年同格式唔啱嘅時間戳）、寬限邊界（啱啱三個鐘唔算較新，多一秒就算）、md3-v225 嘅情況、每個路線同結果分支，同埋喺 `UTC`、`America/Toronto` 同 `Asia/Hong_Kong` 時區下得出相同結論。構建時唔好定義 `NDEBUG`（檔案自己會取消定義），然後執行，例如 `g++ -std=c++17 -Wall -Wextra -Werror tests/app_update_check_policy_test.cpp -o aucp && ./aucp`。
- 合約：`node --test ui-md3/tests/app-auto-update.test.mjs` 固定安裝偵測、`check_new_version` 嘅路線、唔用時區 API 嘅 UTC 比較、固定來源、隱藏行程、退出代碼加資料夾嘅成功規則、結果處理（每次檢查後都有準備好橫幅、手動檢查失敗後開下載對話框、背景檢查失敗後每個發佈一個失敗通知、冇較新版本時顯示「已經係最新版本」或者唔出聲）、重新啟動交接、取消關閉規則、永遠唔會淡出嘅橫幅連兩條連結同冇簽署通知、每六個鐘重新檢查、`auto_update` 預設值同抽取咗嘅訊息。`node scripts/check-quiet-prompts.mjs` 要求更新路線入面每個下載對話框都要有明確請求先出現，而兩個背景更新通知都必須係唔阻住你嘅通知。
- 執行時（等發佈截取）：用 `Setup.exe` 裝一個舊發佈，啟動佢，確認 `auto update:` 記錄行、「已準備好」橫幅，同埋撳「重新啟動以安裝更新」之後新版本會啟動；然後關咗偏好設定、再用可攜式副本重複一次，做一次手動檢查，確認出現嘅係下載對話框。啟動一個比最新發佈更新嘅構建，確認兩條路線都冇觸發。呢個改動嘅原生編譯要等雲端 Windows 構建確認。喺託管 Windows 上，[自我更新診斷](../automation/self-update-diagnostic.md)會裝一個舊發佈，用預設設定啟動佢，再記低佢有冇自己準備好最新嗰個。

## 相關

- [Windows 原生安裝程式](../releases/windows-native-installer.md)
- [發佈代號](../releases/release-codenames.md)
