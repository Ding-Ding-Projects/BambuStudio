---
translation-of: mcp-and-cli.md
source-sha256: 1a01bf95c2abcc4f5b722aad264224d4c0ba42fc3b064411ea780a6e0b9d7f5e
review-status: agent-drafted
---

> 英文原文：[MCP and command-line automation](mcp-and-cli.md)

# MCP 同命令列自動化

## 交付同狀態

Windows 套件包含獨立配套程式 `automation/bambu-automation.exe`，提供 stdio 同 Streamable HTTP MCP 傳輸，以及使用同一經驗證指令服務嘅 CLI。唔需要另外安裝 .NET runtime。

實作同驗證記錄喺 [issue #53](https://github.com/Ding-Ding-Projects/BambuStudio/issues/53)。有原始碼唔等於有打包後執行證據。針對性託管工作流程檢查受管理服務；另外嘅版本執行驗證流程檢查已安裝原生應用程式。真實印表機驗證仍然分開處理。

## 啟用應用程式實例

啟動前設定 `BAMBU_AUTOMATION=1` 同 `BAMBU_AUTOMATION_ROOTS`。根目錄值係用分號分隔、明確允許嘅目錄清單。一般使用時唔設定開關，就唔會有原生自動化監聽器。

原生橋接只接受同一使用者嘅本機具名管道用戶端。每個已啟用程序有自己嘅 `BambuStudio.Automation.v1.<PID>` 端點。有多個程序執行時，必須明確選擇實例。配套程式轉送要求前會檢查工作區存取；原生橋接亦會獨立檢查根目錄。

## 連接本機 MCP 用戶端

設定 MCP 用戶端以以下參數啟動已安裝配套程式：

```text
serve --transport stdio --workspace C:\Models
```

用戶端 `command` 欄位要使用實際已安裝執行檔嘅絕對路徑，每個選項分開傳入參數。標準輸出只包含協定訊息；診斷使用標準錯誤。單靠用戶端設定唔會啟用原生實例。

## Streamable HTTP

使用 `serve --transport http`，並按[指令參考](../../../automation/README.md)設定驗證同端點選項。預設只限回送介面。非回送存取需要 HTTPS、驗證同明確網絡設定。Origin 同 Host 檢查保護端點，阻止瀏覽器跨來源同重新綁定要求。唔好將憑證放入命令參數、URL、設定範例或紀錄。

呢個版本支援可提供已設定 bearer 憑證嘅用戶端，唔實作瀏覽器帳戶註冊或 OAuth 授權伺服器。

## CLI 備用路徑

命令介面使用同 MCP 一樣嘅操作：

```text
bambu-automation.exe command capabilities --arguments "{}" --json --workspace C:\Models
```

複雜參數按指令參考使用 JSON 檔案或標準輸入。CLI 係傳輸備用路徑，唔係另一套切片實作。原生實例指令仍需已啟用應用程式；無介面工作使用套件內切片器，喺隔離目錄執行。

## 工作同印表機操作

專案檢視、匯入、設定、切片、匯出、工作監察同已設定印表機指令，透過功能探索公開。不支援嘅裝置功能會返回明確錯誤，唔會捏造成功。

明確開始列印嘅呼叫唔會再加第二個確認對話框。呼叫必須指定印表機、要求 ID、已完成原生 `sliceJobId` 同必要列印設定。原生就緒同支援映射檢查仍然有效。重試同一要求唔可以開始另一份實體列印。網絡結果唔確定時會如實報告，提交新要求前必須同印表機狀態核對。

原生切片同匯出接受可選、由零開始嘅 `plateIndex`；省略或 null 代表目前列印板。揀索引前先檢查列印板清單。無介面切片保留獨立 `plate` 慣例：零代表所有列印板，正數選擇該原生 CLI 列印板編號。模型匯出支援適用 FFF 列印板幾何嘅二進位 STL。需要互動式布林選擇嘅負體積會被拒絕。專案儲存使用 `.3mf`；可列印匯出使用已切片 `.3mf` 封存。

印表機操作重用應用程式已設定連線。憑證永遠唔會返回 MCP 用戶端。冇提供任意 shell、原始印表機指令或無限制 G-code 執行工具。

## 失敗同私隱界線

檔案解析後必須位於已設定工作區根目錄內。重新解析穿越同隱含覆寫會被拒絕。未儲存專案唔會被默默丟棄。長操作有受限工作狀態、取消同明確結果。傳輸斷線唔代表實體印表機指令已撤銷。

除非明確有獲授權硬件可用，驗證使用公開模型測試資料同模擬印表機。呢項實作工作冇進行本機建置、測試、安裝或執行驗證。託管證據記錄確切原始碼同套件二進位身分。

## 相關文件

- [指令參考](../../../automation/README.md)
- [自動化索引](README.md)
- [版本供應鏈](../releases/windows-release-supply-chain.md)
