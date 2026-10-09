---
translation-of: lan-model-drop-site.md
source-sha256: 194807a7d1ebb6674760da132ed825acf0502aa86bdf691ad71615bd6f103c1d
review-status: agent-drafted
---

> 英文原文：[LAN model drop site](lan-model-drop-site.md)

# LAN 模型投遞網站

LAN 模型投遞網站係一個喺你本地網絡上面、喺 Docker 容器入面運行嘅細網頁。其他人
用手機、平板或者手提電腦打開佢，就可以傳 3D 模型去你部電腦嘅 Bambu Studio MD3。
Bambu Studio 會攞走每個模型，顯示係邊個傳嘅，再問你要開啟定棄置。唔會自動開啟、
切片或者打印任何嘢。

呢篇文講容器本身：點樣啟動、其他裝置點樣連到、接收站金鑰、設定、更新同保安。
Bambu Studio 入面接收嗰邊喺 **偏好設定 > LAN 模型投遞** 同 **檔案 > LAN 模型投遞...**，
嗰度亦會整你交俾對方嘅邀請連結同 QR 碼。

## 你需要

- 呢部電腦上面嘅 Docker Desktop for Windows，或者同一個網絡入面任何一部 Docker
  主機（家用伺服器、可以行容器嘅 NAS、迷你電腦）。映像可以喺 64 位元嘅 Intel、
  AMD 同 ARM 機器上面運行。
- 呢部電腦上面嘅 Bambu Studio MD3。
- 對方要同容器喺同一個網絡：同一個 Wi-Fi 或者有線網絡。訪客 Wi-Fi 通常會將裝置
  分隔開，所以用訪客網絡嘅手機可能打唔開個網頁。

## 喺呢部電腦用 Docker Desktop 啟動

Windows 安裝會喺應用程式旁邊放一個 `lan-model-drop` 資料夾，位置係
`%LOCALAPPDATA%\BambuStudioMD3\app-<version>\lan-model-drop`。

1. 開啟 Docker Desktop，等到佢顯示引擎運行緊。
2. 喺呢個資料夾開 PowerShell：

   ```powershell
   cd "$env:LOCALAPPDATA\BambuStudioMD3"
   cd (Get-ChildItem -Directory app-* | Sort-Object LastWriteTime | Select-Object -Last 1).FullName
   cd lan-model-drop
   ```

3. 啟動容器：

   ```powershell
   docker compose up -d
   ```

   第一次啟動會建立映像，要下載一次 Node.js 基礎映像。之後啟動只需要幾秒。
4. 檢查佢係咪健康：

   ```powershell
   docker compose ps
   ```

   大約半分鐘之內，狀態會變成 `healthy`。喺呢部電腦用瀏覽器打開
   `http://localhost:8833` 就會睇到個網頁。

Docker Desktop 啟動嗰陣，容器會自己再啟動，直到你停止佢（睇[停止](#停止)）。

### 喺 Windows 防火牆開放呢個埠

其他裝置會經呢部電腦嘅 8833 埠連到個網頁。你未允許之前，Windows 防火牆會擋住
呢個埠。用 **以系統管理員身份執行** 開 PowerShell，執行一次：

```powershell
New-NetFirewallRule -DisplayName "Bambu Studio LAN model drop" -Direction Inbound -Protocol TCP -LocalPort 8833 -Action Allow -Profile Private
```

呢條規則只會用喺 Windows 當係私人網絡嘅網絡。請喺 **設定 > 網絡和網際網絡 >
Wi-Fi**（或者 **乙太網路**）**> 網絡內容** 確認屋企或者辦公室嘅網絡設定咗做
**私人網絡**。千祈唔好喺公用網絡開放呢個埠。之後要移除條規則：

```powershell
Remove-NetFirewallRule -DisplayName "Bambu Studio LAN model drop"
```

如果 Windows 為 Docker Desktop 彈出自己嘅防火牆提示，只允許私人網絡。

### 搵呢部電腦嘅 LAN 地址

Bambu Studio 連接好之後，會用連結同 QR 碼顯示人哋要打開嘅地址，所以通常唔使自己
搵。如果要自己搵：

```powershell
Get-NetIPAddress -AddressFamily IPv4 | Where-Object { $_.PrefixOrigin -in 'Dhcp','Manual' } | Select-Object InterfaceAlias, IPAddress
```

用連住共用網絡嗰張網卡（Wi-Fi 或者乙太網路）嘅地址，唔好用虛擬網卡（例如
`vEthernet (WSL)`）嘅地址。屋企網絡通常派 192.168 或者 10 開頭嘅私人地址。人哋
就打開 `http://<嗰個地址>:8833`。

## 連接 Bambu Studio

1. 喺 `lan-model-drop` 資料夾顯示接收站金鑰：

   ```powershell
   docker compose exec lan-model-drop node server/station-key.mjs
   ```

2. 喺 Bambu Studio 打開 **偏好設定 > LAN 模型投遞**，將金鑰貼入 **接收站金鑰**，
   網站地址保持 `http://localhost:8833`，再揀 **測試連線**。狀態會顯示 **已連接**。
3. 開啟 **由 LAN 投遞網站接收模型**。Bambu Studio 就會顯示邀請：一條好似
   `http://<呢部電腦嘅地址>:8833/#code=482915` 嘅連結，同埋同一條連結嘅 QR 碼。
   將連結傳俾對方，或者畀對方掃描 QR 碼。

個網頁會由連結填好投遞碼，再由網址列移走佢；對方只要揀模型再傳送。Bambu Studio
嘅 **新連結** 會整一個新投遞碼，舊連結就會失效。

## 喺網絡上面另一部 Docker 主機運行

容器唔一定要喺裝咗 Bambu Studio 嗰部電腦運行。

1. 將 `lan-model-drop` 資料夾複製去另一部主機。
2. 喺嗰部主機嘅資料夾執行 `docker compose up -d`；如果佢有防火牆，開放 8833 埠。
3. 喺嗰度用 `docker compose exec lan-model-drop node server/station-key.mjs`
   顯示接收站金鑰。
4. 喺 Bambu Studio 將網站地址設做 `http://<主機嘅地址>:8833`，再貼入金鑰。之後
   邀請連結就會用呢個地址。

如果個網頁放咗喺反向代理後面，或者想人哋用固定名稱，就設定 `DROP_PUBLIC_URL`
（睇[設定](#設定)）；Bambu Studio 會用呢個地址整邀請連結同 QR 碼。

## 顯示或者更換接收站金鑰

Bambu Studio 要用接收站金鑰先可以列出、下載同移除收到嘅模型。容器第一次啟動
嗰陣會整一條，係 32 個隨機位元組，寫成 43 個字元，存喺佢嘅資料卷入面。隨時都
可以顯示：

```powershell
docker compose exec lan-model-drop node server/station-key.mjs
```

金鑰永遠唔會寫入容器記錄。要更換（例如畀人睇咗之後），移除存起嘅金鑰再重新
啟動：

```powershell
docker compose exec lan-model-drop rm /data/station-key
docker compose restart
docker compose exec lan-model-drop node server/station-key.mjs
```

然後將新金鑰貼入 Bambu Studio。你亦可以喺 `.env` 檔用 `DROP_STATION_KEY` 自己揀
金鑰。

## 設定

喺 `lan-model-drop` 資料夾將 `.env.example` 複製做 `.env`，改你要改嘅，再執行
一次 `docker compose up -d`。所有設定都可以唔填。

| 設定 | 預設 | 意思 |
| --- | --- | --- |
| `DROP_PORT` | 8833 | 人哋打開嘅呢部電腦嘅埠 |
| `DROP_STATION_NAME` | Bambu Studio | 網頁上面顯示嘅名稱（「傳送去 ...」） |
| `DROP_MAX_BYTES` | 268435456（256 MB） | 接受嘅最大模型 |
| `DROP_TTL_HOURS` | 24 | 模型等 Bambu Studio 攞走嘅鐘數，過咗就移除（最多 720） |
| `DROP_QUEUE_MAX_FILES` | 50 | 同一時間最多等緊嘅模型數目 |
| `DROP_QUEUE_MAX_BYTES` | 2147483648（2 GB） | 同一時間最多等緊嘅位元組 |
| `DROP_STATION_KEY` | 自動產生 | 接收站金鑰，16 至 512 個可見字元，唔可以有空格 |
| `DROP_CODE` | 隨機 6 個數字 | 固定投遞碼，4 至 12 個數字；Bambu Studio 唔可以更新佢 |
| `DROP_PUBLIC_URL` | 冇 | 反向代理後面用嚟整邀請連結嘅地址，例如 `https://drop.example.org` |

數值唔啱嘅話，容器會停止，並喺 `docker compose logs` 留低訊息，唔會估你想點。
`DROP_PUBLIC_URL` 一定要係 `http://` 或者 `https://` 地址，唔可以有用戶名、密碼、
查詢或者片段。

### 改埠

喺 `.env` 加 `DROP_PORT=9000`（或者其他冇用緊嘅埠），再執行 `docker compose up -d`。
然後：

- 將防火牆規則改做新埠（移除再用 `-LocalPort 9000` 加返）；
- 將 Bambu Studio 嘅網站地址改做 `http://localhost:9000`。

容器入面嘅服務永遠用 8080；改嘅只係對外公開嘅埠。

## 更新

安裝咗嘅資料夾屬於某一個應用程式版本：Bambu Studio MD3 每次更新都會裝一個新嘅
`app-<version>` 資料夾，之後再移除舊嗰個。運行緊嘅容器唔靠呢個資料夾，所以會繼續
運作。要轉用新版本嘅網頁，喺新版本嘅 `lan-model-drop` 資料夾執行：

```powershell
docker compose up -d --build
```

Compose 項目永遠叫 `lan-model-drop`，所以呢個指令會取代舊容器，並保留同一個資料卷：
接收站金鑰、投遞碼同等緊嘅模型都會保留。如果你有用 `.env` 檔，請先將佢複製去新
資料夾，或者放喺一個固定位置，然後喺每個 `docker compose` 指令加
`--env-file <你嘅 .env 路徑>`。

## 停止

| 你想 | 指令 |
| --- | --- |
| 暫停，直到你再啟動 | `docker compose stop` |
| 再啟動 | `docker compose start` |
| 移除容器，保留金鑰同等緊嘅模型 | `docker compose down` |
| 全部移除，包括金鑰同等緊嘅模型 | `docker compose down -v` |

容器停咗嘅時候，Bambu Studio 會顯示 **連唔到**，並會慢慢咁繼續檢查。

## 私隱同保安

- **會保留乜嘢。** 每個等緊嘅模型會連同整理過嘅檔名、可以唔填嘅寄件人名、大小、
  SHA-256 同收到嘅時間一齊存起。Bambu Studio 攞走之後，或者過咗
  `DROP_TTL_HOURS`，就會移除。接收站金鑰同投遞碼係只有服務先讀到嘅檔案。
- **會記錄乜嘢。** 隨機嘅項目 ID、模型類型同大小、鎖定同移除。永遠唔會記錄檔名、
  寄件人名、投遞碼或者接收站金鑰。
- **乜嘢都唔會離開你嘅網絡。** 個網頁唔會由互聯網載入任何嘢：字型同圖示都係內置嘅，
  亦冇分析追蹤。映像建立好之後，容器唔需要連互聯網。
- **投遞碼。** 寄件人要有目前嘅投遞碼。一分鐘內錯五次，嗰部裝置會被鎖五分鐘。
  喺邀請連結入面，投遞碼放喺 `#` 後面；瀏覽器永遠唔會將呢部分傳去伺服器或者其他
  網站，而個網頁一打開就會將佢由網址列移走。有目前連結嘅人都可以傳模型，直到你揀
  **新連結**。
- **接收站金鑰。** 只有 Bambu Studio 需要佢。有金鑰嘅人可以列出、下載同移除等緊嘅
  模型，所以千祈唔好放入邀請，亦唔好分享你嘅 `.env`。Bambu Studio 會為你嘅 Windows
  用戶帳戶加密保存佢。
- **接受乜嘢。** 只接受 3MF、STL、STEP、OBJ 同 AMF，按檔名同內容檢查，而且唔可以
  超過大小上限。檔名會整理過，只用嚟顯示；模型用隨機名稱儲存。Bambu Studio 下載
  之後會再檢查大小、SHA-256 同類型，只會喺你揀 **開啟** 先開啟模型。
- **普通 HTTP。** 個網頁喺你嘅本地網絡用 HTTP。同一個網絡嘅人睇到傳緊乜，而嗰度
  知道投遞碼嘅人都可以傳模型。請喺你信任嘅網絡使用。其他情況，請將佢放喺 HTTPS
  反向代理後面，再設定 `DROP_PUBLIC_URL`。千祈唔好喺路由器將呢個埠轉發去互聯網。
- **鎖緊咗嘅容器。** 服務用冇特權嘅用戶、喺唯讀檔案系統運行，冇任何 Linux 權能，
  唔可以攞到更多權限，有記憶體、CPU 同程序數目上限，只會寫入佢嘅資料卷。佢嘅回應
  帶住嚴格嘅 Content-Security-Policy 同其他保安標頭。

## 有嘢唔得嘅時候

| 你見到 | 要檢查 |
| --- | --- |
| 手機打唔開個網頁 | 手機喺同一個網絡，唔係訪客網絡；防火牆規則存在；`docker compose ps` 顯示 `healthy` |
| 「投遞碼唔獲接受」 | 攞條新連結；投遞碼可能已經更新咗 |
| 「錯咗太多次投遞碼」 | 等五分鐘，再用條連結 |
| 「投遞箱滿咗」 | 喺 Bambu Studio 開啟或者棄置等緊嘅模型 |
| Bambu Studio 顯示 **接收站金鑰錯誤** | 再顯示一次金鑰，然後貼入 |
| Bambu Studio 顯示 **連唔到** | Docker Desktop 運行緊，而且容器已經啟動 |

## 已經檢查咗乜嘢

服務、網頁同容器設定由 `tests/lan_model_drop/` 入面嘅自動測試覆蓋：每種接受嘅類型
同拒絕、接收站 API、喺真嘅無頭 Chromium 入面處理邀請連結、容器定義同 Windows
安裝規則。容器已經喺 Linux 嘅 Docker Engine 建立同運行，以寄件人同接收站身份試過，
並報告健康。喺 Windows 用 Docker Desktop 運行、防火牆規則，同埋喺真網絡用真手機
傳送，仲要喺 Windows 電腦上面觀察。
