---
translation-of: monitor-atlas.md
source-sha256: 818fdc9a351e32c037b5b3e924d11061bf5b6bf284cfff64125ef8c312a167c4
review-status: agent-drafted
---

> 英文原文：[Studio Atlas native monitor surfaces](monitor-atlas.md)

# Studio Atlas 原生監察介面

呢次來源外觀更新將 Atlas 層次套用到原生監察及打印機選取，唔能夠證明原生編譯、畫面幾何、相機或打印命令成功。呢個實作範圍排除啟動程式及硬件互動。

## 改動頁面

| 來源 | 外觀 | 保留邊界 |
| --- | --- | --- |
| `StatusPanel.cpp` | 頁面背景、最低層遙測卡、低層標題、可見 16 像素 Control／Printing Progress 標題、按文字量度最小高度、縮放只換算一次 DPI | 相機、溫度、風扇、移動、AMS、進度、更多操作同啟用條件 |
| `MultiMachinePage.cpp` | 量度打印機列高、不透明圓角底色、選取／懸停、選取文字同內縮鍵盤焦點 | 方格、列身份、選取事件、鍵盤同裝置篩選 |
| `MediaPlayCtrl.cpp` | 相機頁尾底色、原 40 DIP 列內方形目標、13 像素狀態及等寬診斷、水平間距 | 播放、工作階段追蹤、重試、URL、執行緒、剪貼簿及網絡回呼 |
| `PrintOptionsDialog.cpp` | 使用時解析說明文字／分隔線語意色，強調 AI Detections 章節 | 切換、敏感度、能力顯示、命令同驗證 |

重上色走訪配合新角色，Device 配色繼續解析用戶強調色。進度數字同進度條不變。選取列唔改尺寸，而且有原勾選符號同鍵盤行為，唔只靠顏色。冇新增文字、ID、命令、持久化欄位、協定值或資產，AMS 模型同獨立讀取狀態修正不變。

## 明確剩餘擁有權

`MonitorPage.cpp` 只係主 sizer，唔係另一個遙測實作，所以唔加第二層外距。`AmsWidgets.cpp` 係料槽模型，保持不變；呢個唔代表材料子頁面全部完成。

`AMSControl`、`AMSPopup`、`AMSSetting`、`AMSRoad`、`FanControlPopup`、`CameraPopup`、`CameraHUD`、`PrinterPartsDialog`、`MachineList`、`LocalTaskManagerPage`、`CloudTaskManagerPage`、`MultiMachineManagerPage`、噴嘴架、溫度編輯同打印機專用對話框都要各自來源及執行驗證。DeviceWeb 等內嵌頁面另有擁有者。停用／等待／離線規則保留，冇捏造新狀態或聲稱已實際操作。

## 驗證

`ui-md3/tests/ams-reading-state.test.mjs` 八項來源模型測試通過，涵蓋混合 Lite 讀取身份、保留設定、普通裝置、單槽 HT 索引、閒置遙測、界限及原生／網頁共用讀取器。唔係編譯或硬件證據。來源審查比較事件綁定、硬件命令及不變 AMS 模型，`git diff --check` 檢查空白。冇完整建置、啟動、擷圖、安裝執行或打印操作。

### DPI 審查修正

初版有兩個缺陷。相機按鈕將已縮放值傳畀自行換算嘅 `SetIconButton`，而家傳設計值 `32`，四 DIP 內距仍喺 sizer 邊界換算。200% 下目標為 72 × 64 實體像素，加內距頁尾總高 80，而唔係超出頁尾嘅 136 × 128。

打印標題原先保留建立時計算嘅 sizer 最小高度。共用 `layout_printing_title` 而家重新套字型、清量度快取、取 40 DIP 同文字高加 16 DIP 之較大值，更新 sizer／面板最小值、清面板快取及排版。建立同 `msw_rescale` 共用該方法；縮放亦清打印面板快取及排版，搬返較低 DPI 可以縮返。

`ui-md3/tests/monitor-atlas-dpi.test.mjs` 執行由產品來源抽出嘅幾何表達式及方法內容。三項測試喺初版失敗，修正後通過，涵蓋 100%、125%、150%、200% 相機幾何、文字／DPI 雙向變化、字型／快取次序及真實建立／縮放入口。連八項 AMS，共十一項通過；唔係原生繪製證據。

`StatusPanel::msw_rescale` 原有 Control 標題仍包含另一個雙重 `FromDIP(PAGE_TITLE_HEIGHT)`，呢次有限修正按要求保留作獨立審查，唔宣稱該路徑已正確。

完成畫面驗收前，監察、揀選、打印選項同相機頁尾須涵蓋英文／廣東話／雙語、明暗、兩密度、四比例、正常／最小尺寸。記錄文字／控件邊界、焦點／選取、DPI／主題切換、不可用／等待／確認遙測、來源版本、執行檔雜湊同真實擷圖。本文唔能夠取代呢啲證據。
