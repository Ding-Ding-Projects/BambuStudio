---
translation-of: print-actions.md
source-sha256: 6e78a5ca50ddedd378345cd199817885d6c77c84b3413e7d8e887e58db36ff55
review-status: agent-drafted
---

> 英文原文：[Plate print actions and material mapping](print-actions.md)

# 打印盤操作同材料映射

準備操作欄提供 **Slice plate**、**Slice and print** 同 **Print plate** 分開。Slice and print 使用相同版本、材料同構建盤預檢 as Slice plate。佢開始當前盤嘅一個 slice，接著只打開打印設定當果個同一盤有成功、當前、可打印結果。佢從未提交工作。最後 **Print** 操作喺打印設定保持明確。

continuation 係一次性。唔同 slice、取消、失敗或未開始 slice、盤或項目替換或改變盤結果防止打印設定打開。當盤已經有完成、不變、可打印 slice，**Slice and print** 重用果個結果同打開設定一次冇等待完成事件。佢清除打印設定對話框出現之前。改變材料映射使切片結果無效同返回準備除非用戶選擇 **Swap and reslice**。

Custom material 頁面保留拖放同新增材料選擇器帶 **Move to left nozzle** 同 **Move to right nozzle** 控制項。**Swap groups** 交換両個噴嘴組進行檢查同時保留每個材料嘅流選擇。如果目標冇相容噴嘴或無法代表果個流選擇，swap 被拒絕喺改變任何組前。**Swap and reslice** 要求新鮮 slice 只在被接受交換通過噴嘴驗證之後。呢啲係項目同盤選擇；佢哋唔改變活實體打印或打印機嘅預設設定。

打印設定也放置每個材料旁邊移動按鈕到佢嘅當前左或右噴嘴卡。移動保持待定直到 **Swap** 或 **Swap and reslice** 被選擇，同 **Send** 無法使用待定映射。両個操作設定當前盤嘅 Custom 映射、使其舊 slice 無效同返回準備。**Swap and reslice** 呼叫相同檢查 Slice and print 操作如準備按鈕。打印設定只在成功當前 reslice 後重開，同提交仍需要最後 **Send** 點擊。現有打印選項保留喺重用對話框同佢嘅儲存選項設定；噴嘴相關映射係為新 slice 重新計算。

每個建議移動係檢查對抗安裝噴嘴容量、飾品/噴嘴相容性同目標噴嘴嘅可打印面積。一個不可用移動被禁用帶可見解釋。應用待定地圖重複果啲檢查對抗當前盤，所以盤或預設改變同時打印設定開啟無法發佈舊分配。

一個儲存自動映射偏好屬於選擇打印機預設。一個新項目只應用佢在重置後。喺後來打印機改變，偏好只應用同時項目仍然被擁有那個新項目路徑同每個盤繼承項目模式。一個明確項目映射選擇或匯入 3MF 設定清除那個所有權。一個明確盤模式，包括 Custom，也優先。手材料分配永遠從打印機偏好唯一重構。

匯入選單公開 Model Creator。命令調色盤索引相同選單命令。佢嘅驗證 STL 到達 `Plater::load_files` 只有在對話框嘅 **Add to plate** 操作之後。

聚焦 C++ 迴歸覆蓋儲存偏好優先同單次 slice continuation 謂詞。一個完整 Windows 構建、原生交互捕獲、真實打印機設定同硬件打印保持未驗證直到那啲檢查對抗構建應用程式執行。
