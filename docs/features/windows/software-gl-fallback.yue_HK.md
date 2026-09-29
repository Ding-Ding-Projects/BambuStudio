---
translation-of: software-gl-fallback.md
source-sha256: 22a18772b3ccf82d1c8a088e8638df2ae3952e15e2a027a2bfddcc2b5a95cc8a
review-status: agent-drafted
---

> 英文原文：[Software OpenGL fallback (Mesa llvmpipe)](software-gl-fallback.md)

# 軟件 OpenGL 後備（Mesa llvmpipe）

Bambu Studio MD3 需要 OpenGL 2.0。喺機器上面 display driver 無法提供（最常見係虛擬機執行喺「Microsoft Basic Display Adapter」上面），舊版本顯示「Unsupported OpenGL version」錯誤同出佗，用戶報告做「應用無法啟動」。Windows 安裝依家自己修復喺呢啲機器通過自動切換到 Mesa 嘅 llvmpipe 軟件光柵化器。

## 行為

- 安裝程式喺安裝資料夾嘅**惰性** `mesa\` 子資料夾到兩個 Mesa DLL：`mesa\opengl32.dll` 同 `mesa\libgallium_wgl.dll`。當佢哋保持喺呢個子資料夾到佢哋冇效果。
- 喺啟動，當建立 OpenGL 內容報告版本低於 2.0 時（`OpenGLManager::init_gl`、`src/slic3r/GUI/OpenGLManager.cpp`），應用：
  1. 檢查 `BBS_SOFTGL_RETRIED` 環境變量係**唔係**設定（重啟迴圈守護），
  2. 檢查兩個 DLL 存在喺 `mesa\` 子資料夾旁邊 `bambu-studio.exe`，
  3. 檢查可執行檔資料夾係可寫，
  4. 複製兩個 DLL**旁邊** `bambu-studio.exe`（`libgallium_wgl.dll` 首先，所以一個半複製對唔會被留後），同
  5. 用相同命令列重啟自己加上環境 `BBS_SOFTGL_RETRIED=1`、`GALLIUM_DRIVER=llvmpipe`、`MESA_GL_VERSION_OVERRIDE=3.3`、`LIBGL_ALWAYS_SOFTWARE=1`，然後出佗。
- 重新啟動進程載入 Mesa `opengl32.dll` 代替系統一個：Windows 解析一個靜態連接 `opengl32.dll` 從可執行檔資料夾首先，同完整 UI 喺軟件入面渲染。
- 當任何前提條件失敗，或重試進程仲係睇到 OpenGL 低於 2.0，熟悉錯誤對話框出現，擴展帶著一句話解釋軟件渲染後備失敗或者無法使用。
- 當應用執行喺 llvmpipe 佢記錄 `OpenGL renderer is Mesa llvmpipe (software rasterizer); the UI renders without GPU acceleration.` 喺資訊等級。冇故意 UI 騷擾。

## 何時觸發

只係喺 Windows，只當 OpenGL 版本門（< 2.0）開火，同只係一次每啟動鏈：重新啟動進程帶著 `BBS_SOFTGL_RETRIED=1` 喺佢嘅環境，所以佢唔可以永遠產生另一代，同進程本地旗標限制企圖一次每進程即使 GL 初始化被重新進入。機器帶著工作 GPU 驅動永唔達到呢個代碼任何。

## 點樣撤銷

刪除兩個複製 DLL 旁邊可執行檔：

- `<install dir>\opengl32.dll`
- `<install dir>\libgallium_wgl.dll`

下一個開始使用系統 OpenGL 驅動再次（同會重新觸發後備如果驅動仲係缺少 OpenGL 2.0）。`mesa\` 子資料夾自己係無害同被卸載程式移除，加上兩個執行時複製如果後備曾經開火（生成卸載清單喺 `packaging/windows/GenerateUninstallInclude.ps1` 擁有兩個）。

## 安全考慮

- Mesa 二進制文件嚟自上游 [pal1000/mesa-dist-win](https://github.com/pal1000/mesa-dist-win) 項目，釘住喺 `.github/workflows/build_bambu.yml` 到發佈**26.1.3**（`mesa3d-26.1.3-release-msvc.7z`），針對記錄 SHA-256 用於存檔**同**用於每一兩個提取 x64 DLL 驗證。一個雜湊不匹配失敗建立，所以被篡改或靜靜替換上游資產永唔可以進入有效載荷。
  - 存檔 SHA-256：`6dd431f4620cea73970b13e3ffa94f721f2a3924306b8a4283c97648cdb6eb9c`
  - `x64\opengl32.dll` SHA-256：`12499866437a161d2b250d5105188ae00732dd74b4bebbcdf972e6145af00f9e`
  - `x64\libgallium_wgl.dll` SHA-256：`1895f8c19ede5efd0497f9dfab463b19bf4377e3af7c06c2d4d073e4680c5f69`
- 複製步驟永唔覆蓋無一切關係檔案：佢只建立 `opengl32.dll` / `libgallium_wgl.dll` 旁邊可執行檔，喺每用戶安裝資料夾內部。
- 有效載荷 SBOM（`New-WindowsCycloneDxSbom.ps1`）記錄船上 `mesa/` 檔案帶著佢哋嘅雜湊好像每個其他有效載荷檔案。

## 失敗模式

| 情況 | 結果 |
| --- | --- |
| `mesa\` DLL 缺失（例如便攜式 unzip 唔帶著資料夾） | 錯誤對話框帶著「後備無法使用」句子。 |
| 安裝資料夾唔可寫 | 錯誤對話框帶著「後備無法使用」句子。 |
| 重啟自己失敗（`CreateProcessW` 錯誤） | 環境恢復；錯誤對話框帶著「後備無法使用」句子；複製 DLL 保持同喺下一個手工開始採取效果。 |
| 重試進程仲係報告 OpenGL < 2.0 | 錯誤對話框帶著「已經試過」句子；無進一步重啟。 |

## 確認

- CI（`build_bambu.yml`、步驟「Stage Mesa software OpenGL fallback payload」）下載釘住存檔、驗證所有三個雜湊同階段兩個 x64 DLL 進入 `install-dir\mesa\` 先至 SBOM 生成、便攜式 zip 同 NSIS 包裝：所以安裝程式測試通過練習安裝同卸載 `mesa\` 有效載荷自動。
- 手工証明喺虛擬機帶著 Microsoft Basic Display Adapter：放相同兩個 DLL 旁邊 `bambu-studio.exe` 帶著上面環境渲染完整 UI 通過 llvmpipe。
