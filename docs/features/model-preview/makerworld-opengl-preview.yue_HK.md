---
translation-of: makerworld-opengl-preview.md
source-sha256: 4ee379ee3fba5ec7956521a4475924923e08b79cd859ee6937da6a3989b551be
review-status: agent-drafted
---

> 英文原文：[MakerWorld OpenGL preview](makerworld-opengl-preview.md)

# MakerWorld OpenGL 預覽

## 行為

當一個 MakerWorld 模型通過「下載同開啟」流程被下載時，原生應用程式顯示一個互動 OpenGL 模型預覽，喺佢被匯入到準備之前。預覽係一個輕量 `wxGLCanvas` 面板（`ModelPreviewCanvas`），渲染一個單一中立遮罩網格帶著：

- 軌道旋轉（左拖）；
- 平移（右拖或中拖）；
- 滾輪縮放；同
- 符合檢視。

對話（`ModelPreviewDialog`）使用 MD3 對話解剖同提供兩個操作：

- **喺準備開啟**傳回 `wxID_OK` 同繼續現有匯入未變。
- **關閉**傳回 `wxID_CANCEL` 同跳過匯入。

預覽被線上作為預匯入鉤喺 `Plater::import_model_id`：幾何係首先提取，同對話只係被顯示，當一個非空網格可用時。

實作：`src/slic3r/GUI/ModelPreviewDialog.hpp` 同 `src/slic3r/GUI/ModelPreviewDialog.cpp`；鉤係喺 `src/slic3r/GUI/Plater.cpp`（`import_model_id`）。

## 設定

冇。預覽係由 MakerWorld 下載同開啟路徑觸發；冇應用程式設定鍵或使用者設定去設定佢。

## 失敗模式

- 幾何提取係最佳努力。`ModelPreviewDialog::load_geometry` 永遠唔拋；佢傳回 `false` 失敗或空網格時。
- 當幾何無法被提取時（或係空），對話被略過，同流貫穿到現有自動開啟匯入行為未變，所以預覽失敗永遠唔塊匯入。

## 驗證

- 喺釋放設定本地構建。呢個功能係本地提交 `8d727d49d` 嘅部分，喺撰寫時冇被推往主機 CI，所以冇主機執行証據仍然。
