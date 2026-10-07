---
translation-of: readme-screenshots.md
source-sha256: 035037d2e4266a4fb6446bfdcdaec93d3ba0a01179a516203e6665da3ccaa24c
review-status: agent-drafted
---

> 英文原文：[README screenshots](readme-screenshots.md)

# README 截圖

`.github/workflows/readme-screenshots.yml` 會重新影 `README.md` 入面嘅截圖，再上載畀人覆核。其他託管截圖流程全部都會將圖片加密，淨係維護者手上把鑰匙先開到。呢個流程就上載普通 PNG 檔，因為佢嘅目的就係整一套可以直接睇、直接提交嘅替換圖。所以佢一定要過咗下面講嘅私隱檢查先可以上載，而且上載檔只會保留三日。

## 點樣行

淨係可以手動觸發：喺 Actions 揀 **Retake README screenshots for review**，撳 Run workflow，或者：

```
gh workflow run readme-screenshots.yml -f target=native-app \
  -f release_tag=md3-v225 -f expected_source_commit=<md3-v225 嘅 40 位提交>
gh workflow run readme-screenshots.yml -f target=design-references \
  -f release_tag=md3-v225 -f expected_source_commit=<md3-v225 嘅 40 位提交>
```

| 輸入 | 預設 | 意思 |
| --- | --- | --- |
| `target` | `native-app` | `native-app` 操作已安裝嘅應用程式；`design-references` 渲染 Pages 應用 |
| `release_tag` | 必填 | 要安裝嘅已發佈版本，格式係 `md3-v<數字>` |
| `expected_source_commit` | 必填 | 嗰個版本準確嘅 40 位小寫原始碼提交；對唔上就唔會安裝 |
| `rows` | 空白 | 一條正規表示式，用嚟收窄原生允許清單；空白即係全部重影 |

`design-references` 工作乜都唔安裝，所以佢唔理 `release_tag`、`expected_source_commit` 同 `rows`；不過觸發表格仍然要求填頭兩個。

成個工作流程對儲存庫淨係有唯讀權限，亦唔會用任何已儲存嘅密碼。每個 action 都釘死喺其他託管工作流程用緊嘅同一個提交，而每個輸入都只會經環境變數傳入腳本。

## 原生工作

`native-app` 喺 `windows-2025` 上面行，最多 60 分鐘：

1. 用 `scripts/ci/Verify-HostedSquirrelInstall.ps1` 安裝版本，同其他託管工作流程做嘅檢查一樣：已發佈嘅摘要值、`RELEASES` 嗰行、套件規格、安裝咗嘅執行檔同埋捷徑，用預設嘅靜默安裝。今次執行自己嘅權杖淨係交俾呢一步，用嚟讀版本資料；腳本喺執行任何下載返嚟嘅嘢之前就會清走佢，所以權杖永遠唔會去到應用程式手上。
2. 喺工作專用嘅環境入面安裝釘死版本嘅無頭電腦操作工具同 Pillow，做法同 `scripts/ci/Invoke-HostedReleaseVerification.ps1` 一模一樣。
3. 先行一次私隱檢查自己嘅測試資料（見下面），檢查本身壞咗嘅話，未截圖就會停低。
4. 用 `scripts/ci/HostedDisplayMode.cs`（顯示縮放工作流程嘅顯示模式工具）將螢幕設成 1920x1080。
5. 將套件自帶嘅軟件 OpenGL 兩個檔（`mesa\opengl32.dll`、`mesa\libgallium_wgl.dll`）核對過釘死嘅 SHA-256 之後，複製去安裝好嘅執行檔隔籬。託管執行器冇 GPU，唔咁做嘅話應用程式會自己複製再重新啟動，而截圖驅動程式只會跟住佢自己開嗰個程序。
6. 準備一個資料目錄：英文、淺色、舒適密度，即係所有允許清單項目用嘅組合。目錄放喺執行器嘅暫存目錄，喺用戶設定檔以外。
7. 由檢出目錄刪走允許清單入面嘅檔案，咁舊圖就永遠冒充唔到新圖；然後用 `--kinds page,crop-probe --mesa --evidence-probe` 對揀咗嘅項目行 `scripts/md3/recapture.py`。`--evidence-probe` 會喺每張圖影完、視窗仲喺度嘅時候，多攞一份版面傾印，並喺報告嗰行寫低佢個名。

## 設計參考工作

README 嗰三張 Material 設計參考圖（`material-prepare-light-en.png`、`material-preview-dark-yue-hk.png`、`material-device-dark-bilingual.png`）係 Pages 應用嘅截圖，每張都連去佢顯示緊嗰條應用網址。之前幾次原生重影將佢哋蓋咗做原生截圖；而家 `docs/screenshots/recapture-manifest.json` 入面佢哋嘅做法寫明係 `pages`，原生流程唔會再掂佢哋。

`design-references` 喺 `ubuntu-latest` 上面行，最多 20 分鐘。佢用 `ui-md3/scripts/compose-site.mjs` 砌好 Pages 網站，用 `ui-md3/tests/serve.mjs` 喺本機迴環介面提供，再行 `ui-md3/scripts/capture-app.mjs --readme-references`：喺無頭 Chrome 入面，對 README 每條準確網址影一張 1600x1000 嘅全頁截圖，即係佢哋第一次發佈時嘅尺寸。每張圖都會寫低頁面渲染出嚟嘅文字同所有欄位嘅值，做私隱檢查要讀嘅證據。

## 允許清單

每個工作都喺工作流程檔案入面列明可以上載邊啲路徑。唔喺清單上嘅嘢，無論截圖報告點講，都唔會預備上載。

- 原生，23 張：`docs/readme-assets/` 下面嘅 `yum-20260811-*`、`shot-*` 同 `native-material-*`，加上 README 用到嘅 `docs/screenshots/` 圖片（通知、版本歷史、正規表示式建構器、外觀、專案分頁、兩個偏好設定分頁、檔案選單、精靈嘅墨水選擇頁、設定檔同備份，同埋三張現行嘅處理側欄圖）。
- 設計參考，3 張：上面講嗰幾張 `material-*`。

兩張處理側欄「之前」嘅圖係已修正缺陷嘅歷史證據。重影嘅話會用舊名顯示修好咗嘅狀態，所以永遠唔會重影。

有四個原生做法原本錯咗，今次一齊改正：

| 圖片 | 之前 | 而家 |
| --- | --- | --- |
| `shot-prepare-sidebar.png`、`after-sidebar-readable.png` | `Process` 標籤：一條 350 乘 53 像素、得個區段標題嘅窄條 | 名叫 `Sidebar` 嘅視窗，即係說明文字講嘅成個「準備」側欄 |
| `after-header-intact.png` | 淨係 `Process` 標籤 | 設定標題列（標題、全域同物件切換、比較同表格按鈕），係一個冇名嘅面板，經 `Compare presets` 按鈕嘅上一層攞到（`"parent": 1`） |
| `native-material-filament-manager-light-en.png` | 先 `nav:Prepare` 再 `nav:Ink`，結果撞中側欄嘅墨水標題，影咗「準備」頁 | 淨係 `nav:Ink` |

`recapture.py` 而家亦都認得工作區分頁列嘅新名 `Workspace navigation` 同舊名 `Navigation rail`；分頁列存在但冇嗰個分頁嘅時候，唔會再退返去成個視窗搵：之前就係呢個後備做法令 `nav:Ink` 撳咗落側欄。

## 私隱檢查

`scripts/md3/check-readme-screenshot-privacy.py` 喺上載之前行，有疑問就當唔安全。一張圖要全部符合以下條件先會預備上載：

- 報告嗰行係 `done`，而路徑喺嗰個工作嘅允許清單上；
- 證據檔存在而且完整，以傾印嘅結束記錄收尾；
- 證據入面嘅文字，無論原文定 JSON 解碼之後，都冇 `Public` 以外嘅 Windows 用戶設定檔路徑（同 `ui-md3/tests/evidence-privacy.test.mjs` 用同一個模式）、POSIX 主目錄、執行器嘅帳戶名或者電腦名、`runneradmin`、GitHub 權杖模式，或者電郵地址；
- 檔案係檢出目錄入面嘅 PNG：成個視窗或者成頁至少 10 KiB，裁剪出嚟嘅控制項至少 1 KiB，最多 32 MiB；而且唔係空白：128 像素縮圖上至少有 12 種唔同顏色，色版標準差至少 10，即係加密託管截圖用嘅同一個測試。

被扣起嘅圖會用一個固定嘅原因字記錄，例如 `email` 或者 `blank-image`。撞中嘅文字本身永遠唔會印出嚟，亦唔會儲存。一張都唔過嘅話，工作就會失敗，乜都唔會上載。

上載檔（`readme-screenshots-<執行編號>` 或者 `readme-design-references-<執行編號>`）入面只有：

- 預備好嘅 PNG 檔，放喺佢哋喺儲存庫嘅路徑；
- `report.json`：每行嘅序號、路徑、狀態、SHA-256、闊同高，或者被扣起嘅原因；
- `SHA256SUMS`，格式啱 `sha256sum -c` 讀。

版面傾印、頁面文字、安裝收據同所有記錄檔都留喺執行器度，跟住執行器一齊刪走。

## 私隱模式

啲圖係公開儲存庫一個工作流程產物入面嘅普通 PNG 檔：三日之內，任何登入咗 GitHub 嘅人都可以下載。私隱檢查係自動閘門，唔係保證。版面傾印會報告應用程式自己視窗嘅文字，但係睇唔到畫喺 3D 畫布上面、網頁檢視入面或者原生彈出選單入面嘅文字。所以每張圖喺提交之前都要有人親自覆核；工作流程本身永遠唔會提交、推送或者發佈任何其他嘢。

## 限制

- 未真正觸發工作流程之前，以上全部都未證實。版本一定要帶住有結束記錄嘅版面探針，同埋做法用到嘅驅動指令。
- 主頁同設定精靈嘅內容係網頁檢視。如果佢哋喺視窗截圖入面係空白，像素檢查會扣起佢哋。
- 如果工作區分頁列喺 1200 像素闊時將墨水分頁收咗入溢出選單，墨水嗰行會報告為受阻，唔會影咗第二頁當數。
- README 入面較早期安裝版截圖嘅說明文字描述緊舊圖；換圖嘅時候要一齊更新。

## 驗證

- `node --test ui-md3/tests/readme-screenshots-workflow.test.mjs` 釘死：淨係手動觸發、唯讀權限、釘死嘅 action、權杖只喺安裝嗰步、私隱檢查喺上載之前、保留三日、對照 README 同重影清單嘅純 PNG 允許清單、上載路徑只係預備好嗰個目錄，同埋設計參考圖用 README 嘅網址。
- `python scripts/md3/test-readme-screenshot-privacy.py` 會整一個乾淨測試資料，再每種洩漏類別各整一個，檢查邊啲預備上載、邊啲被扣起同埋原因。兩個工作截圖之前都會先行佢。
