# LAN 模型投遞

呢個係你本地網絡上面嘅一個細網頁，其他人（手機、平板、手提電腦）可以用佢傳
3D 模型去呢部電腦嘅 Bambu Studio MD3。佢喺 Docker 容器入面運行；Bambu Studio
會攞走收到嘅模型，再問你每個要開啟定棄置。唔會自動開啟、切片或者打印任何嘢。

完整指南：Bambu Studio MD3 原始碼入面嘅
`docs/features/application-integration/lan-model-drop-site.yue_HK.md`，應用程式嘅「說明」都有。

## 啟動

開咗 Docker Desktop 之後，喺呢個資料夾開終端機，執行：

```powershell
docker compose up -d
```

網頁會喺呢部電腦每個網絡介面嘅 8833 埠收連線。用系統管理員身份開 PowerShell，
為私人網絡喺 Windows 防火牆開放呢個埠一次：

```powershell
New-NetFirewallRule -DisplayName "Bambu Studio LAN model drop" -Direction Inbound -Protocol TCP -LocalPort 8833 -Action Allow -Profile Private
```

## 連接 Bambu Studio

顯示接收站金鑰，然後貼入 Bambu Studio 嘅「偏好設定」>「LAN 模型投遞」：

```powershell
docker compose exec lan-model-drop node server/station-key.mjs
```

再開啟「由 LAN 投遞網站接收模型」。Bambu Studio 會顯示一條邀請連結同一個
QR 碼，畀你交俾對方；條連結已經帶住投遞碼，對方只要揀模型再傳送就得。

## 設定

將 `.env.example` 複製做 `.env`，再改你要改嘅（埠、接收站名稱、大小上限、模型
等幾耐、固定投遞碼或者公開地址），然後再執行一次 `docker compose up -d`。
`.env` 要保密：入面可能有接收站金鑰。

## 停止、更新、移除

| 工作 | 指令 |
| --- | --- |
| 停止（模型同金鑰會保留） | `docker compose down` |
| 安裝咗新版 Bambu Studio MD3 之後更新 | `docker compose up -d --build` |
| 全部移除，包括等緊嘅模型同金鑰 | `docker compose down -v` |

## 會做同唔會做嘅嘢

- 只接受 3MF、STL、STEP、OBJ 同 AMF，會按檔名同內容檢查。
- 要輸入 Bambu Studio 顯示嘅投遞碼；一分鐘內錯五次，嗰部裝置會被鎖五分鐘。
- 模型只會保留到 Bambu Studio 攞走，或者 24 個鐘。
- 唔會由互聯網載入任何嘢，冇分析追蹤，記錄入面冇檔名、投遞碼或者金鑰。
- 喺本地網絡用普通 HTTP。同一個網絡入面知道投遞碼嘅人都可以傳模型，網絡亦
  睇到傳緊乜。請喺你信任嘅網絡使用，或者放喺 HTTPS 反向代理後面，再設定
  `DROP_PUBLIC_URL`。
