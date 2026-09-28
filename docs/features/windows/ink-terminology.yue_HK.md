---
translation-of: ink-terminology.md
source-sha256: 431b97b43ef29f7ee860476befa8decfc9cc657a5937de2c679e4be4da5d155d
review-status: agent-drafted
---

> 英文原文：[Ink terminology (filament → ink, AMS → Ink Dispenser)](ink-terminology.md)

# 墨水術語（墨水 → 墨水、AMS → 墨水機）

呢個分支重新命名用戶面向項目喺 UI 中：

| 舊術語 | 英文 UI | 粵語 UI |
| --- | --- | --- |
| 墨水 / 墨水(s) | 墨水 / 墨水(s) | 墨水 |
| AMS | 墨水機 | 墨水機 |
| 同步 AMS（寬度限制側邊欄按鈕） | 同步 | 同步 |

## 機制

重新命名係實現為**翻譯目錄覆蓋只**。冇源
`_L()` msgids、配置鍵、設定類型名稱、檔案格式、CLI 旗幟或記錄字串被改變、所以切片行為、設定相容性、3MF/G-code輸出同上游合併不受影響。

- **英文**：Bambu Studio 運送一個英文覆蓋目錄嗰個
  `wxTranslations` 喺執行時載入（`GUI_App::load_language` →
  `AddCatalog(SLIC3R_APP_KEY)`）；因為目錄語言（「en」）與 msgid 語言不同（「en_US」）、wxWidgets 載入
  `resources/i18n/en/BambuStudio.mo`。墨水術語存活喺
  `bbl/i18n/en/BambuStudio_en.po` 作為 `msgstr` 覆蓋（msgids 未觸及）同
  係編譯到嗰個 MO。
- **粵語 / 雙語**：`bbl/i18n/yue_HK/BambuStudio_yue_HK.po` msgstrs
  而家使用 墨水 / 墨水機；編譯決定性由
  `bbl/i18n/yue_HK/compile_translation.py` 到
  `resources/i18n/yue_HK/BambuStudio.mo`（驗證門：佔位符、
  已審視分類、coverage.json、`--check` 重現能力）。
- **網頁表面**：`resources/web/data/text.js`（設定嚮導墨水
  選擇、首頁「墨水指南」、用戶設定過濾器）喺 `en`
  同 `yue_HK` 部分更新；其他語言保持佢哋現有術語。

## 涵蓋

- 每個 `msgid` 喺英文目錄包含獨立單詞
  「墨水(s)」（549 項）或「AMS」（97+ 項）而家攜帶墨水 /
  墨水機覆蓋、包括字串老舊上游 PO 係遺漏
  （例如側邊欄「添加墨水」按鈕、ConfigWizard 墨水頁面、
  「進料墨水」、AMS/腔室溫度警告）嗰啲被追加到
  PO 從一個源掃描 `src/slic3r` 同 `src/libslic3r`。
- 字邊界替換保持技術字面完整：配置鍵例如
  `filament_start_gcode`/`nozzle_temperature` 提及喺工具提示、
  URL 同格式佔位符（`%s`、`%1%`、`{}`）係未觸及、同
  「a 墨水」變成「an 墨水」度文法要求佢。
- 破壞/錯誤訊息保持佢哋精確意義；只有兩個項目係
  替換。

## 寬度限制標籤

`Sidebar::priv::adjust_filament_title_layout()` 擠壓尾隨按鈕
喺墨水部分標題中、所以光禿「同步」（同步）用於 `Sync AMS`
msgid 代替完整「同步墨水機」；工具提示
（`同步 AMS 同噴嘴資訊` → 「同步墨水機同噴嘴
資訊」）攜帶完整名稱。第一個嘗試呢個縮短、
「同步分配器」、仲然超越面板邊緣同係削減喺 `a4498bc72`。
更長重新命名標籤值得睇喺窄寬度（佢哋回流但冇
縮短）：「墨水機設定」（設備狀態頁面）同
「同步墨水機同噴嘴資訊」（工具提示、無限制）。

## 刻意遺漏

- **`AMS Materials Setting`** 已經顯示為「Materials Setting」透過一個
  上游複製編輯覆蓋、所以無 AMS 保持可見喺嗰個標題。
- 其他顯示語言（de/fr/ja/…）：上游術語保持。
- 內部/只記錄字串、HMS 雲端供應錯誤文字同任何 msgid 文字
  本身：設計未改變。

兩個表面被列喺呢度作為頑固由原始重新命名同自從
被重新命名；佢哋唔再係例外：

- **設備頁面網頁視圖** (`src/slic3r/GUI/DeviceWeb/device_page`)：`en` 同
  `yue_HK` i18next 目錄而家攜帶墨水值（「墨水管理員」、
  「墨水類型」、「搜尋墨水」）。佢嘅執行時包被生成、唔係提交──
  CMake `device_page_build` 目標重建 `resources/web/device_page/dist/`
  從呢啲地區──所以一個普通構建運送重新命名頁面。
- **ui-md3 設計工具演示** (`ui-md3/app`)：重新命名附帶佢嘅查詢鍵喺一個
  通過。嗰啲鍵係渲染英文字串、所以顯示文字同鍵有
  移動一起或每個粵語查詢會無聲地錯過。

## 歷史同散文文件

重新命名係顯示唯一喺時間以及喺範圍中。散文紀錄
**咩被運送同何時**──`## Landed` 波喺 `ROADMAP.md`、提交
表喺 `HANDOFF.md`、對比重存檔列──保持自己日期嘅措辭、所以一個 2026-07-24 項目仲然讀「AI 墨水掃瞄器」對於一個選單項目
嗰個讀「AI 墨水掃瞄器」今日。重寫一個日期記錄符合今日嘅
標籤使佢變差紀錄冇使任何野更容易找到；同樣原因係為乜嘢
`scripts/ci/Test-InkTerminology.ps1` 跳過過時 `#~` PO
項、嗰啲係合併歷史同永遠冇被載入。

散文描述**當前**產品──README 特徵同
截圖部分、`ROADMAP.md` 嘅 `## Remaining`、特徵文件──使用
墨水措辭、因為讀者係意圖找到嗰啲字喺螢幕。
識別符引用喺散文（`FilamentPicker`、`filamentRows`、`?view=filament`、
`filament_start_gcode`）保持上游拼寫無論佢哋出現度、當前
或歷史、正係呢個文件其餘部分予以原因。

一個截圖係一個日期紀錄太。一個擷取取自重新命名之前係唔
由編輯佢嘅標題更正：或者從一個當前構建重新拍攝、或說
喺周圍散文嗰個佢先於重新命名。一個標題聲稱
「墨水」超一個圖像嗰個清楚讀「墨水」係比之差
任一個。

## 驗證

- `py -3 bbl/i18n/yue_HK/compile_translation.py --check`──綠
  （620 已審視翻譯、可重現 MO）。
- `node resources/web/data/validate-text-locales.mjs`──綠（168 網頁鍵）。
- `scripts/i18n/Test-LanguageModes.ps1`──綠（13/13 ui-md3 i18n 測試加
  目錄、DeviceWeb 同舊網頁檢查）。
- 英文 MO 被重新生成決定性（3748 項）附帶同一個
  寫者佈局 yue_HK 編譯器使用；探針確認
  `墨水→墨水`、`添加墨水→添加墨水`、`同步 AMS→同步`、
  `AMS 設定→墨水機設定` 同零剩餘
  「墨水」/「AMS」字橫越所有已翻譯字串。

