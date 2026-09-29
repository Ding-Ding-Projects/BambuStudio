---
translation-of: dialogs-and-pickers.md
source-sha256: 396166bba69b6cc6de4454740d8761e1a903c0d0e3584c483abb7889fc73da29
review-status: agent-drafted
---

> 英文原文：[Dialogs and pickers](dialogs-and-pickers.md)

# 對話框同揀選器

Windows 程式要你輸入文字、數字或者揀選項，話你知佢忙緊，或者要你揀顏色嗰陣，打開嘅對話框全部都係 Material 對話框。
wxWidgets 內置嘅版本係系統對話框：佢哋嘅框、欄位、掣同顏色格都唔理主題同語言模式。

## 邊個換咗邊個

| wxWidgets 內置 | Material | 程式喺邊度問 |
| --- | --- | --- |
| `wxTextEntryDialog` | `TextEntryDialog`：套件文字欄，要打幾行就用套件文字區 | 外觀編輯器嘅「另存為預設」；工作區面板嘅提示；網頁檢視嘅開發者腳本提示 |
| `wxNumberEntryDialog`、`wxGetNumberFromUser` | `NumberEntryDialog`：套件數值欄 | 「克隆數量：」；「克隆」 |
| `wxMultiChoiceDialog`、`wxGetSelectedChoices` | `MultiChoiceDialog`：每個選項一行套件剔選框 | 物件清單入面層範圍嘅「加入設定」；相容預設 |
| `wxGetSingleChoiceIndex` | `SingleChoiceDialog`，即係 Windows 本身已經用緊嘅套件對話框，而家每個平台都用 | 語言清單同其他單選 |
| `wxBusyInfo` | `BusyInfo`：用對話框表面顏色嘅圓角面板 | 「重新載入來自：」、「替換來自：」 |
| `wxColourDialog` | Material 揀色器，有「最近使用」一行 | 墨水機槽位顏色；預設選單嘅墨水顏色同佢本身嘅揀色器；墨水揀選器嘅「更多顏色」；紋理匯入；批量墨水顏色；設定頁嘅顏色欄 |

## 點做

- 三個提示都係建基於 `MsgDialog`，所以有 Material 外殼、標題列、底部動作、Escape 同關閉掣。佢哋收內置版本嘅參數，次序一樣，
  所以呼叫嘅地方淨係要改類別名。提示文字同每個訊息一樣經過內文渲染器，雙語模式會將廣東話擺喺英文下面。撳 Enter 等於確定，
  欄位一開始已經有焦點，入面嘅文字全部揀咗。
- `NumberEntryDialog::GetValue()` 會讀你打咗嘅數字，就算欄位仲未確認都得，而且會限制喺範圍之內。
- `BusyInfo` 喺建構函數返回之前已經畫好，因為呼叫佢嘅程式跟住就會阻住事件迴圈。訊息係標題，雙語模式下面會有佢嘅廣東話；
  詳情（例如讀緊嘅檔案）跟喺後面，用細啲嘅輔助樣式。長路徑喺任何字元都可以換行，唔會超出面板。
- `wxExtensions.cpp` 入面嘅 `pick_filament_color()` 會打開揀墨水顏色嘅 Material 揀色器。墨水顏色係用 `#RRGGBB` 儲存，
  所以冇透明度滑桿，揀出嚟嘅顏色一定係不透明。最近用過嘅顏色，即係系統對話框喺程式設定入面保存嘅十六個自訂顏色，會喺
  「最近使用」下面做一撳就揀到嘅選項；每次確定咗嘅顏色都會搬去清單最前面。設定頁嘅顏色欄保留透明度滑桿，亦會提供同一份清單。
- 有兩個內置呼叫嘅字擺錯咗位：克隆提示將「克隆」擺喺欄位旁邊，「克隆數量：」反而做咗標題；相容預設嘅揀選器就用咗解釋做標題。
  而家兩個都擺返啱位。

## 邊啲保留原生，點解

- 五個喺 Material 介面存在之前，或者拆緊嘅時候彈出嚟嘅訊息盒：介面初始化失敗（兩個）、致命同嚴重錯誤，同第一次載入語言。
- 開檔、存檔同揀資料夾用嘅系統檔案對話框，因為佢哋帶埋用戶常用嘅位置、預覽同 Windows 殼層。
- 由「偏好設定」、「開發者工具」、「內部開發者模式」打開嘅日誌視窗。「開發者工具」分頁唔會編譯入任何發佈版本，所以冇一個
  發佈版本可以打開佢。

## 驗證

- `node --test ui-md3/tests/stock-dialogs.test.mjs` 會拒絕允許清單以外嘅所有內置提示、揀選器、忙碌通知同顏色對話框，同埋檢查
  Material 版本係咪用套件欄位砌成。
- `node --test ui-md3/tests/dialog-header-includes.test.mjs`：源碼檔用咗 Material 對話框，但係冇 include 路徑去到
  `MsgDialog.hpp`，就會失敗；呢個錯誤曾經令建置失敗。
- 仲未喺發佈版本度逐個打開過呢啲對話框。
