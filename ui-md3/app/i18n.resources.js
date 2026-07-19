/*
 * User-facing language resources for the standalone MD3 prototype.
 *
 * English source copy remains the lookup key so the original screen templates
 * stay readable and continue to run from file://.  The runtime deliberately
 * falls back to that English source whenever a Cantonese entry is absent.
 */
(function registerBambuI18nResources(global) {
  'use strict';

  var modes = [
    { id: 'en', label: 'English', htmlLang: 'en' },
    { id: 'yue_HK', label: '廣東話（香港）', htmlLang: 'yue-Hant-HK' },
    { id: 'bilingual_en_yue_HK', label: 'English + 廣東話', htmlLang: 'en' }
  ];

  // Values can be a string or an object. `display: "detail"` keeps lengthy
  // bilingual copy behind the small 廣 disclosure badge to prevent crowding.
  // `tone: "literal"` documents deliberately restrained safety/error wording.
  var yueHK = {
    // Application shell and navigation.
    'File': '檔案',
    'Edit': '編輯',
    'View': '檢視',
    'Objects': '物件',
    'Help': '幫手',
    'Home': '主頁',
    'Prepare': '準備',
    'Preview': '預覽',
    'Device': '裝置',
    'Multi-device': '多裝置',
    'Project': '項目',
    'Calibration': '校準',
    'Filament': '耗材',
    'Settings': '設定',
    'Appearance': '外觀',
    'General': '一般',
    'Presets': '預設',
    'Network': '網絡',
    'Version control': '版本控制',
    'About': '關於',
    'Version history': '版本記錄',
    'View history': '查看記錄',
    'Language': '語言',
    'Language mode': '語言模式',
    'Choose how interface text is shown': {
      text: '選擇介面文字點樣顯示',
      display: 'detail'
    },

    // Pages landing surface.
    'Screens': '畫面',
    "How it's built": '點樣整出嚟',
    'Bambu Studio — Material Design 3 concept': 'Bambu Studio — Material Design 3 概念版',
    'Bambu Studio · MD3 concept': 'Bambu Studio · MD3 概念版',
    'reimagined in': { text: '重新想像成', display: 'detail' },
    'Launch app': '開啟應用程式',
    'Launch the app': '開啟應用程式',
    'Toggle light / dark': '切換淺色／深色',
    'Independent concept redesign': '獨立概念重新設計',
    'A pure Material Design 3 re-skin of the real Bambu Studio slicer — every screen, faithfully restyled. Runs entirely in the browser: light & dark themes, a density toggle, and a customizable accent seed color that regenerates the whole tonal palette.': {
      text: '將真實 Bambu Studio 切片器完整換成 Material Design 3 外觀—每個畫面都忠實重新設計。全部在瀏覽器內運行，有淺色／深色主題、密度切換同可自訂強調色。',
      display: 'detail'
    },
    'Download native Windows app': '下載原生 Windows 應用程式',
    'View source': '查看原始碼',
    'Source': '原始碼',
    'Downloads': '下載',
    'Upstream project': '上游項目',
    'SHA-256 file': 'SHA-256 檔案',
    'release details': '發行詳情',
    'Unsigned native C++ Windows build · per-user installer ·': {
      text: '未簽署的原生 C++ Windows 版本 · 每位使用者獨立安裝 ·',
      display: 'detail', tone: 'literal'
    },
    '. The checksum confirms file integrity, not publisher identity.': {
      text: '。校驗碼只確認檔案完整性，不代表發行者身分。',
      display: 'inline', tone: 'literal'
    },
    'Accent → tonal ramp': '強調色 → 色調層級',
    'Every screen, restyled': '每個畫面都重新整過',
    'The full Bambu Studio information architecture — nothing invented, everything re-drawn in Material 3.': {
      text: '完整保留 Bambu Studio 資訊架構—沒有自行加功能，全部用 Material 3 重新繪製。',
      display: 'detail'
    },
    'Material You, all the way down': 'Material You，由頭到尾',
    'Theme, density, and accent are first-class. Pick a seed color and the app derives a full HSL tonal ramp for primary, containers, and state layers — live, with no reload.': {
      text: '主題、密度同強調色都係核心功能。選個種子色，應用程式就會即時生成完整 HSL 色調，無需重新載入。',
      display: 'detail'
    },
    'Try it live': '即時試玩',
    'Accent': '強調色',
    'A design imported from Claude, then implemented as real code by orchestrating the Codex CLI.': {
      text: '從 Claude 匯入設計，再由 Codex CLI 協作編排成真正程式碼。',
      display: 'detail'
    },
    '01 · Design': '01 · 設計',
    '02 · Runtime': '02 · 執行環境',
    '03 · Implement': '03 · 實作',
    'Imported the MD3 design': '匯入 MD3 設計',
    'Tiny vanilla runtime': '輕量原生執行環境',
    'Coded by Codex CLI': '由 Codex CLI 編寫',
    'A full desktop-shell Material 3 design of the real Bambu Studio UI — 9 screens, dialogs, tokens.': {
      text: '將真實 Bambu Studio 介面完整設計成 Material 3 桌面外殼—包括 9 個畫面、對話方塊同設計令牌。',
      display: 'detail'
    },
    "A dependency-free engine reimplements the design's template dialect — keyed patching that preserves focus & caret.": {
      text: '零依賴引擎重新實作設計樣板語法—用鍵值更新來保留焦點同插入點。',
      display: 'detail'
    },
    "Screens built in parallel across git worktrees, orchestrated agent-by-agent, then assembled and QA'd.": {
      text: '畫面分別在 Git worktree 並行建置，逐個代理編排，最後組裝同做品質檢查。',
      display: 'detail'
    },
    'Start a project, reopen recents, browse the model library.': '開新項目、重開最近檔案、去模型庫睇吓。',
    'The 3D editor — gizmo rail, scene tools, printer & process sidebar.': '立體編輯器—操作工具列、場景工具、列印機同工藝側邊欄。',
    'G-code viewer with color schemes, legend, layer & move sliders.': 'G-code 檢視器，包含顏色配置、圖例、層數同移動滑桿。',
    'Live camera, temperatures, fans, axis control, AMS & calibration.': '實時鏡頭、溫度、風扇、軸控制、AMS 同校準。',
    'Monitor every printer at a glance with progress and status.': '一眼睇晒每部列印機嘅進度同狀態。',
    'Project files — pictures, BOM, assembly guides and notes.': '項目檔案—圖片、物料清單、組裝指南同備註。',
    'Flow dynamics, flow rate, max volumetric speed, temp tower & more.': '流量動態、流量比、最大體積速度、溫度塔等等。',
    'Manage presets; filter, select and bulk-export filaments.': '管理預設；篩選、選擇同批量匯出耗材。',
    'Appearance, presets, network, version control and about.': '外觀、預設、網絡、版本控制同關於。',
    'An independent, open-source Material Design 3': {
      text: '一個獨立、開源的 Material Design 3',
      tone: 'literal'
    },
    'concept': '概念',
    'redesign built on a fork of the open-source Bambu Studio project. Not affiliated with, authorized by, or endorsed by Bambu Lab. “Bambu Studio”, “Bambu Lab”, and related names are trademarks of their respective owners; they are used here only to describe the software being restyled. “Material Design”, “Material You”, and “Roboto” are trademarks of Google LLC.': {
      text: '重新設計，建基於開源 Bambu Studio 項目的分支。本項目不隸屬 Bambu Lab，也未獲其授權或認可。「Bambu Studio」、「Bambu Lab」及相關名稱為各自權利人的商標，此處只用來說明所重新設計的軟件。「Material Design」、「Material You」及「Roboto」為 Google LLC 的商標。',
      display: 'detail', tone: 'literal'
    },

    // Shared appearance and search controls.
    'Theme': '主題',
    'Light': '淺色',
    'Dark': '深色',
    'Density': '密度',
    'Comfortable': '舒服',
    'Compact': '緊湊',
    'Accent color': '強調色',
    'Green': '綠色',
    'Violet': '紫羅蘭',
    'Teal': '青綠色',
    'Blue': '藍色',
    'Orange': '橙色',
    'Pink': '粉紅色',
    'Search': '搜尋',
    'Clear': '清除',
    'Regex builder': '正則表達式建構器',
    'Use regular expression': '使用正則表達式',
    'Insert token': '插入令牌',
    'Apply expression': '套用表達式',
    'Ignore case': '不分大小寫',
    'Whole word': '完整字詞',
    'Multiline': '多行',
    'Dotall': '點號包括換行',
    'valid': '有效',
    'invalid': { text: '無效', tone: 'literal' },
    'pattern': '模式',
    'any': '任何',
    'digit': '數字',
    'word': '字詞',
    'space': '空格',
    'start': '開頭',
    'end': '結尾',
    'bound': '邊界',
    'optional': '可選',
    'group': '群組',
    'set': '集合',
    'or': '或',
    'repeat': '重複',
    'non-cap': '不擷取',
    '1 or +': '1 次或以上',
    '0 or +': '0 次或以上',
    'literal .': '字面 .',
    'Search settings': '搜尋設定',
    'Search objects': '搜尋物件',
    'Search filaments': '搜尋耗材',
    'Search filament presets': '搜尋耗材預設',
    'Search calibrations': '搜尋校準項目',
    'Search commits': '搜尋提交記錄',
    'Search devices': '搜尋裝置',
    'Search files': '搜尋檔案',
    'Search models on MakerWorld': '去 MakerWorld 搜尋模型',
    'Name containing… (supports regex)': '名稱包含…（支援正則表達式）',

    // Settings. Privacy/security-related copy intentionally stays literal.
    'Auto-arrange on import': '匯入時自動排列',
    'Automatically arrange new objects on the plate': {
      text: '自動將新物件排列在列印板上',
      display: 'detail'
    },
    'Auto-commit every edit to Git': '每次編輯自動提交到 Git',
    'Save each undoable change to the project repository': {
      text: '將每項可復原的變更儲存到項目儲存庫',
      display: 'detail', tone: 'literal'
    },
    'Show “auto-saved” notifications': '顯示「已自動儲存」通知',
    'Pop a snackbar each time an edit is committed': {
      text: '每次提交編輯時顯示底部通知',
      display: 'detail'
    },
    'Bundle version history into .3mf': '將版本記錄包含在 .3mf 內',
    'Include the local Git repo when saving a project': {
      text: '儲存項目時包含本機 Git 儲存庫',
      display: 'detail', tone: 'literal'
    },
    'Show daily tips': '顯示每日小貼士',
    'Display slicing tips on launch': '啟動時顯示切片小貼士',
    'Share anonymous usage data': {
      text: '分享匿名使用資料',
      tone: 'literal'
    },
    'Help improve Bambu Studio': {
      text: '幫助改善 Bambu Studio',
      tone: 'literal'
    },

    // Home and project entry points.
    'Welcome back': '返嚟喇',
    'Start a new project, open an existing one, or continue where you left off.': {
      text: '開個新項目、打開舊檔，或者接住上次繼續整。',
      display: 'detail'
    },
    'New Project': '新項目',
    'Open project': '打開項目',
    'Resume a .3mf project': '繼續整 .3mf 項目',
    'Explore models': '去模型庫尋寶',
    'Browse the online library': '去網上模型庫睇吓',
    'Recent projects': '最近項目',
    'Open': '打開',
    'Import model': '匯入模型',
    'Today': '今日',
    'Yesterday': '尋日',
    'Last week': '上星期',

    // Prepare screen.
    'Printer': '列印機',
    'Process': '工藝',
    'Quality': '品質',
    'Strength': '強度',
    'Speed': '速度',
    'Support': '支撐',
    'Advanced settings': '進階設定',
    'Layer height': '層高',
    'Wall loops': '牆循環',
    'Top shell layers': '頂層層數',
    'Sparse infill density': '稀疏填充密度',
    'Infill pattern': '填充圖案',
    'Enable support': '啟用支撐',
    'Disable support': '停用支撐',
    'Brim type': '邊緣類型',
    'Grid': '網格',
    'Gyroid': '螺旋體',
    'Auto': '自動',
    'Object manipulation': '物件調整',
    'Position': '位置',
    'Move': '移動',
    'Rotate': '旋轉',
    'Rotation': '旋轉',
    'Scale': '縮放',
    'Size': '大小',
    'Slice plate': '切片列印板',
    'Send print': '傳送列印',
    'Plate 1': '列印板 1',
    'Add object': '新增物件',
    'Add plate': '新增列印板',
    'Auto orient': '自動定向',
    'Arrange all': '排列全部',
    'Variable layer height': '可變層高',
    'Split to objects': '分割成物件',
    'Assembly view': '組裝檢視',
    'More tools': '更多工具',
    'Move (M)': '移動 (M)',
    'Rotate (R)': '旋轉 (R)',
    'Scale (S)': '縮放 (S)',
    'Place on face': '平面著地',
    'Cut (C)': '切割 (C)',
    'Mesh Boolean': '網格布爾運算',
    'Measure': '測量',
    'Assembly': '組裝',
    'Support painting': '繪製支撐',
    'Seam painting': '繪製接縫',
    'Color painting': '繪製顏色',
    'Text': '文字',
    'Brim ears': '邊緣耳',
    'Fuzzy skin': '模糊外皮',
    'Simplify mesh': '簡化網格',
    'Textured PEI Plate': '紋理 PEI 列印板',
    'Bed type': '底板類型',

    // Preview screen.
    'Color scheme': '顏色配置',
    'Line type': '線條類型',
    'Layer time': '層時間',
    'Flow': '流量',
    'Temperature': '溫度',
    'Outer wall': '外牽',
    'Inner wall': '內牽',
    'Sparse infill': '稀疏填充',
    'Internal solid infill': '內部實心填充',
    'Top surface': '頂層表面',
    'Bridge': '橋接',
    'Support interface': '支撐介面',
    'Gap infill': '空隙填充',
    'Prime tower': '擦料塔',
    'Travel': '空走',
    'Seams': '接縫',
    'Retractions': '回抽',
    'Wipe': '擦料',
    'Layers': '層數',
    'Layers & height range': '層數及高度範圍',
    'Options': '選項',
    'Statistics': '統計',
    'Estimated time': '預計時間',
    'Filament used': '耗材用量',
    'Est. cost': '預計成本',
    'Print options': '列印選項',
    'Print Plate 1': '列印第 1 塊板',

    // Device and multi-device screens.
    'Monitor & control printers': '監控同操作列印機',
    'My devices': '我嘅裝置',
    'Add device': '新增裝置',
    'Device farm': '裝置群組',
    'LIVE': '實時',
    'Connected': '已連線',
    '● Connected · Idle': '● 已連線 · 閒置',
    'Idle': '閒置',
    'Printing': '列印中',
    'Offline': '離線',
    'Nozzle': '噴嘴',
    'Bed': '底板',
    'Chamber': '機箱',
    'Chamber light': '機箱燈',
    'Part cooling': '部件散熱',
    'Aux fan': '輔助風扇',
    'Extrude': '擠出',
    'AMS mapping': 'AMS 映射',
    'Sync AMS': '同步 AMS',
    'Sync with AMS': '同 AMS 同步',
    'Humidity: Dry': '濕度：乾燥',
    'Bed leveling': '底板調平',
    'Timelapse': '延時攝影',
    'Silent': '寧靜',
    'Standard': '標準',
    'Sport': '加速',
    'Ludicrous': '超高速',
    'Pause': '暫停',
    'Stop': { text: '停止', tone: 'literal' },

    // Project, calibration, and filament management.
    'Model pictures': '模型圖片',
    'History': '記錄',
    'Bill of materials': '物料清單',
    'Assembly guide': '組裝指南',
    'Others': '其他',
    'Add file': '新增檔案',
    'Add to project': '加入項目',
    'Fine-tune your printer and filament for the best possible print quality.': {
      text: '幫部機同耗材校到啱啱好，印出嚟靚仔啲。',
      display: 'detail'
    },
    'Flow dynamics calibration': '流量動態校準',
    'Flow Dynamics': '流量動態',
    'Calibrate pressure advance for sharp corners': '校準壓力預控，改善尖角',
    'Flow Rate': '流量比',
    'Tune extrusion multiplier for accurate walls': '調整擠出倍率，使牽寬更準確',
    'Max Volumetric Speed': '最大體積速度',
    'Find the fastest reliable flow for a filament': '找出此耗材穩定的最高流量',
    'Temperature Tower': '溫度塔',
    'Find the ideal nozzle temperature': '找出理想噴嘴溫度',
    'Retraction Test': '回抽測試',
    'Reduce stringing and oozing': '減少拉絲同滲料',
    'Vertical Fine Tuning': 'Z 軸精細調整',
    'Correct Z-offset and first layer': '修正 Z 偏移及首層',
    'Filament Manager': '耗材管理器',
    'Filter, select and bulk-export presets': '篩選、選擇同批量匯出預設',
    'Export all': '全部匯出',
    'Import': '匯入',
    'New filament': '新耗材',
    'Vendor': '供應商',
    'Type': '類型',
    'Nozzle °C': '噴嘴 °C',
    'Bed °C': '底板 °C',
    'Export filament': '匯出耗材',
    'Add filament': '新增耗材',
    'Pick a preset to add to this project': '選個預設加入項目',
    'Export filaments': '匯出耗材',
    'Format': '格式',
    'Select all': '全選',
    'No filaments match your filter.': {
      text: '沒有耗材符合篩選條件。',
      display: 'inline', tone: 'literal'
    },
    'All': '全部',
    'ZIP bundle': 'ZIP 壓縮包',

    // Dialogs, history, and common actions.
    'Cancel': '取消',
    'Apply': '套用',
    'Add': '新增',
    'Save': '儲存',
    'Send': '傳送',
    'Export': '匯出',
    'Restore': { text: '還原', tone: 'literal' },
    'Restore this version': { text: '還原此版本', tone: 'literal' },
    'Changes in this commit': '此提交的變更',
    'Auto-commit is on — every edit is saved automatically.': {
      text: '已啟用自動提交—每次編輯都會自動儲存。',
      display: 'detail', tone: 'literal'
    },
    'This repository is bundled into the .3mf when you save.': {
      text: '儲存時，此儲存庫會包含在 .3mf 內。',
      display: 'detail', tone: 'literal'
    },
    'Every edit is auto-committed to this project’s local Git repo': {
      text: '每次編輯都會自動提交到此項目的本機 Git 儲存庫',
      display: 'detail', tone: 'literal'
    },
    "Version history — every edit is auto-saved to this project's Git repo": {
      text: '版本記錄—每次編輯都會自動儲存到此項目的 Git 儲存庫',
      display: 'detail', tone: 'literal'
    },
    'Send to printer over the network': {
      text: '透過網絡傳送到列印機',
      display: 'detail', tone: 'literal'
    },
    'Send to printer': '傳送到列印機',
    'Print plate': '列印此板',
    'Show file': '顯示檔案',
    'Docs': '文件',
    'Action': '操作',
    'Faces': '面數',
    'Rotation Z': 'Z 軸旋轉',
    'Size Z': 'Z 軸大小',
    'Position X': 'X 軸位置',
    'Position Y': 'Y 軸位置',
    'You': '你',
    'just now': '啱啱',
    'created': '已建立',
    'Initialize project repository': '初始化項目儲存庫',
    'Repository': '儲存庫',
    'Set sparse infill density → 15%': '將稀疏填充密度設為 15%',
    'Rotate 3DBenchy 45° around Z': '將 3DBenchy 繞 Z 軸旋轉 45°',
    'Scale 3DBenchy 100% → 120%': '將 3DBenchy 由 100% 縮放到 120%',
    'Move 3DBenchy to plate center': '將 3DBenchy 移到列印板中心',
    'Add 3DBenchy.stl': '新增 3DBenchy.stl',
    'Auto-orient objects': '自動調整物件方向',
    'Arrange plate': '排列列印板',
    'Edit variable layer height': '編輯可變層高',
    'Slice Plate 1': '切片列印板 1',
    'Add filament: Bambu PLA Basic': '新增耗材：Bambu PLA Basic',
    'Plate 1 · snapshot': '列印板 1 · 快照',
    'by you · 3 objects': '由你製作 · 3 個物件',
    '1 object · 15,842 faces': '1 個物件 · 15,842 個面',
    '0.4 nozzle · Connected': '0.4 噴嘴 · 已連線'
  };

  // Patterns cover visible text whose numbers/names are interpolated by the
  // original templates. Captures are substituted using JavaScript's $1 form.
  var patterns = [
    { source: '^(\\d+) of (\\d+) selected$', yue: '已選 $1 / $2 項' },
    { source: '^Export (\\d+)$', yue: '匯出 $1 項' },
    { source: '^Sliced · (\\d+) layers$', yue: '已切片 · $1 層' },
    { source: '^(\\d+) commits · auto-saved\\. Bundled into the \\.3mf on save\\.$', yue: '$1 個提交 · 已自動儲存，儲存時會包含在 .3mf 內。', display: 'detail', tone: 'literal' },
    { source: '^(.+) · (\\d+) commits$', yue: '$1 · $2 個提交' },
    { source: '^AMS · Slot (\\d+)$', yue: 'AMS · 槽位 $1' },
    { source: '^Today · (.+)$', yue: '今日 · $1' },
    { source: '^Yesterday · (.+)$', yue: '尋日 · $1' },
    { source: '^(\\d+) days ago · (.+)$', yue: '$1 日前 · $2' },
    { source: '^Last week · (.+)$', yue: '上星期 · $1' },
    { source: '^(\\d+) min ago$', yue: '$1 分鐘前' },
    { source: '^(\\d+) days ago$', yue: '$1 日前' },
    { source: '^Layer (\\d+) / (\\d+) · (.+) remaining$', yue: '第 $1 / $2 層 · 剩餘 $3' },
    { source: '^Move (\\d+) / (\\d+)$', yue: '移動 $1 / $2' },
    { source: '^(\\d+) objects? · ([\\d,]+) faces$', yue: '$1 個物件 · $2 個面' },
    { source: '^(.+) nozzle · Connected$', yue: '$1 噴嘴 · 已連線' },
    { source: '^(.+) Standard @(.+)$', yue: '$1 標準 @ $2' },
    { source: '^Restored project to #(.+)$', yue: '項目已還原到 #$1', tone: 'literal' },
    { source: '^Starting (.+) calibration…$', yue: '正在開始 $1 校準…' },
    { source: '^Auto-saved: (.+)$', yue: '已自動儲存：$1', tone: 'literal' },
    { source: '^Exported (\\d+) filament presets? → (.+)$', yue: '已匯出 $1 個耗材預設 → $2', tone: 'literal' }
  ];

  var messages = {
    viewMenu: { en: 'View menu', yue: '檢視選單' },
    objectsMenu: { en: 'Objects menu', yue: '物件選單' },
    slicingPlate: { en: 'Slicing Plate {plate}…', yue: '正在切片列印板 {plate}…' },
    plateSliced: { en: 'Plate {plate} sliced · {time} · {weight}', yue: '列印板 {plate} 已切片 · {time} · {weight}' },
    projectSaved: { en: 'Project saved — version history bundled into .3mf', yue: '項目已儲存—版本記錄已包含在 .3mf 內' },
    exportedPresets: { en: 'Exported {count} filament preset{suffix} → {format}', yue: '已匯出 {count} 個耗材預設 → {format}' },
    restoredProject: { en: 'Restored project to #{hash}', yue: '項目已還原到 #{hash}' },
    printSent: { en: 'Sent to {printer} · print starting', yue: '已傳送到 {printer} · 即將開始列印' },
    openingLibrary: { en: 'Opening online model library…', yue: '正在打開網上模型庫…' }
  };

  global.BAMBU_I18N_RESOURCES = Object.freeze({
    defaultMode: 'en',
    storageKey: 'bambuStudio.languageMode.v1',
    modes: Object.freeze(modes.map(function(mode) { return Object.freeze(mode); })),
    catalog: Object.freeze({ yue_HK: Object.freeze(yueHK) }),
    patterns: Object.freeze(patterns.map(function(pattern) { return Object.freeze(pattern); })),
    messages: Object.freeze(messages)
  });
})(typeof window !== 'undefined' ? window : globalThis);
