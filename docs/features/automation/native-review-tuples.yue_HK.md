---
translation-of: native-review-tuples.md
source-sha256: 21eba587553b43c049dd6e33cd99f5a16b7e1124e37c9e544a3afb95fce8f1d7
review-status: agent-drafted
---

# 原生檢查組合清單

[English](native-review-tuples.md)

`scripts/md3/review-tuples.py` 為
[實際介面檢查隊列](../../../design/workflow-refresh/built-review-queue.md)
嘅九個範圍準備組合。佢唔會啟動程序、改偏好或主機設定、輸入操作、擷取畫面、
接受檢查或提升證據。結果只會係待觀察或明確不符，**任何比較都唔會變成介面通過證明**。

## 要求值同觀察值

每個範圍有 192 個要求組合，總共 1,728 個：

| 欄位 | 要求值 |
| --- | --- |
| `language` | `en`、`yue_HK`、`bilingual_en_yue_HK` |
| `theme` | `light`、`dark` |
| `scalePercent` | 100、125、150、200 |
| `density` | `comfortable`、`compact` |
| `motion` | `full`、`reduced` |
| `geometry` | `normal`、`minimum` |

清單只記錄範圍層級意圖，唔係現成資料或逐個狀態嘅覆蓋。各範圍實際可到達狀態
仍須按隊列展開。產生清單會核對 `remainingVisualCoverage` 嘅完整範圍次序；
合約改咗就停止，唔會自行更新來源或完成旗標。

使用現有 Python 3：

```text
python scripts/md3/review-tuples.py inventory
python scripts/md3/review-tuples.py compare --requested requested.json --probe native.jsonl --pid 123 --hwnd 456 --tag owned-observation
```

`requested.json` 係一行清單內嘅 `requested` 物件，唔係成行。PID、HWND 同標籤
必須由當次驅動工作階段獨立掌握，唔可以信輸入檔案自己宣稱嘅身分。省略 `--probe`
會回傳待觀察同 `nativeProbe` 不可用。工具只輸出 JSON，唔寫檔。退出碼 0 只代表
清單產生成功；1 係輸入無效；2 係已知不符；3 係仍待觀察。全部都唔代表視覺驗收。

比較沿用[原生版面探針](../design-system/layout-probe.md)嘅 NDJSON 格式。
必須有唯一標頭、唯一吻合而可見嘅頂層目標、最後 `end` 記錄、吻合 PID 同標籤、
有限正數比例，以及正整數客戶區尺寸。要求值同觀察值分開保存。重複 JSON 鍵、
非有限 JSON 常數會被拒絕；每檔最多 16 MiB。輸出唔會複製原始標籤。

比較直接重用 `local-native-review.py::validate_probe`，傳入獨立要求嘅
`expected_language`、`expected_theme`、`expected_density` 關鍵字參數。
預設值仍然係 `en`、`light`、`comfortable`，所以原有初始介面驅動仍要求完全
相同設定。絕不改寫原始標頭令驗證器通過。不符模式會同吻合觀察分開報告，
兩者都唔會產生完整組合驗收。

輸出**唔係第一版版面收據**。共用 `validate-layout-probe.mjs` 合約要求來源綁定
產物、畫面、擁有權及私隱聲明，同指定背景路線嘅元素計算量度。
單一原生探針唔足夠，工具唔會偽造 DOM 量度、畫面、來源身分或收據。
輸入身分只供關聯核對，唔係當下擁有權證明。

## 幾何同目前缺乏嘅觀察

一般尺寸係 1200 × 800 DIP。比較目標乘以觀察比例後嘅原生客戶區尺寸，
只容許半個原生像素嘅整數取整差異。尺寸被程式最低限制夾住係不符，
唔可以靜靜改寫要求值。

最低尺寸來自 `GUI_App::get_min_size()`：
`max(1000,76*em)` × `max(600,49*em)`，單位係原生尺寸。
`minimum_geometry(measured_em)` 只接受實測有限正數，拒絕布林、零、負數、
無限值或溢出。例如獨立實測原生 `em` 為 20，結果係 1520 × 980，唔係
1000 × 600。工具唔會用檔名、估計字體、比例、畫面或要求值推算 `em`。

現有探針冇原生 `em`、實際最低分配規則或有效動態策略。因此即使可觀察欄位吻合，
`measuredNativeEm`、`minimumGeometry`、`effectiveMotion` 仍然不可用。
輸入額外同名欄位唔會變成可信量度。純最低尺寸計算留畀未來實測配接器使用，
未有真正觀察合約之前，唔會接去驗收。

探針比例取自第一個頂層視窗，其他螢幕上嘅對話框需要獨立目標 DPI 證據。
工具未能證明呢點。原生外框亦唔能夠證明嵌入 DOM 溢出、ImGui 內部幾何、
文字排版、焦點、狀態轉移或私隱。帳戶、裝置及資料不足仍係個別範圍嘅限制。

## 未來本機驅動整合

整合位置係 `inspect_shell()` 擁有嘅工作階段內，來源同目前程序／視窗身分核對
之後、`finally` 收尾之前。初始介面命令唔會留下可恢復程序。未來驅動必須另行取得
實際操作授權，觀察當下本地化控制名稱及目標身分，唔可以重播舊座標。

1. 用真實偏好語言選單切換，觀察原生標頭及控制。出現重新載入提示就按實際流程處理；
   保存設定唔代表文字已套用。
2. 用外觀嘅主題、密度控制切換，等排版穩定後觀察實際值。需要時喺可棄置工作階段
   還原設定，唔修改使用者設定檔。
3. 用 `Interface motion` 控制。選項係 `System` 同 `Reduce motion`；
   作業系統可能要求減少動態，所以 `System` 唔保證完整動態。未來配接器必須觀察
   有效策略，選中一行唔夠。
4. 先量度目標 DPI 同原生 `em`，再用真實視窗操作要求尺寸並讀回實際分配。
   比例唔符就記錄不符或不可用，唔改主機顯示、主題或無障礙設定去填滿清單。
5. 每個實際隊列狀態都取得新完整探針再比較。來源／建置綁定、原生操作歷史及另行
   授權畫面沿用各自合約。組合欄位吻合，唔代表個別範圍或整個檢查完成。

## 驗證

`python -m unittest discover -s scripts/md3/tests -p test_review_tuples.py -v`
核對 1,728 行清單、現有驗證器重用、數值拒絕、身分及完整性、個別不符同缺乏觀察。
測試全部用離線合成輸入，唔會產生執行、視覺或硬件證據。
