---
translation-of: qr-pairing.md
source-sha256: e6137ada85e8483fcbc7c614a02357839ddbcac97b0b0c5674838ee4062ea2bd
review-status: agent-drafted
---

# 本機驗證器 QR 配對

`PairingQr::encode` 使用已隨程式提供嘅 Nayuki QR 編碼器，採用中等糾錯，四邊必須留四個模組闊嘅白色靜區。編碼內容係標準 `otpauth://totp/` URI；登記服務會驗證簽發者、帳戶、摘要演算法、位數同期長。資料上限係 2048 個位元組。兩款主題嘅原生檢視都係白底黑碼，設有中性無障礙名稱，亦可以改用手動登記密鑰。

鎖定精靈用 OpenSSL 隨機來源喺本機產生新密鑰。QR 碼同手動密鑰預設隱藏，要使用者明確要求先顯示。收起登記畫面會移除矩陣並清空已顯示嘅手動密鑰。使用者輸入嘅當前驗證碼通過驗證後先登記種子。呢啲係臨時登記元件，唔可以用嚟顯示已儲存嘅密鑰。共用敏感元件登記表必須將佢哋排除喺截圖、歷史、診斷、日誌、分析同匯出之外。產生嘅 QR 圖片唔會寫入磁碟。

## 驗證

純矩陣實作同內置編碼器已用 MSVC 編譯。獨立 Python ZXing 2.3.0 解碼器直接由記憶體 PGM 串流讀取公開合成測試資料，確認可掃描、解碼所得登記參數完全相符，以及四模組靜區。實際輸出如下。

```text
PASS independent ZXing decode, exact enrollment parameters and four-module quiet zone
```

呢個結果只驗證編碼器輸出，未有驗證原生畫面實際顯示或者實體相機掃描。

## 圖片匯入封裝接點

原來只確認有內置 QR 編碼器，未確認有內置解碼器。`QrImport.cpp` 係工作程序用嘅 **ZXing-cpp 2.3.0** 轉接器，使用文件列明嘅 `ReadBarcodes`、`ReaderOptions`、`ImageView` 同 `Barcode` API。支援嘅依賴建置提供確實嘅 `ZXing::ZXing` CMake 目標之後，先可以啟用 `SLIC3R_LOCAL_SECURITY_QR_DECODER`。欠缺指定套件，設定階段就會停止。開發者自己安裝嘅 Python 模組或者 PATH 工具唔會被當成已交付功能。

封裝時要將 `libslic3r_qr_import` 連結到現有隔離圖片工作程序，一併提供完整離線執行環境，再驗證安裝好嘅執行檔。工作程序只接受正規化亮度資料：每邊必須大過零而且唔超過 4096，最多 4,194,304 個像素，緩衝區長度必須完全吻合。檔案同剪貼簿解碼器要喺同一隔離工作程序驗證檔案位元組、影格數同解碼尺寸，先可以配置記憶體。父程序要限制 CPU、記憶體、運行時間同取消操作。像素緩衝區同解碼結果只經受保護嘅本機管道傳送，唔可以出現喺指令參數、日誌或檔案。唔准用網絡；多個 QR 符號、結構化附加片段、不支援內容同無效登記參數都要拒絕。

封裝驗證完成後，主程式先可以提供以下 `Hooks` 回呼：

- `decode_qr_file(path)` 解碼使用者所揀嘅 PNG/JPEG，回傳使用後會抹除嘅 URI 緩衝區。
- `decode_qr_clipboard()` 解碼使用者所揀嘅剪貼簿圖片。
- `scan_qr_camera(completion)` 用獲同意嘅本機相機掃描。完成回呼喺 wx 主執行緒執行，所屬介面銷毀前要取消。

如果冇回呼，原生控制元件保持可見但停用，並解釋缺少邊項能力。解碼成功會填入隱藏登記 URI，保留其參數，仍然要手動輸入確認碼。

可選 C++ 轉接器未有喺呢批工作同 ZXing 一齊編譯，唔可以當成已交付或者已驗證嘅匯入功能。獨立驗證編碼器時使用嘅測試用 Python ZXing wheel 唔符合封裝要求。

相關文章：[原生整合](native-integration.yue_HK.md)、[服務總覽](README.md)。
