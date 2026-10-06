---
translation-of: native-crash-symbols.md
source-sha256: 2ee5079d6e86f419b1b0d56023d7e0850e2c63cca68cdc889e66318198051981
review-status: agent-drafted
---

# 加密原生崩潰符號

一般 Windows 發佈工作流程唔會啟用原生偵錯資料。調查崩潰時，喺準確嘅候選分支執行
`build_all.yml`，設定 `debug-symbols=true`，並喺 `symbol-public-key` 提供經審核、
至少 3072 位元嘅 RSA SubjectPublicKeyInfo PEM 公開金鑰。私人金鑰只留喺受保護嘅
本地儲存，絕對唔好交畀工作流程。

呢個模式保留 Release 最佳化，啟用 PDB 同 `/FS`，唔讀取亦唔儲存應用程式建置快取。
編譯器 launcher 會清空，sccache 亦會停用，避免含符號嘅編譯輸出寫入快取。
依賴套件前綴仍然可以重用，但原生程式要完整重新編譯。工作流程照常產生無簽署嘅
Squirrel 發佈；符號模式唔會加入測試或者 lint。

收集步驟會讀取二進位檔案嘅 RSDS 記錄同 PDB 嘅 MSF 身份串流。
`BambuStudio.dll` 同 `bambu-studio.exe` 都必須各自有唯一、GUID 同 age 完全相同嘅 PDB。
收據記錄名稱、二進位檔案及 PDB 嘅 SHA-256、來源 SHA、run ID 同 attempt，唔含絕對路徑。
缺少或者唔匹配嘅 PDB 會令收集失敗，唔會扮成有可用符號。

只上傳加密 ZIP、envelope 同安全收據，名稱係
`encrypted-native-symbols-<run-id>-<attempt>`，保留七日。
ZIP 包含匹配嘅二進位檔案、符號同收據，上限係 1 GiB。
使用隨機 AES-256-GCM 金鑰加密，並以收據位元組做驗證資料；RSA-OAEP SHA-256
封裝 AES 金鑰。Envelope 提供 nonce、驗證 tag、封裝金鑰、準確嘅 base64 AAD、
公開金鑰雜湊同密文雜湊。原始 PDB 唔會加入公開發佈附件或者應用程式快取封存檔。

透過 `gh run download` 下載，先核實密文雜湊同公開金鑰身份，再喺本地解封裝金鑰，
用 envelope 嘅準確 AAD 做驗證同解密。驗證失敗、收據唔符或者二進位雜湊唔同，
都必須拒絕使用。某個候選版本嘅符號唔能夠解釋另一個二進位檔案嘅準確來源行號；
舊版本嘅崩潰報告仍然需要當時原本嘅符號。

設定同編譯記錄無論成功定失敗都會另外保留，託管工作目錄同暫存目錄前綴會換成佔位字。
呢啲記錄只證明建置結果，唔係執行中行為或者崩潰修復證據。
符號傳送要有自己完成咗嘅託管工作流程，先可以話已核實。
