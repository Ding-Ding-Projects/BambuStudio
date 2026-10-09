---
translation-of: instruction-mirror.md
source-sha256: 245859918defbfdfdbfff5511ea42637eea0ebac5e024a4d14720eb43ef9976d
review-status: agent-drafted
---

> 英文原文：[Shared instruction mirror](instruction-mirror.md)

# 共用指示鏡像

喺呢個 repository 做嘢嘅代理同貢獻者，都要跟維護者嘅共用代理指示。嗰份指示嘅正本係私人嘅，所以呢個 repository 會喺 `README.md` 同 `AGENTS.md` 兩個檔案入面，各自保留一份已清理嘅鏡像。咁樣，喺度做嘢嘅人唔使攞到正本，都睇得到啲規則。

鏡像係自動產生嘅。唔好人手改佢：喺呢度改咗，下次更新就會被覆寫，而且永遠唔會傳返去正本。要改指示，就要先改正本，然後喺改指示嘅同一個工作入面，再將佢鏡像出嚟。

## 鏡像喺邊度

每個檔案都只有一個標題係 **Shared agent instructions (mirror)** 嘅區塊。區塊開頭有一段註明，講明呢個係鏡像、唔好喺度改，並連結去呢篇文章。註明亦記錄咗三樣嘢：

- 鏡像匯出時用嘅正本來源版本；
- 寫入鏡像嘅日期；
- 鏡像內文嘅 SHA-256。

喺 `README.md`，區塊放喺 **Report issue** 之前，而長篇指示內文會收埋喺一個可展開嘅部分入面，等第一次睇嘅讀者唔使捲好耐。喺 `AGENTS.md`，區塊跟喺 repository 本身嘅規則後面，代理會先睇嗰啲規則，而內文會直接顯示。兩個檔案嘅內文逐個 byte 都一樣。

區塊用 `shared-instructions-mirror:` 開頭嘅 HTML 註解標記界線。有一個隱藏嘅中繼資料註解，用腳本讀得明嘅格式，再記錄一次版本、日期同摘要值。

## 「已清理」係咩意思

鏡像保留晒啲規則，但會拎走所有認得出喺邊度寫、為咩基礎設施而寫嘅細節。入面唔會有 repository 以外嘅絕對路徑、作業系統用戶名或者主目錄、機器名稱或者主機清單、本地網絡或者遠端 IP 位址、SSH 目標、容器主機、token 同憑證。

如果一條規則唔講私人細節就講唔清楚，匯出時會將佢概括化：講返係邊一類位置或者主機，而唔係點名。唔會因為難清理，就靜靜雞刪走一條要求。

私人對話詞彙係唯一一個刻意嘅例外。正本指示規定，喺公開 repository 嘅任何鏡像入面，都要成個略去，連佢嗰部分嘅每一條都唔留，所以鏡像唔會將佢概括化。`AGENTS.md` 原有嘅 **Agent conversation vocabulary** 部分已經講明守則，而且冇點名任何詞彙。

## 更新鏡像

1. 先改正本指示。
2. 用維護者嘅清理匯出工具，匯出一份已清理嘅副本。匯出檔要寫喺呢個 repository 以外，繼續之前要先檢查一次。匯出時會略去詞彙部分，並將剩低嘅私人位置全部概括化。
3. 喺 repository 根目錄執行：

   ```text
   node scripts/instructions/refresh-instruction-mirror.mjs --source <export.md> --source-revision <revision>
   ```

   `<revision>` 係匯出所用嘅正本版本，寫法係 7 至 64 個小楷十六進位字元。匯出檔開頭如果有一個第一級標題，會換成鏡像標題。如果有其他第一級標題，或者文字入面本身已經有鏡像標記，就會拒絕，乜都唔寫。加 `--date YYYY-MM-DD` 可以記錄指定日期；如果冇加，有改動嘅鏡像會記錄今日嘅 UTC 日期，冇改動嘅就保留原本記錄嘅日期。

   寫入任何嘢之前，更新會用下面講嘅私隱掃描，掃晒成份匯出檔，連標題都掃。只要搵到一項，就會拒絕更新。加 `--private-terms <file>`，就會連私人詞彙清單一齊掃。
4. 執行 `node scripts/instructions/check-instruction-mirror.mjs --require --source <export.md>`，然後用維護者嘅公開界線掃描檢查 `README.md` 同 `AGENTS.md`。
5. 喺改指示嘅同一個工作入面，提交更新咗嘅檔案。

`--check` 乜都唔寫。如果任何一個檔案冇用呢份匯出應該產生嘅鏡像，就會以狀態 1 結束；兩個都係最新，就以狀態 0 結束。

Windows 嘅 checkout 可能會用 CRLF 行尾儲存 `README.md` 同 `AGENTS.md`，匯出檔開頭亦可能有 byte-order mark。兩個腳本都接受。鏡像內文會用 LF 行尾比較，而更新會用每個檔案原本嘅行尾寫返去，所以重複執行都唔會改到任何嘢。

## 私隱同偏差把關

`node scripts/instructions/check-instruction-mirror.mjs` 會檢查兩個檔案：通過就以狀態 0 結束，唔通過就以 1 結束，用法錯就以 2 結束。以下情況會唔通過：

- 鏡像內文同記錄咗嘅 SHA-256 唔再對得上，即係有人手改過；
- 標示或者排版同自動產生嘅樣唔同；
- 得一個檔案有鏡像，或者兩個檔案嘅內文或者來源中繼資料唔同；
- 標記重複、唔成對，或者次序錯；
- 內文有私人細節；
- 加咗 `--source <export.md>` 時，鏡像比嗰份匯出舊。

如果 repository 完全冇鏡像，會報告 `ABSENT` 並當通過，除非加咗 `--require`。

私隱掃描會報告每項發現嘅行、欄同類別，但永遠唔會印出配對到嘅文字，所以拒絕訊息唔會將秘密抄入記錄。佢會標記：

- repository 以外嘅絕對路徑：例如 `C:\…` 呢類磁碟機路徑、網絡分享、`file://` 路徑，同埋喺主目錄、用戶、掛載、磁碟區、暫存同資料目錄底下嘅 POSIX 路徑；
- 機器名稱同主機清單：Windows 預設電腦名稱、`.local`、`.lan` 或者 `.internal` 呢類私人後綴嘅主機名稱、硬件位址，同埋 SSH 嘅 `Host` 同 `HostName` 項目；
- IPv4 同 IPv6 位址；
- SSH 目標，例如 `ssh://` URL、`user@host:path` 寫法、`ssh` 或者 `scp` 指令嘅目標，同埋個人帳戶位址；
- 容器主機：`DOCKER_HOST` 嘅值、`tcp://` 端點同 `docker -H` 主機；
- 常見供應商格式嘅 token、私人金鑰區塊、webhook URL、寫成 `password=…` 或者 `api_key: …` 嘅憑證、URL 入面嘅密碼，同埋授權標頭。

一般性嘅寫法仍然可以用，因為佢哋講嘅係一類位置，唔係某個特定位置：`%USERPROFILE%` 或者 `$HOME` 呢類環境變數、`~/` 路徑、`/usr/bin/env` 呢類系統路徑、loopback 同未指定位址、文件專用位址範圍 `192.0.2.0/24`、`198.51.100.0/24` 同 `203.0.113.0/24`，同埋 `noreply@anthropic.com`、`noreply@github.com` 同 `git@github.com` 呢啲角色位址。四段式版本號要寫喺講明佢係咩嘅字後面，例如 "version"、"release" 或者 "upstream"，或者前面加 `v`，咁就唔會當成位址。

私人詞彙清單要放喺呢個 repository 以外；呢度冇提交任何詞彙清單。用 `--private-terms <file>` 或者環境變數 `INSTRUCTION_MIRROR_PRIVATE_TERMS` 指定。檔案可以係 JSON 文件，用佢嘅字串值做詞彙（唔用鍵）；亦可以係純文字，一行一個詞彙，空白行同 `#` 開頭嘅行會略過。詞彙按完整字詞比對，唔分大細楷，而且永遠唔會印出嚟。清單讀唔到或者係空嘅，就當唔通過。冇清單嘅話，詞彙掃描會略過，輸出亦會講明。維護者自己嘅公開界線掃描仍然係必要步驟。

## 目前狀態

呢個 repository 暫時未發佈任何鏡像。發佈鏡像，等於將由維護者私人指示衍生出嚟嘅文字，抄入呢個公開 repository，所以要等維護者準備好已清理嘅匯出，並批准發佈先得。喺嗰之前，`README.md` 同 `AGENTS.md` 都冇鏡像區塊，把關工具會報告 `ABSENT`。更新工具、把關工具、佢哋嘅測試同呢套程序已經準備好，等第一次發佈用。

## 驗證

執行 `node --test ui-md3/tests/instruction-mirror.test.mjs ui-md3/tests/instruction-mirror-guard.test.mjs`。測試會用一個合成嘅指示檔，更新兩個檔案嘅暫時副本，然後檢查標示、記錄咗嘅版本、日期同摘要值、兩個檔案內文一樣、區塊位置、重複更新、檢查模式，同埋拒絕格式唔啱嘅輸入。測試會為每一類私人細節，餵一個合成樣本俾私隱掃描，再餵一段一定要通過嘅普通指示文字；亦會測試詞彙清單、拒絕未清理嘅匯出，同每一種偏差。最後一個測試會喺呢個 repository 本身執行把關工具，所以一旦發佈咗鏡像，真正嘅 `README.md` 同 `AGENTS.md` 都會受到把關。測試唔會讀取正本指示。
