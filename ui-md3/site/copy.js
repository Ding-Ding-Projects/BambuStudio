/*
 * Copy catalog for the GitHub Pages site.
 *
 * Every entry carries English and Hong Kong Cantonese text. Each language holds
 * a tone ladder addressed by that language's own funny level (1 = fully serious,
 * 5 = maximum playfulness); the two ladders are independent, so English can be
 * dead straight while Cantonese is at full volume.
 *
 * Ladder lengths are deliberately allowed to vary and are expanded like this:
 *   5 variants -> levels 1,2,3,4,5 map one to one
 *   3 variants -> levels 1-2 use [0], level 3 uses [1], levels 4-5 use [2]
 *   2 variants -> levels 1-2 use [0], levels 3-5 use [1]
 *   1 variant  -> the same text at every level (proper nouns, legal text, and
 *                 atomic control labels that have no voice to vary)
 *
 * The rule the whole catalog obeys: the funny level changes VOICE, never FACTS.
 * Every warning still names what is unsigned, what is deleted, what is sent, and
 * what the user's options are, at level 5 exactly as at level 1.
 */
(function (global) {
  'use strict';

  var copy = {
    /* ---------------------------------------------------------------- shell */
    'shell.brand': { en: ['Bambu Studio'], yue: ['Bambu Studio'] },
    'shell.tagline': { en: ['Material Design 3'], yue: ['Material Design 3'] },
    'shell.skip': { en: ['Skip to content'], yue: ['跳去內容'] },
    'shell.launch': {
      en: ['Launch app', 'Launch the app', 'Launch it'],
      yue: ['開啟應用', '開個 App 嚟玩', '撳落去玩啦']
    },
    'shell.theme.toggle': {
      en: ['Toggle light / dark'],
      yue: ['切換淺色 / 深色']
    },
    'shell.language': { en: ['Language mode'], yue: ['語言模式'] },
    'shell.notifications': {
      en: ['Notifications'],
      yue: ['通知']
    },
    'shell.notifications.empty': {
      en: [
        'No notifications yet.',
        'Nothing has happened worth telling you about yet.',
        'Empty. Go press something and I will start gossiping.'
      ],
      yue: [
        '暫時未有通知。',
        '未有嘢值得通知你。',
        '一片空白。你撳多幾嘢，我就有得八卦。'
      ]
    },
    'shell.notifications.clear': { en: ['Clear all'], yue: ['清除全部'] },
    'shell.notifications.dismiss': { en: ['Dismiss this notification'], yue: ['關閉呢個通知'] },
    'shell.tabsearch': {
      en: ['Search tabs', 'Find a tab', 'Where did that tab go?'],
      yue: ['搜尋分頁', '搵分頁', '嗰個分頁去咗邊？']
    },
    'shell.overflow': {
      en: ['More tabs', 'More tabs', 'The tabs that would not fit']
    ,
      yue: ['更多分頁', '仲有分頁', '塞唔落嘅分頁喺呢度']
    },
    'shell.tabmenu': { en: ['Tab actions'], yue: ['分頁操作'] },
    'shell.pin': { en: ['Pin tab'], yue: ['釘住分頁'] },
    'shell.unpin': { en: ['Unpin tab'], yue: ['解除釘住'] },
    'shell.moveleft': { en: ['Move left'], yue: ['向左移'] },
    'shell.moveright': { en: ['Move right'], yue: ['向右移'] },
    'shell.resettabs': { en: ['Reset tab order'], yue: ['還原分頁次序'] },
    'shell.group': { en: ['Group'], yue: ['分組'] },

    /* --------------------------------------------------------------- tabs */
    'tab.overview': { en: ['Overview'], yue: ['總覽'] },
    'tab.screens': { en: ['Screens'], yue: ['介面'] },
    'tab.materialyou': { en: ['Material You'], yue: ['Material You'] },
    'tab.download': { en: ['Download'], yue: ['下載'] },
    'tab.changelog': { en: ['Changelog'], yue: ['更新日誌'] },
    'tab.regex': { en: ['Regex lab'], yue: ['正則實驗室'] },
    'tab.build': { en: ['How it is built'], yue: ['點樣砌出嚟'] },
    'tab.settings': { en: ['Settings'], yue: ['設定'] },

    'group.product': { en: ['Product'], yue: ['產品'] },
    'group.tools': { en: ['Tools'], yue: ['工具'] },
    'group.project': { en: ['Project'], yue: ['專案'] },

    /* ------------------------------------------------------------ overview */
    'overview.eyebrow': {
      en: [
        'Independent concept redesign',
        'Independent concept redesign',
        'An unofficial redesign, done properly',
        'Nobody asked for this redesign. We did it anyway.',
        'Nobody asked for this redesign. We did it anyway, and it looks great.'
      ],
      yue: [
        '獨立概念重新設計',
        '獨立概念重新設計',
        '非官方重新設計，但做得認真',
        '無人叫我哋改，我哋照改。',
        '無人叫我哋改，我哋照改，仲要改到靚仔。'
      ]
    },
    'hero.headline': {
      en: [
        'Bambu Studio, rebuilt on Material Design 3.',
        'Bambu Studio, rebuilt on Material Design 3 — every screen.',
        'Bambu Studio, wearing Material You.',
        'Bambu Studio went for a Material You makeover and kept the receipts.',
        'Bambu Studio walked into a Material You salon and walked out with cheekbones.'
      ],
      yue: [
        'Bambu Studio 全面改用 Material Design 3。',
        'Bambu Studio 成個介面轉晒 Material Design 3。',
        'Bambu Studio 換咗 Material You 新衫。',
        'Bambu Studio 去咗做 Material You facial，靚咗好多。',
        'Bambu Studio 入咗 Material You 髮型屋，出返嚟連骨相都靚埋。'
      ]
    },
    'hero.lede': {
      en: [
        'A Material Design 3 restyle of the Bambu Studio slicer. It runs entirely in the browser: light and dark themes, a density toggle, and an accent seed colour that regenerates the whole tonal palette.',
        'A Material Design 3 restyle of the Bambu Studio slicer — every screen, faithfully rebuilt. Runs entirely in the browser: light and dark, a density toggle, and an accent seed colour that regenerates the whole tonal palette.',
        'Every screen of the Bambu Studio slicer, restyled in Material Design 3 and running in your browser. Flip the theme, squeeze the density, pick a seed colour and watch the entire palette follow along.',
        'We took every screen of the slicer, put it through Material Design 3, and left it running in your browser. Flip light to dark, squeeze the density, throw a seed colour at it and the whole palette obediently regenerates.',
        'Every single screen of the slicer got the Material Design 3 treatment, and it all runs in your browser with no install. Flip light to dark, squash the density, lob a seed colour at it and the entire palette rearranges itself like it was waiting to be asked.'
      ],
      yue: [
        'Bambu Studio 切片軟件嘅 Material Design 3 改造版，完全喺瀏覽器行：淺色深色、密度切換，仲有種子色一改就重新生成成套色階。',
        'Bambu Studio 切片軟件嘅 Material Design 3 改造版，每一版都照住重砌，完全喺瀏覽器行：淺色深色、密度切換，種子色一改就重新生成成套色階。',
        '成個切片軟件嘅介面都用 Material Design 3 重新設計，喺瀏覽器就行得。轉主題、收密度、揀隻種子色，成套色階即刻跟住變。',
        '我哋將每一版介面掉入 Material Design 3 度洗一次，然後放喺瀏覽器等你玩。淺轉深、密度收窄、掟隻種子色入去，成套色階乖乖哋自己重生。',
        '每一版介面都做過 Material Design 3 大改造，喺瀏覽器直接行，唔使裝。淺轉深、密度收到貼晒、隨手掟隻種子色入去，成套色階即刻自動執位，好似等咗你好耐咁。'
      ]
    },
    'hero.cta.launch': {
      en: ['Launch the app', 'Launch the app', 'Open the prototype', 'Go press things', 'Go press things, nothing breaks'],
      yue: ['開啟應用', '開啟應用', '開個原型嚟睇', '入去撳嘢啦', '入去亂撳都唔會爆']
    },
    'hero.cta.download': {
      en: ['Download for Windows', 'Download the Windows app', 'Get the real Windows build'],
      yue: ['下載 Windows 版', '下載 Windows 應用', '攞真身 Windows 版']
    },
    'hero.cta.source': {
      en: ['View source', 'View source', 'Read the code'],
      yue: ['查看原始碼', '查看原始碼', '睇下啲 code']
    },
    'hero.integrity': {
      en: [
        'The Windows build is unsigned and installs per user. A SHA-256 checksum file is published beside it. The checksum confirms the file arrived intact; it does not confirm who published it.',
        'The Windows build is unsigned and installs per user. A SHA-256 checksum file ships beside it. The checksum proves the file arrived intact — it proves nothing about who published it.',
        'Heads up: the Windows build is unsigned and installs per user. The SHA-256 file beside it proves the download arrived intact — it does not prove who made it. Windows will say so too.',
        'Fair warning: this Windows build is unsigned and installs per user, just for you. The SHA-256 file proves the bytes arrived unharmed, not that anyone trustworthy made them. Windows SmartScreen will have opinions.',
        'Fair warning: this Windows build is unsigned and installs per user only. The SHA-256 file proves the bytes survived the trip — it proves absolutely nothing about who packed them. SmartScreen will squint at you, and honestly it has a point.'
      ],
      yue: [
        'Windows 版未簽名，以每位使用者身分安裝，旁邊有 SHA-256 檢查碼檔案。檢查碼只證明檔案完整，唔證明係邊個發佈。',
        'Windows 版未簽名，以每位使用者身分安裝，旁邊附有 SHA-256 檢查碼。檢查碼證明檔案完整，但唔會證明發佈者身分。',
        '講明先：Windows 版未簽名，淨係裝俾你自己。旁邊嘅 SHA-256 只證明檔案冇壞，唔證明係邊個整。Windows 都會咁話你知。',
        '醜話講前頭：呢個 Windows 版未簽名，淨係裝俾你一個人。SHA-256 只證明啲 byte 冇喺路上跌親，唔代表整佢嗰個係好人。SmartScreen 一定有意見。',
        '醜話講前頭：呢個 Windows 版未簽名，淨係裝俾你一個。SHA-256 只證明啲 byte 一路平安到埗，完全唔證明係邊個包嘅。SmartScreen 一定側埋眼望你，講真佢都唔算冤枉你。'
      ]
    },
    'hero.checksum': { en: ['SHA-256 file'], yue: ['SHA-256 檔案'] },
    'hero.releaseDetails': { en: ['Release details'], yue: ['發佈詳情'] },
    'hero.chip.layers': { en: ['Sliced · 180 layers'], yue: ['已切片 · 180 層'] },
    'hero.chip.accent': { en: ['Accent → tonal ramp'], yue: ['種子色 → 色階'] },
    'hero.chip.time': { en: ['1h 24m · 23.4 g'], yue: ['1 小時 24 分 · 23.4 克'] },
    'hero.art.alt': {
      en: ['A desktop 3D printer creating a faceted model while luminous sliced toolpaths surround the build plate.'],
      yue: ['一部枱面 3D 打印機正喺度打印一個多面體模型，發光嘅切片路徑圍住打印板。']
    },
    'overview.facts.heading': {
      en: [
        'What this site actually is',
        'What this site actually is',
        'What you are looking at',
        'What you are actually looking at',
        'What you have wandered into'
      ],
      yue: [
        '呢個網站究竟係乜',
        '呢個網站究竟係乜',
        '你而家睇緊咩',
        '你而家究竟睇緊咩',
        '你行咗入嚟嘅係咩地方'
      ]
    },
    'overview.facts.body': {
      en: [
        'This page is a static site with no build step and no third-party requests. Fonts, artwork and code are served from this repository. Preferences are stored in your browser only.',
        'This page is a static site: no build step, no third-party requests. Fonts, artwork and code all come from this repository, and your preferences never leave your browser.',
        'No build step, no trackers, no CDN. The fonts, the artwork and every line of script come from this repository, and your preferences stay in your own browser where they belong.',
        'No build step, no trackers, no CDN, no cookie banner to decline. Fonts, artwork and script all come from this repository, and your preferences never leave your browser.',
        'No build step, no trackers, no CDN, and no cookie banner for you to angrily decline. Everything — fonts, artwork, every line of script — comes from this repository, and your preferences stay locked in your own browser like a diary.'
      ],
      yue: [
        '呢一版係靜態網站，冇建置步驟，亦冇第三方請求。字型、圖像同程式碼全部由呢個 repository 提供，偏好設定只存喺你部瀏覽器。',
        '呢一版係靜態網站：冇建置步驟、冇第三方請求。字型、圖像同程式碼全部由呢個 repository 出，偏好設定唔會離開你部瀏覽器。',
        '冇建置步驟、冇追蹤、冇 CDN。字型、圖像同每一行 script 都由呢個 repository 出，偏好設定安安穩穩留喺你自己部瀏覽器。',
        '冇建置步驟、冇追蹤、冇 CDN，連俾你嬲住撳「拒絕」嘅 cookie 橫額都冇。字型、圖像、script 全部自己 repository 出，偏好設定唔會離開你部瀏覽器。',
        '冇建置步驟、冇追蹤、冇 CDN，連俾你嬲住撳「拒絕」嘅 cookie 橫額都慳返。字型、圖像、每一行 script 都係自己 repository 出，偏好設定就好似日記咁鎖喺你自己部瀏覽器。'
      ]
    },
    'overview.stat.releases': { en: ['Published releases'], yue: ['已發佈版本'] },
    'overview.stat.screens': { en: ['Restyled screens'], yue: ['重新設計嘅介面'] },
    'overview.stat.cases': { en: ['Layout cases checked'], yue: ['已檢查嘅版面情況'] },
    'overview.stat.requests': { en: ['Third-party requests'], yue: ['第三方請求'] },

    /* ------------------------------------------------------------- screens */
    'screens.heading': {
      en: [
        'Screens',
        'Screens',
        'Every screen, restyled',
        'Every screen, restyled — yes, even the boring ones',
        'Every screen, restyled — yes, even the ones nobody screenshots'
      ],
      yue: [
        '介面',
        '介面',
        '每一版都重新設計',
        '每一版都重新設計 — 連悶嗰啲都有份',
        '每一版都重新設計 — 連冇人截圖嗰啲都執埋'
      ]
    },
    'screens.body': {
      en: [
        'The real Bambu Studio tab set, plus the connected-home handover this fork adds. Each card links to the matching screen in the interactive prototype.',
        'The real Bambu Studio tab set, plus the connected-home handover this fork adds. Each card opens the matching screen in the interactive prototype.',
        'The genuine Bambu Studio tab set, plus the Home Assistant handover this fork adds on top. Tap a card and the prototype opens on that screen.'
      ],
      yue: [
        '真實 Bambu Studio 嘅分頁組合，加埋呢個分支新增嘅智能家居交接。每張卡片對應原型入面同一版介面。',
        '真實 Bambu Studio 嘅分頁組合，加埋呢個分支新增嘅智能家居交接。撳張卡就會開返原型嗰一版。',
        '正牌 Bambu Studio 嘅分頁組合，再加呢個分支自己加嘅 Home Assistant 交接。撳張卡，原型即刻跳去嗰一版。'
      ]
    },

    'screen.home': { en: ['Home'], yue: ['主頁'] },
    'screen.home.body': {
      en: [
        'Start a project, reopen recent work, and browse the model library.',
        'Start a project, reopen recent work, browse the model library.',
        'Start something, reopen the thing you abandoned last night, or go shopping in the model library.'
      ],
      yue: [
        '開新專案、重開最近嘅工作、瀏覽模型庫。',
        '開新專案、重開最近嘅工作、行下模型庫。',
        '開新嘢、重開你尋晚放棄咗嗰件、或者入模型庫行下街。'
      ]
    },
    'screen.prepare': { en: ['Prepare'], yue: ['準備'] },
    'screen.prepare.body': {
      en: [
        'The 3D editor: gizmo rail, scene tools, and the printer and process sidebar.',
        'The 3D editor — gizmo rail, scene tools, printer and process sidebar.',
        'The 3D editor, where the gizmo rail, the scene tools and the printer sidebar all fight for your attention.'
      ],
      yue: [
        '3D 編輯器：工具導軌、場景工具、打印機同流程側欄。',
        '3D 編輯器 — 工具導軌、場景工具、打印機同流程側欄。',
        '3D 編輯器，工具導軌、場景工具同打印機側欄喺度爭住要你注意佢。'
      ]
    },
    'screen.preview': { en: ['Preview'], yue: ['預覽'] },
    'screen.preview.body': {
      en: [
        'The G-code viewer with colour schemes, a legend, and layer and move sliders.',
        'The G-code viewer — colour schemes, legend, layer and move sliders.',
        'The G-code viewer, where you scrub through every layer and quietly judge your own supports.'
      ],
      yue: [
        'G-code 檢視器，有色彩配置、圖例同層數、移動滑桿。',
        'G-code 檢視器 — 色彩配置、圖例、層數同移動滑桿。',
        'G-code 檢視器，你可以逐層拉嚟睇，順便靜靜雞嫌自己支撐做得差。'
      ]
    },
    'screen.device': { en: ['Device'], yue: ['裝置'] },
    'screen.device.body': {
      en: [
        'Live camera, temperatures, fans, axis control, AMS and calibration.',
        'Live camera, temperatures, fans, axis control, AMS and calibration.',
        'Live camera, temperatures, fans, axis control, AMS and calibration — printer television, basically.'
      ],
      yue: [
        '即時鏡頭、溫度、風扇、軸控制、AMS 同校準。',
        '即時鏡頭、溫度、風扇、軸控制、AMS 同校準。',
        '即時鏡頭、溫度、風扇、軸控制、AMS 同校準 — 即係打印機台電視。'
      ]
    },
    'screen.multidevice': { en: ['Multi-device'], yue: ['多裝置'] },
    'screen.multidevice.body': {
      en: [
        'Monitor every printer at a glance, with progress and status.',
        'Monitor every printer at a glance, with progress and status.',
        'Every printer on one wall, with progress and status. Excellent for pretending you are in mission control.'
      ],
      yue: [
        '一眼睇晒所有打印機嘅進度同狀態。',
        '一眼睇晒所有打印機嘅進度同狀態。',
        '成幅牆嘅打印機，進度同狀態一眼睇晒。扮太空總署指揮中心一流。'
      ]
    },
    'screen.project': { en: ['Project'], yue: ['專案'] },
    'screen.project.body': {
      en: [
        'Project files: pictures, bill of materials, assembly guides and notes.',
        'Project files — pictures, bill of materials, assembly guides and notes.',
        'Pictures, bill of materials, assembly guides and notes — everything future-you will wish past-you had written down.'
      ],
      yue: [
        '專案檔案：圖片、物料清單、組裝指南同筆記。',
        '專案檔案 — 圖片、物料清單、組裝指南同筆記。',
        '圖片、物料清單、組裝指南同筆記 — 即係將來嘅你，會慶幸而家嘅你有寫低。'
      ]
    },
    'screen.calibration': { en: ['Calibration'], yue: ['校準'] },
    'screen.calibration.body': {
      en: [
        'Flow dynamics, flow rate, maximum volumetric speed, temperature tower and more.',
        'Flow dynamics, flow rate, maximum volumetric speed, temperature tower and more.',
        'Flow dynamics, flow rate, max volumetric speed, temperature tower — the part where you print small objects to make bigger ones behave.'
      ],
      yue: [
        '流量動態、流量、最大體積速度、溫度塔等等。',
        '流量動態、流量、最大體積速度、溫度塔等等。',
        '流量動態、流量、最大體積速度、溫度塔 — 即係打細嘢，等大嘢聽話嗰個部分。'
      ]
    },
    'screen.filament': { en: ['Filament'], yue: ['線材'] },
    'screen.filament.body': {
      en: [
        'Manage presets: filter, select and bulk-export filaments.',
        'Manage presets — filter, select and bulk-export filaments.',
        'Filter, select and bulk-export filaments, so the spool graveyard finally has a filing system.'
      ],
      yue: [
        '管理預設：篩選、選取同批量匯出線材。',
        '管理預設 — 篩選、選取同批量匯出線材。',
        '篩選、選取、批量匯出線材，你個線軸墳場終於有個檔案系統。'
      ]
    },
    'screen.settings': { en: ['Settings'], yue: ['設定'] },
    'screen.settings.body': {
      en: [
        'Appearance, presets, network, version history and about.',
        'Appearance, presets, network, version history and about.',
        'Appearance, presets, network, version history and about — the tab everyone visits once and then never leaves.'
      ],
      yue: [
        '外觀、預設、網絡、版本歷史同關於。',
        '外觀、預設、網絡、版本歷史同關於。',
        '外觀、預設、網絡、版本歷史同關於 — 人人話入去睇一眼，然後就唔捨得走。'
      ]
    },
    'screen.smarthome': { en: ['Home Assistant'], yue: ['Home Assistant'] },
    'screen.smarthome.body': {
      en: [
        'Choose an explicit long-lived-token handover or a five-minute local discovery window. Both disclose that the printer access code is transferred.',
        'Choose an explicit long-lived-token handover or a five-minute local discovery window. Both disclose that the printer access code is transferred.',
        'Either hand over a long-lived token yourself, or open a five-minute local discovery window. Both say plainly that the printer access code goes across — no quiet transfers here.'
      ],
      yue: [
        '可以明確交出長期權杖，或者開一個五分鐘嘅本機探索窗口。兩種都會講明打印機存取碼會被傳送。',
        '可以明確交出長期權杖，或者開一個五分鐘嘅本機探索窗口。兩種都會講明打印機存取碼會被傳送。',
        '你可以自己交長期權杖，或者開個五分鐘本機探索窗口。兩種都會白紙黑字話你知打印機存取碼會過檔 — 冇偷雞。'
      ]
    },

    /* -------------------------------------------------------- material you */
    'you.heading': {
      en: ['Material You, all the way down', 'Material You, all the way down', 'Material You, all the way down to the tokens'],
      yue: ['Material You，由頭到尾', 'Material You，由頭到尾', 'Material You，一路貫穿到 token']
    },
    'you.body': {
      en: [
        'Theme, density and accent are first-class settings. Choose a seed colour and the site derives a full tonal ramp for primary, containers and state layers, live, with no reload.',
        'Theme, density and accent are first-class settings. Pick a seed colour and a full tonal ramp for primary, containers and state layers is derived live, with no reload.',
        'Theme, density and accent are proper settings, not afterthoughts. Pick a seed colour and the whole tonal ramp — primary, containers, state layers — is derived on the spot, no reload.',
        'Theme, density and accent are proper settings, not afterthoughts bolted on at the end. Pick a seed colour and the entire tonal ramp regenerates on the spot. No reload, no flicker, no apology.',
        'Theme, density and accent are proper settings, not afterthoughts bolted on the night before shipping. Pick a seed colour and the entire tonal ramp regenerates while you watch — no reload, no flash of unstyled regret.'
      ],
      yue: [
        '主題、密度同重點色都係一級設定。揀隻種子色，主色、容器同狀態層嘅完整色階即時生成，唔使重新載入。',
        '主題、密度同重點色都係一級設定。揀隻種子色，主色、容器、狀態層嘅完整色階即場生成，唔使重新載入。',
        '主題、密度、重點色都係認真嘅設定，唔係事後補鑊。揀隻種子色，成條色階即刻生成，唔使 reload。',
        '主題、密度、重點色都係認真設定，唔係最後一晚先補上去嗰啲。揀隻種子色，成條色階即場重生，唔 reload、唔閃、唔使講對唔住。',
        '主題、密度、重點色全部係正經設定，唔係出貨前一晚先貼上去嗰啲。揀隻種子色，成條色階當住你面前重生 — 唔 reload、唔閃白光、唔使事後道歉。'
      ]
    },
    'you.theme': { en: ['Theme'], yue: ['主題'] },
    'you.density': { en: ['Density'], yue: ['密度'] },
    'you.accent': { en: ['Accent'], yue: ['重點色'] },
    'you.light': { en: ['Light'], yue: ['淺色'] },
    'you.dark': { en: ['Dark'], yue: ['深色'] },
    'you.comfortable': { en: ['Comfortable'], yue: ['寬鬆'] },
    'you.compact': { en: ['Compact'], yue: ['緊湊'] },
    'you.try': { en: ['Try it live', 'Try it live', 'Go on, poke it'], yue: ['即時試玩', '即時試玩', '試下撳兩下'] },

    /* ------------------------------------------------------------ download */
    'download.heading': {
      en: ['Download', 'Download', 'Get the Windows build'],
      yue: ['下載', '下載', '攞 Windows 版']
    },
    'download.body': {
      en: [
        'Every push that passes its tests publishes a uniquely tagged GitHub Release with a real Windows installer, its SHA-256 checksum, and a CycloneDX SBOM.',
        'Every push that passes its tests publishes a uniquely tagged GitHub Release carrying a real Windows installer, its SHA-256 checksum, and a CycloneDX SBOM.',
        'Every push that passes its tests ships a uniquely tagged release: a real installer, its SHA-256 checksum and a CycloneDX SBOM. A push that fails its tests ships nothing at all.',
        'Every push that passes its tests ships a uniquely tagged release — real installer, SHA-256 checksum, CycloneDX SBOM. A push that fails its tests ships absolutely nothing, which is the point.',
        'Every push that passes its tests ships a uniquely tagged release — real installer, SHA-256 checksum, CycloneDX SBOM, the lot. A push that fails its tests ships nothing whatsoever, and that silence is the feature.'
      ],
      yue: [
        '每次通過測試嘅推送都會發佈一個獨立標籤嘅 GitHub Release，附有真實 Windows 安裝程式、SHA-256 檢查碼同 CycloneDX SBOM。',
        '每次通過測試嘅推送都會發佈一個獨立標籤嘅 GitHub Release，帶住真實 Windows 安裝程式、SHA-256 檢查碼同 CycloneDX SBOM。',
        '每次通過測試嘅推送都會出一個獨立標籤版本：真安裝程式、SHA-256、CycloneDX SBOM。測試唔過嘅推送，一件都唔會出。',
        '每次通過測試嘅推送都會出一個獨立標籤版本 — 真安裝程式、SHA-256、CycloneDX SBOM，樣樣齊。測試唔過就一件都唔出，呢個就係重點。',
        '每次通過測試嘅推送都會出一個獨立標籤版本 — 真安裝程式、SHA-256、CycloneDX SBOM，一件都唔少。測試唔過就靜靜雞乜都唔出，嗰份靜就係功能嚟。'
      ]
    },
    'download.latest': { en: ['Latest release'], yue: ['最新版本'] },
    'download.assets': { en: ['Attached files'], yue: ['附帶檔案'] },
    'download.verify.heading': {
      en: ['Verify before you run it', 'Verify before you run it', 'Check it before you trust it'],
      yue: ['執行前先驗證', '執行前先驗證', '信之前先查清楚']
    },
    'download.verify.body': {
      en: [
        'Compare the downloaded installer against the published SHA-256, or verify the build attestation with the GitHub CLI. Both commands are shown below and run locally.',
        'Compare the downloaded installer against the published SHA-256, or verify the build attestation with the GitHub CLI. Both commands run locally and are shown below.',
        'Compare your download against the published SHA-256, or ask the GitHub CLI to verify the build attestation. Both commands run on your own machine — copy either one.',
        'Compare your download against the published SHA-256, or make the GitHub CLI check the build attestation for you. Both run on your own machine, and neither phones anyone.',
        'Compare your download against the published SHA-256, or let the GitHub CLI interrogate the build attestation on your behalf. Both run on your own machine and neither one phones home to tell on you.'
      ],
      yue: [
        '將下載到嘅安裝程式同已公佈嘅 SHA-256 對照，或者用 GitHub CLI 驗證建置證明。兩條指令都喺本機執行，列喺下面。',
        '將下載到嘅安裝程式同已公佈嘅 SHA-256 對照，或者用 GitHub CLI 驗證建置證明。兩條指令都喺本機行，列喺下面。',
        '將你下載嘅檔案同公佈咗嘅 SHA-256 對一對，或者叫 GitHub CLI 幫你查建置證明。兩條都喺你自己部機行，隨便抄一條。',
        '將你下載嘅檔案同公佈咗嘅 SHA-256 對一對，或者叫 GitHub CLI 幫你盤問建置證明。兩條都喺你自己部機行，冇一條會通水俾人。',
        '將你下載嘅檔案同公佈咗嘅 SHA-256 對一對，或者叫 GitHub CLI 幫你盤問吓個建置證明。兩條都喺你自己部機行，冇一條會偷偷通水話你落咗架。'
      ]
    },
    'download.size': { en: ['Size'], yue: ['大小'] },
    'download.published': { en: ['Published'], yue: ['發佈時間'] },
    'download.commit': { en: ['Commit'], yue: ['提交'] },
    'download.workflow': { en: ['CI run'], yue: ['CI 執行'] },
    'download.allreleases': { en: ['All releases'], yue: ['全部版本'] },

    /* ----------------------------------------------------------- changelog */
    'changelog.heading': {
      en: ['Changelog', 'Changelog', 'Everything that shipped'],
      yue: ['更新日誌', '更新日誌', '出過嘅嘢全部喺度']
    },
    'changelog.body': {
      en: [
        'Every published release, newest first. Versions, dates, commits and attached files are read from the GitHub Releases API; change lines are the commit subjects between each release tag.',
        'Every published release, newest first. Versions, dates, commits and attached files come from the GitHub Releases API; change lines are the commit subjects between each release tag.',
        'Every release this fork has published, newest first. Versions, dates, commits and files come straight from the Releases API; the change lines are the actual commit subjects between tags — nothing here is written by hand.',
        'Every release this fork has ever published, newest first. Versions, dates, commits and files come straight from the Releases API, and the change lines are the real commit subjects between tags. Nothing is hand-written, so nothing is flattering.',
        'Every release this fork has ever published, newest first. Versions, dates, commits and files come straight from the Releases API, and the change lines are the genuine commit subjects between tags — nobody polished them, so they are as flattering as a passport photo.'
      ],
      yue: [
        '所有已發佈版本，最新排先。版本、日期、提交同附件由 GitHub Releases API 讀取；變更行係兩個版本標籤之間嘅提交標題。',
        '所有已發佈版本，最新排先。版本、日期、提交同附件由 GitHub Releases API 攞；變更行係兩個標籤之間嘅提交標題。',
        '呢個分支出過嘅所有版本，最新排先。版本、日期、提交、檔案全部直接由 Releases API 攞；變更行係標籤之間真實嘅提交標題，冇一行係人手寫。',
        '呢個分支出過嘅所有版本，最新排先。版本、日期、提交、檔案全部直接由 Releases API 攞，變更行係標籤之間真實提交標題。冇人執過靚，所以都幾老實。',
        '呢個分支出過嘅所有版本，最新排先。全部資料直接由 Releases API 攞，變更行係標籤之間原汁原味嘅提交標題 — 冇人執過靚，靚仔程度大概等同你張回鄉證相。'
      ]
    },
    'changelog.search': { en: ['Search changelog'], yue: ['搜尋更新日誌'] },
    'changelog.from': { en: ['From date'], yue: ['起始日期'] },
    'changelog.to': { en: ['To date'], yue: ['結束日期'] },
    'changelog.calendar': { en: ['Open calendar'], yue: ['開啟日曆'] },
    'changelog.preset.all': { en: ['All time'], yue: ['全部時間'] },
    'changelog.preset.7': { en: ['Last 7 days'], yue: ['最近 7 日'] },
    'changelog.preset.30': { en: ['Last 30 days'], yue: ['最近 30 日'] },
    'changelog.preset.90': { en: ['Last 90 days'], yue: ['最近 90 日'] },
    'changelog.dateformat': {
      en: ['Type a date as YYYY-MM-DD, or use the calendar.'],
      yue: ['可以打 YYYY-MM-DD 格式，或者用日曆揀。']
    },
    'changelog.dateinvalid': {
      en: [
        'That date was not understood, so the filter was left unchanged. Expected YYYY-MM-DD.',
        'That date was not understood, so the filter is unchanged. Expected format: YYYY-MM-DD.',
        'That date did not parse, so nothing was filtered. What we understand is YYYY-MM-DD.',
        'That date did not parse, so the filter stayed where it was. YYYY-MM-DD is the format that works.',
        'That date defeated the parser, so the filter stayed exactly where it was. YYYY-MM-DD is the magic shape.'
      ],
      yue: [
        '睇唔明呢個日期，所以篩選冇改。格式應該係 YYYY-MM-DD。',
        '睇唔明呢個日期，篩選維持原狀。格式係 YYYY-MM-DD。',
        '呢個日期解析唔到，所以乜都冇篩。我哋識睇嘅係 YYYY-MM-DD。',
        '呢個日期解析唔到，篩選原封不動。YYYY-MM-DD 先至行得通。',
        '呢個日期打敗咗個解析器，篩選文風不動。YYYY-MM-DD 先至係通行證。'
      ]
    },
    'changelog.empty': {
      en: [
        'No releases match the current search and date filter.',
        'No releases match the current search and date filter.',
        'Nothing matches that search inside that date range. Widen one of them.',
        'Nothing matches that search inside that date range. Loosen one of them and try again.',
        'Nothing survived that search plus that date range. One of the two is being unreasonable — loosen it.'
      ],
      yue: [
        '冇版本符合目前嘅搜尋同日期篩選。',
        '冇版本符合目前嘅搜尋同日期篩選。',
        '喺呢個日期範圍入面搵唔到符合嘅嘢。放寬其中一樣啦。',
        '喺呢個日期範圍入面乜都搵唔到。放寬其中一樣再試。',
        '呢個搜尋加呢個日期範圍，殺到一個唔剩。兩樣入面總有一樣太苛刻，放鬆啲啦。'
      ]
    },
    'changelog.nochanges': {
      en: [
        'No commits were recorded between this release and the one before it.',
        'No commits were recorded between this release and the one before it.',
        'No commits were recorded between this release and the previous one.',
        'Not one commit was recorded between this release and the previous one.',
        'Not one single commit was recorded between this release and the last one.'
      ],
      yue: [
        '呢個版本同上一個版本之間冇記錄到任何提交。',
        '呢個版本同上一個版本之間冇記錄到任何提交。',
        '呢個版本同上一個之間冇記錄到提交。',
        '呢個版本同上一個之間，一個提交都冇記錄到。',
        '呢個版本同上一個之間，一個提交都搵唔返。'
      ]
    },
    'changelog.samecommit': {
      en: [
        'This release was published from the same commit as the one before it.',
        'This release was published from the same commit as the one before it.',
        'Same commit as the previous release — a new tag on identical code.',
        'Same commit as the previous release. New tag, identical code, no drama.',
        'Same commit as the previous release. New tag, identical code, zero drama.'
      ],
      yue: [
        '呢個版本同上一個版本用同一個提交發佈。',
        '呢個版本同上一個版本用同一個提交發佈。',
        '同上一個版本同一個提交 — 新標籤、一樣嘅程式碼。',
        '同上一個版本同一個提交。新標籤、一模一樣嘅程式碼、冇故事。',
        '同上一個版本同一個提交。新標籤、一模一樣嘅程式碼、零劇情。'
      ]
    },
    'changelog.baseline': {
      en: ['Oldest published release — there is no earlier release to compare it against.'],
      yue: ['最早發佈嘅版本 — 冇更早嘅版本可以比較。']
    },
    'changelog.count': { en: ['{shown} of {total} releases'], yue: ['{total} 個版本中顯示 {shown} 個'] },
    'changelog.export': { en: ['Export Markdown'], yue: ['匯出 Markdown'] },
    'changelog.copy': { en: ['Copy view'], yue: ['複製目前檢視'] },
    'changelog.derivation': {
      en: ['Change categories are derived mechanically from each commit subject’s leading verb.'],
      yue: ['變更分類係機械式由每個提交標題嘅開頭動詞推導出嚟。']
    },
    'changelog.cat.added': { en: ['Added'], yue: ['新增'] },
    'changelog.cat.fixed': { en: ['Fixed'], yue: ['修正'] },
    'changelog.cat.changed': { en: ['Changed'], yue: ['變更'] },
    'changelog.cat.removed': { en: ['Removed'], yue: ['移除'] },
    'changelog.cat.documented': { en: ['Documented'], yue: ['文件'] },

    /* --------------------------------------------------------------- regex */
    'regex.heading': {
      en: ['Regex lab', 'Regex lab', 'Regex lab — build it, do not guess it'],
      yue: ['正則實驗室', '正則實驗室', '正則實驗室 — 砌出嚟，唔好靠估']
    },
    'regex.body': {
      en: [
        'Build a pattern from labelled pieces or type it directly. The engine is the browser’s own ECMAScript RegExp, so what matches here is exactly what matches in every search field on this site.',
        'Build a pattern from labelled pieces or type it directly. The engine is the browser’s ECMAScript RegExp, so what matches here matches identically in every search field on this site.',
        'Assemble a pattern from labelled pieces, or type it raw. The engine is your browser’s own ECMAScript RegExp — what matches here matches the same way in every search field on this site.',
        'Assemble a pattern from labelled pieces, or type it raw if you are feeling brave. It runs on your browser’s own ECMAScript RegExp, so what matches here matches identically everywhere else on this site.',
        'Assemble a pattern from labelled pieces, or type it raw if you fancy your chances. It runs on your browser’s own ECMAScript RegExp — so whatever it matches here, it matches exactly the same everywhere else on this site.'
      ],
      yue: [
        '可以用標籤化嘅零件砌出模式，亦可以直接打。引擎係瀏覽器本身嘅 ECMAScript RegExp，所以喺呢度襯到嘅嘢，喺本站每個搜尋框都一模一樣襯到。',
        '可以用標籤化零件砌模式，亦可以直接打。引擎係瀏覽器本身嘅 ECMAScript RegExp，喺呢度襯到嘅，喺本站每個搜尋框都一樣。',
        '用零件砌，或者直接打生粗。引擎係你部瀏覽器嘅 ECMAScript RegExp — 呢度襯到嘅嘢，全站每個搜尋框都一樣襯到。',
        '用零件砌，夠膽就直接打生粗。行嘅係你部瀏覽器自己嘅 ECMAScript RegExp，呢度襯到乜，全站其他地方都襯到一模一樣嘅嘢。',
        '用零件砌，覺得自己好打得就直接打生粗。行嘅係你部瀏覽器自己嘅 ECMAScript RegExp — 呢度襯到乜，全站其他地方都照襯，一個字都唔會走漏。'
      ]
    },
    'regex.engine': { en: ['Engine: ECMAScript RegExp (this browser)'], yue: ['引擎：ECMAScript RegExp（本瀏覽器）'] },
    'regex.pattern': { en: ['Pattern'], yue: ['模式'] },
    'regex.flags': { en: ['Flags'], yue: ['旗標'] },
    'regex.sample': { en: ['Sample text'], yue: ['測試文字'] },
    'regex.matches': { en: ['Matches'], yue: ['命中'] },
    'regex.groups': { en: ['Capture groups'], yue: ['擷取組'] },
    'regex.nomatch': {
      en: [
        'The pattern is valid and matches nothing in this sample.',
        'The pattern is valid and matches nothing in this sample.',
        'Valid pattern, zero matches in this sample. Both of those are facts, neither is a bug.',
        'Valid pattern, zero matches. Nothing is broken; the sample simply refuses to co-operate.',
        'Valid pattern, zero matches. Nothing is broken — the sample text is just refusing to play along.'
      ],
      yue: [
        '呢個模式有效，但喺呢段測試文字入面冇任何命中。',
        '呢個模式有效，但喺呢段測試文字入面冇任何命中。',
        '模式正確，命中零個。兩樣都係事實，唔係 bug。',
        '模式正確，命中零個。乜都冇壞，係段文字唔肯合作啫。',
        '模式正確，命中零個。乜都冇壞 — 純粹係段測試文字唔肯就你。'
      ]
    },
    'regex.invalid': {
      en: [
        'The pattern is not valid and was not applied. The engine reported: {message}',
        'The pattern is not valid and was not applied. The engine reported: {message}',
        'That pattern will not compile, so nothing was searched. The engine says: {message}',
        'That pattern refuses to compile, so no search ran. The engine’s exact words: {message}',
        'That pattern point-blank refuses to compile, so no search ran at all. The engine’s exact words: {message}'
      ],
      yue: [
        '呢個模式無效，冇被套用。引擎回報：{message}',
        '呢個模式無效，冇被套用。引擎回報：{message}',
        '呢個模式編譯唔到，所以乜都冇搜尋過。引擎話：{message}',
        '呢個模式死都唔肯編譯，所以冇行過搜尋。引擎原話：{message}',
        '呢個模式死都唔肯編譯，一次搜尋都冇行過。引擎原話一字不改：{message}'
      ]
    },
    'regex.timeout': {
      en: [
        'Evaluation was stopped after {ms} ms and no results were produced. The pattern and sample were left unchanged; simplify the pattern or shorten the sample.',
        'Evaluation was stopped after {ms} ms and produced no results. Your pattern and sample are unchanged; simplify the pattern or shorten the sample.',
        'We stopped the match after {ms} ms, so there are no results. Your pattern and sample are untouched — simplify the pattern or cut the sample down.',
        'We pulled the plug after {ms} ms, so there are no results. Your pattern and sample are untouched; that pattern is backtracking itself into a corner.',
        'We pulled the plug after {ms} ms, so there are no results. Your pattern and sample are untouched — that pattern was backtracking itself into a corner and taking your CPU along for company.'
      ],
      yue: [
        '運算喺 {ms} 毫秒後被中止，冇產生任何結果。模式同測試文字保持原狀；請簡化模式或者縮短文字。',
        '運算喺 {ms} 毫秒後被中止，冇任何結果。模式同文字保持原狀；請簡化模式或者縮短文字。',
        '{ms} 毫秒之後我哋叫停咗，所以冇結果。你嘅模式同文字原封不動 — 簡化個模式或者剪短段文字。',
        '{ms} 毫秒之後我哋直接拔咗插頭，所以冇結果。你嘅模式同文字原封不動；個模式回溯到自己捐晒入死角。',
        '{ms} 毫秒之後我哋直接拔咗插頭，所以冇結果。你嘅模式同文字原封不動 — 個模式回溯到捐晒入死角，仲要拉埋你部 CPU 落水。'
      ]
    },
    'regex.needsunicode': {
      en: [
        'No matches. This pattern uses \\p{…}, which needs the u flag to mean a Unicode property; without it the engine reads it as literal characters.',
        'No matches. This pattern uses \\p{…}, which needs the u flag to mean a Unicode property — without it the engine reads it as literal characters.',
        'No matches, and here is why: \\p{…} only means a Unicode property with the u flag on. Without it the engine is hunting for a literal “p{”.',
        'No matches, and it is not your fault: \\p{…} only means a Unicode property with the u flag on. Without it the engine is politely hunting for a literal “p{”.',
        'No matches, and it is not your fault: \\p{…} only means a Unicode property when u is ticked. Without it the engine is out there hunting for a literal “p{” like it is 1999.'
      ],
      yue: [
        '冇命中。呢個模式用咗 \\p{…}，要開 u 旗標先會當佢係 Unicode 屬性；唔開嘅話引擎會當佢係普通字元。',
        '冇命中。呢個模式用咗 \\p{…}，要開 u 旗標先會當佢係 Unicode 屬性 — 唔開就當普通字元。',
        '冇命中，原因喺度：\\p{…} 一定要開 u 旗標先當 Unicode 屬性。唔開嘅話引擎係喺度搵緊個「p{」字面。',
        '冇命中，唔關你事：\\p{…} 一定要剔咗 u 先當 Unicode 屬性。唔剔嘅話引擎好禮貌咁去搵個「p{」字面。',
        '冇命中，唔關你事：\\p{…} 一定要剔咗 u 先當 Unicode 屬性。唔剔嘅話引擎就會好認真咁去搵個「p{」字面，搵到天光。'
      ]
    },
    'regex.unsafe': {
      en: [
        'This pattern nests one quantifier inside another, which can take exponential time, so it was not run. The pattern and sample were left unchanged.',
        'This pattern nests one quantifier inside another, which can take exponential time, so it was not run. Your pattern and sample are unchanged.',
        'That pattern puts a quantifier inside a quantified group — the shape that goes exponential — so we did not run it. Nothing was changed.',
        'That pattern puts a quantifier inside a quantified group, which is the classic way to make a regex take until Thursday. Not run; nothing changed.',
        'That pattern puts a quantifier inside a quantified group — the classic recipe for a regex that finishes some time next Thursday. Not run, nothing changed.'
      ],
      yue: [
        '呢個模式喺數量詞入面再套數量詞，可能會用指數時間，所以冇執行。模式同測試文字保持原狀。',
        '呢個模式喺數量詞入面再套數量詞，可能會用指數時間，所以冇執行。模式同文字保持原狀。',
        '呢個模式喺已加數量詞嘅群組入面再加數量詞 — 就係會爆炸嗰種 — 所以冇行。乜都冇改。',
        '呢個模式喺已加數量詞嘅群組入面再加數量詞，經典嘅「行到下星期四」寫法。冇行，乜都冇改。',
        '呢個模式喺已加數量詞嘅群組入面再加數量詞，經典嘅「行到下星期四先有答案」寫法。冇行，乜都冇改。'
      ]
    },
    'regex.enginefailed': {
      en: [
        'The sandboxed evaluator could not start, so this ran on the page instead. The browser reported: {message}',
        'The sandboxed evaluator could not start, so this ran on the page instead. The browser reported: {message}',
        'The sandboxed evaluator refused to start, so matching fell back to the page. Your pattern was never judged. The browser said: {message}',
        'The sandboxed evaluator refused to start, so matching fell back to the page on a smaller sample. Nothing is wrong with your pattern. The browser said: {message}',
        'The sandboxed evaluator flat-out refused to start, so matching fell back to the page on a smaller sample. Your pattern is innocent. The browser said: {message}'
      ],
      yue: [
        '沙盒運算器啟動唔到，所以改咗喺頁面直接行。瀏覽器回報：{message}',
        '沙盒運算器啟動唔到，所以改咗喺頁面直接行。瀏覽器回報：{message}',
        '沙盒運算器唔肯啟動，改用頁面內比對。你個模式根本未被判過。瀏覽器話：{message}',
        '沙盒運算器唔肯啟動，改用頁面內比對，樣本亦細咗。你個模式冇問題。瀏覽器話：{message}',
        '沙盒運算器死都唔肯啟動，唯有喺頁面內用細樣本比對。你個模式係無辜嘅。瀏覽器話：{message}'
      ]
    },
    'regex.unsupported': {
      en: [
        'Regular-expression search is unavailable because this browser will not create the sandboxed evaluator; plain text search is still available.',
        'Regular-expression search is unavailable because this browser will not create the sandboxed evaluator. Plain text search still works.',
        'Regex search is off here: this browser will not create the sandboxed evaluator, and running one unsandboxed could freeze the tab. Plain text still works.',
        'Regex search is off here — this browser will not create the sandboxed evaluator, and running one unsandboxed could freeze the tab. Plain text still works fine.',
        'Regex search is off here: this browser refuses to create the sandboxed evaluator, and running one unsandboxed is how you freeze a tab. Plain text still works fine.'
      ],
      yue: [
        '呢部瀏覽器唔肯建立沙盒運算器，所以正則搜尋用唔到；純文字搜尋照常可用。',
        '呢部瀏覽器唔肯建立沙盒運算器，所以正則搜尋用唔到。純文字搜尋照常。',
        '呢度關咗正則搜尋：瀏覽器唔肯開沙盒，冇沙盒行落去可能會卡死成個分頁。純文字照用。',
        '呢度關咗正則搜尋 — 瀏覽器唔肯開沙盒，冇沙盒行落去可能會卡死成個分頁。純文字照用得。',
        '呢度關咗正則搜尋：瀏覽器死都唔肯開沙盒，而冇沙盒硬行就係卡死分頁嘅標準做法。純文字照用得。'
      ]
    },
    'regex.toolong': {
      en: ['The sample was truncated to {limit} characters before matching, to keep this page responsive.'],
      yue: ['測試文字喺比對前被截短到 {limit} 個字元，以保持頁面暢順。']
    },
    'regex.mode.plain': { en: ['Plain text'], yue: ['純文字'] },
    'regex.mode.regex': { en: ['Regular expression'], yue: ['正則表達式'] },
    'regex.open': { en: ['Open regex builder'], yue: ['開啟正則產生器'] },
    'regex.insert': { en: ['Insert'], yue: ['插入'] },
    'regex.copy': { en: ['Copy pattern'], yue: ['複製模式'] },
    'regex.export': { en: ['Export pattern'], yue: ['匯出模式'] },
    'regex.apply': { en: ['Use this pattern'], yue: ['使用呢個模式'] },
    'regex.clear': { en: ['Clear'], yue: ['清除'] },
    'regex.part.literal': { en: ['Literal text'], yue: ['字面文字'] },
    'regex.part.class': { en: ['Character class'], yue: ['字元類別'] },
    'regex.part.anchor': { en: ['Anchor'], yue: ['錨點'] },
    'regex.part.group': { en: ['Group'], yue: ['群組'] },
    'regex.part.alternation': { en: ['Alternation'], yue: ['擇一'] },
    'regex.part.quantifier': { en: ['Quantifier'], yue: ['數量詞'] },
    'regex.escapenote': {
      en: ['Literal text is escaped for you, so . and ? mean themselves.'],
      yue: ['字面文字會自動加逸出字元，所以 . 同 ? 就係佢自己。']
    },

    /* ------------------------------------------------------------ settings */
    'settings.heading': { en: ['Settings'], yue: ['設定'] },
    'settings.body': {
      en: [
        'Every setting here is stored in this browser only and applies immediately.',
        'Every setting here is stored in this browser only and applies immediately.',
        'Everything here saves to this browser and nowhere else, and applies the moment you change it.',
        'Everything here saves to this browser and nowhere else, and applies the instant you touch it. No account, no sync, no server.',
        'Everything here saves to this browser and nowhere else, and applies the instant you touch it. No account, no sync, no server quietly taking notes.'
      ],
      yue: [
        '呢度嘅設定淨係存喺呢部瀏覽器，改完即刻生效。',
        '呢度嘅設定淨係存喺呢部瀏覽器，改完即刻生效。',
        '呢度所有設定只存喺呢部瀏覽器，改嗰下就即刻生效。',
        '呢度所有設定只存喺呢部瀏覽器，一改即刻生效。冇帳戶、冇同步、冇伺服器。',
        '呢度所有設定只存喺呢部瀏覽器，一改即刻生效。冇帳戶、冇同步、亦冇伺服器喺後面靜靜雞抄低你做過乜。'
      ]
    },
    'settings.search': { en: ['Search settings'], yue: ['搜尋設定'] },
    'settings.search.hint': {
      en: ['Searches every settings group, including the ones not currently open.'],
      yue: ['會搜尋所有設定分組，包括而家未打開嗰啲。']
    },
    'settings.search.elsewhere': {
      en: ['{count} more match(es) in {group}.'],
      yue: ['喺「{group}」仲有 {count} 個符合項。']
    },
    'settings.search.empty': {
      en: [
        'No setting matches that search.',
        'No setting matches that search.',
        'No setting answers to that name. Try a shorter word.',
        'No setting answers to that name. Try a shorter word, or switch on regex.',
        'Not one setting answers to that name. Try a shorter word, or flip on regex and get properly clever.'
      ],
      yue: [
        '冇設定符合呢個搜尋。',
        '冇設定符合呢個搜尋。',
        '冇設定應呢個名。試下打短啲。',
        '冇設定應呢個名。試下打短啲，或者開埋正則。',
        '一個設定都唔應呢個名。試下打短啲，或者開埋正則玩大佢。'
      ]
    },
    'settings.group.language': { en: ['Language and tone'], yue: ['語言同語氣'] },
    'settings.group.appearance': { en: ['Appearance'], yue: ['外觀'] },
    'settings.group.typography': { en: ['Typography'], yue: ['字型'] },
    'settings.group.elements': { en: ['Element appearance'], yue: ['個別元件外觀'] },
    'settings.group.surprises': { en: ['Surprises and notifications'], yue: ['驚喜同通知'] },
    'settings.group.data': { en: ['Stored data'], yue: ['已儲存資料'] },
    'settings.language.mode': { en: ['Language mode'], yue: ['語言模式'] },
    'settings.language.desc': {
      en: ['English, Hong Kong Cantonese, or both together.'],
      yue: ['英文、香港廣東話，或者兩種一齊顯示。']
    },
    'settings.funny.en': { en: ['Funny level — English'], yue: ['搞笑程度 — 英文'] },
    'settings.funny.yue': { en: ['Funny level — Cantonese'], yue: ['搞笑程度 — 廣東話'] },
    'settings.funny.desc': {
      en: [
        'Sets the tone of the text this site renders in that language. Level 1 is fully professional; level 5 is maximum playfulness. It applies to every message, including errors, warnings and destructive confirmations. It never changes what a message says has happened or what will be affected.',
        'Sets the tone of every string this site renders in that language. Level 1 is fully professional, level 5 is maximum playfulness, and it applies to every message — errors, warnings and destructive confirmations included. It never changes what actually happened or what will be affected.'
      ],
      yue: [
        '控制本站用嗰種語言寫嘅文字語氣。第 1 級完全專業，第 5 級最玩得。所有訊息都受影響，包括錯誤、警告同破壞性確認。但佢永遠唔會改變訊息講嘅事實同影響範圍。',
        '控制本站用嗰種語言寫嘅文字語氣。第 1 級一本正經，第 5 級玩到盡，所有訊息都計 — 錯誤、警告、刪嘢確認全部有份。但事實同影響範圍永遠唔會被改動。'
      ]
    },
    'settings.funny.level1': { en: ['1 · Serious'], yue: ['1 · 認真'] },
    'settings.funny.level5': { en: ['5 · Maximum'], yue: ['5 · 玩到盡'] },
    'settings.funny.current': { en: ['Level {level} of 5'], yue: ['第 {level} 級（共 5 級）'] },
    'settings.funny.preview': { en: ['Preview'], yue: ['預覽'] },
    'settings.theme': { en: ['Theme'], yue: ['主題'] },
    'settings.theme.desc': { en: ['Light or dark surfaces.'], yue: ['淺色或深色表面。'] },
    'settings.density': { en: ['Density'], yue: ['密度'] },
    'settings.density.desc': { en: ['Comfortable or compact spacing.'], yue: ['寬鬆或者緊湊嘅間距。'] },
    'settings.accent': { en: ['Accent seed colour'], yue: ['重點種子色'] },
    'settings.accent.desc': {
      en: ['The seed the whole tonal palette is derived from.'],
      yue: ['成套色階由呢隻種子色推導出嚟。']
    },
    'settings.accent.custom': { en: ['Custom seed colour'], yue: ['自訂種子色'] },
    'settings.font.family': { en: ['UI font'], yue: ['介面字型'] },
    'settings.font.family.desc': {
      en: ['Bundled Roboto, or a font already installed on this computer. Chinese text falls back to a CJK-capable face automatically.'],
      yue: ['內附 Roboto，或者你部電腦已安裝嘅字型。中文會自動退回支援中文嘅字型。']
    },
    'settings.font.size': { en: ['Text size'], yue: ['文字大小'] },
    'settings.font.size.desc': { en: ['Scales every text style on the site.'], yue: ['會縮放全站每種文字樣式。'] },
    'settings.font.weight': { en: ['Body weight'], yue: ['內文字重'] },
    'settings.font.weight.desc': { en: ['Weight used for body copy.'], yue: ['內文使用嘅字重。'] },
    'settings.elements.desc': {
      en: ['Restyle one surface at a time. Each element keeps its own saved values and can be reset on its own.'],
      yue: ['逐個表面單獨改樣。每個元件有自己嘅儲存值，可以單獨還原。']
    },
    'settings.elements.pick': { en: ['Element'], yue: ['元件'] },
    'settings.elements.radius': { en: ['Corner radius'], yue: ['圓角'] },
    'settings.elements.spacing': { en: ['Spacing'], yue: ['間距'] },
    'settings.elements.color': { en: ['Text colour'], yue: ['文字顏色'] },
    'settings.elements.size': { en: ['Text size'], yue: ['文字大小'] },
    'settings.elements.reset': { en: ['Reset this element'], yue: ['還原呢個元件'] },
    'settings.element.tabstrip': { en: ['Tab strip'], yue: ['分頁列'] },
    'settings.element.cards': { en: ['Content cards'], yue: ['內容卡片'] },
    'settings.element.hero': { en: ['Hero headline'], yue: ['主標題'] },
    'settings.element.toasts': { en: ['Notifications'], yue: ['通知'] },
    'settings.dimsum': { en: ['Dim sum surprise'], yue: ['點心驚喜'] },
    'settings.dimsum.desc': {
      en: ['A one-in-a-hundred chance, per visit, of a dim sum dish appearing in the corner. It never blocks the page and dismisses itself.'],
      yue: ['每次入嚟有百分之一機會，角落會出現一款點心。唔會阻你做嘢，自己會消失。']
    },
    'settings.notify.enabled': { en: ['Show notifications'], yue: ['顯示通知'] },
    'settings.notify.desc': {
      en: ['Corner toasts for actions you take. Errors and warnings stay until dismissed either way, and everything is kept in the notification centre.'],
      yue: ['你做嘢之後喺角落彈出嘅提示。錯誤同警告點都會留到你自己關咗佢，全部都會存入通知中心。']
    },
    'settings.reset': { en: ['Reset all settings'], yue: ['還原所有設定'] },
    'settings.reset.desc': {
      en: [
        'Deletes every preference this site has stored in this browser: language mode, both funny levels, theme, density, accent, fonts, per-element styling, tab order, pinning, tab grouping, the dim sum surprise and the notification switch. This cannot be undone, and it does not touch anything outside this site.',
        'Deletes every preference this site has stored in this browser — language mode, both funny levels, theme, density, accent, fonts, per-element styling, tab order, pinning, tab grouping, the dim sum surprise and the notification switch. It cannot be undone, and it touches nothing outside this site.',
        'Wipes every preference this site keeps in this browser: language mode, both funny levels, theme, density, accent, fonts, per-element styling, tab order, pinning, tab grouping, the dim sum surprise and the notification switch. No undo, and nothing outside this site is touched.',
        'Wipes the lot — language mode, both funny levels, theme, density, accent, fonts, per-element styling, tab order, pinning, tab grouping, the dim sum surprise, the notification switch. There is no undo. Nothing outside this site is touched.',
        'Wipes the lot: language mode, both funny levels, theme, density, accent, fonts, per-element styling, tab order, pinning, tab grouping, the dim sum surprise and the notification switch. There is no undo button, no second thoughts, and nothing outside this site is touched.'
      ],
      yue: [
        '會刪除本站存喺呢部瀏覽器嘅所有偏好：語言模式、兩條搞笑滑桿、主題、密度、重點色、字型、個別元件樣式、分頁次序、釘住狀態、分頁分組、點心驚喜同通知開關。無法復原，亦唔會影響本站以外任何嘢。',
        '會刪除本站存喺呢部瀏覽器嘅所有偏好 — 語言模式、兩條搞笑滑桿、主題、密度、重點色、字型、個別元件樣式、分頁次序、釘住、分頁分組、點心驚喜同通知開關。無法復原，本站以外嘅嘢一律唔碰。',
        '會清走本站喺呢部瀏覽器嘅所有偏好：語言模式、兩條搞笑滑桿、主題、密度、重點色、字型、元件樣式、分頁次序、釘住、分頁分組、點心驚喜同通知開關。冇得返轉頭，本站以外嘢一律唔郁。',
        '一次過清走晒 — 語言模式、兩條搞笑滑桿、主題、密度、重點色、字型、元件樣式、分頁次序、釘住、分頁分組、點心驚喜、通知開關。冇得返轉頭。本站以外嘅嘢一律唔郁。',
        '一次過清走晒：語言模式、兩條搞笑滑桿、主題、密度、重點色、字型、元件樣式、分頁次序、釘住、分頁分組、點心驚喜同通知開關。冇 undo、冇後悔藥，本站以外嘅嘢一條毛都唔會碰。'
      ]
    },
    'settings.reset.confirm': { en: ['Delete stored settings'], yue: ['刪除已儲存設定'] },
    'settings.reset.cancel': { en: ['Keep them'], yue: ['保留'] },
    'settings.storage.blocked': {
      en: [
        'This browser is blocking local storage, so settings apply now but will not survive a reload. Everything else on the page works normally.',
        'This browser is blocking local storage, so your settings apply now but will not survive a reload. Everything else works normally.',
        'Local storage is blocked here, so your settings apply right now but will be gone after a reload. Nothing else is affected.',
        'Local storage is blocked in this browser, so settings apply now and vanish on reload. Nothing else is affected — it is a storage permission, not a bug in the page.',
        'Local storage is bolted shut in this browser, so your settings apply right now and evaporate on reload. Nothing else is affected — that is a storage permission, not the page misbehaving.'
      ],
      yue: [
        '呢部瀏覽器封鎖咗本機儲存，所以設定即刻生效，但重新載入之後就唔會留低。其他功能一切正常。',
        '呢部瀏覽器封鎖咗本機儲存，設定即刻生效，但重新載入就唔見咗。其他功能一切正常。',
        '呢度嘅本機儲存被封鎖，設定即刻生效但 reload 之後會冇咗。其他嘢冇受影響。',
        '呢部瀏覽器封鎖咗本機儲存，設定即刻生效、reload 即刻蒸發。其他嘢冇受影響 — 呢個係儲存權限問題，唔係版面壞咗。',
        '呢部瀏覽器將本機儲存封到實一實，設定即刻生效、一 reload 即刻蒸發。其他嘢冇受影響 — 係儲存權限問題，唔係版面出事。'
      ]
    },

    /* ------------------------------------------------------- notifications */
    'notify.saved': {
      en: ['Setting saved: {name}', 'Setting saved: {name}', 'Saved: {name}', 'Noted: {name}', 'Noted and filed: {name}'],
      yue: ['已儲存設定：{name}', '已儲存設定：{name}', '已儲存：{name}', '記低咗：{name}', '記低咗，仲入咗檔：{name}']
    },
    'notify.reset.done': {
      en: [
        'All stored settings were deleted and the defaults are back.',
        'All stored settings were deleted and the defaults are back.',
        'Every stored setting is gone and the defaults are back. Clean slate.',
        'Every stored setting is gone and the defaults are back. Clean slate, no leftovers.',
        'Every stored setting has been shown the door and the defaults have moved back in. Clean slate, no leftovers.'
      ],
      yue: [
        '所有已儲存設定已刪除，回復預設值。',
        '所有已儲存設定已刪除，回復預設值。',
        '所有儲存設定已經清走，預設值返晒嚟。乾乾淨淨。',
        '所有儲存設定清走晒，預設值搬返入嚟。乾乾淨淨，冇手尾。',
        '所有儲存設定被請出門口，預設值搬返晒入嚟。乾乾淨淨，一條手尾都冇。'
      ]
    },
    'notify.copied': {
      en: ['Copied to the clipboard.', 'Copied to the clipboard.', 'Copied — it is on your clipboard.', 'Copied. Go on, paste it.', 'Copied. Your clipboard is now holding it hostage.'],
      yue: ['已複製到剪貼簿。', '已複製到剪貼簿。', '複製咗喇，喺你剪貼簿度。', '複製咗，貼落去啦。', '複製咗，你個剪貼簿而家扣起咗佢。']
    },
    'notify.copyfailed': {
      en: [
        'The browser refused clipboard access, so nothing was copied. The text is selected instead — press Ctrl+C.',
        'The browser refused clipboard access, so nothing was copied. The text is selected for you — press Ctrl+C.',
        'The browser said no to clipboard access, so nothing was copied. The text is selected — Ctrl+C will finish the job.',
        'The browser slammed the clipboard shut, so nothing was copied. The text is selected — Ctrl+C finishes the job.',
        'The browser slammed the clipboard shut, so nothing was copied. The text is selected and waiting — Ctrl+C finishes the job by hand.'
      ],
      yue: [
        '瀏覽器拒絕存取剪貼簿，所以冇複製到。文字已經幫你選好，撳 Ctrl+C 就得。',
        '瀏覽器拒絕存取剪貼簿，冇複製到。文字已經選好，撳 Ctrl+C。',
        '瀏覽器唔俾掂剪貼簿，所以冇複製到。文字已經選好 — Ctrl+C 收尾。',
        '瀏覽器一手閂咗剪貼簿，所以冇複製到。文字已經選好 — Ctrl+C 幫你收尾。',
        '瀏覽器一手閂實剪貼簿，所以乜都冇複製到。文字已經選好喺度等 — Ctrl+C 自己動手收尾。'
      ]
    },
    'notify.exported': {
      en: ['Exported {count} release(s) as Markdown.', 'Exported {count} release(s) as Markdown.', '{count} release(s) exported as Markdown.', '{count} release(s) packed into Markdown.', '{count} release(s) packed into Markdown and sent to your downloads.'],
      yue: ['已將 {count} 個版本匯出成 Markdown。', '已將 {count} 個版本匯出成 Markdown。', '{count} 個版本已匯出成 Markdown。', '{count} 個版本打包成 Markdown。', '{count} 個版本打包成 Markdown，送咗去你嘅下載資料夾。']
    },
    'notify.tab.pinned': { en: ['Pinned {name}.'], yue: ['已釘住「{name}」。'] },
    'notify.tab.unpinned': { en: ['Unpinned {name}.'], yue: ['已解除釘住「{name}」。'] },
    'notify.tab.reset': { en: ['Tab order and pinning restored to the defaults.'], yue: ['分頁次序同釘住已回復預設。'] },
    'notify.dimsum.off': { en: ['The dim sum surprise is off.'], yue: ['點心驚喜已關閉。'] },

    /* --------------------------------------------------------------- dimsum */
    'dimsum.badge': { en: ['Dim sum surprise'], yue: ['點心驚喜'] },
    'dimsum.line': {
      en: [
        'A one-in-a-hundred visit. Today it is {dish}.',
        'A one-in-a-hundred visit. Today it is {dish}.',
        'One visit in a hundred gets a dish. Yours is {dish}.',
        'One visit in a hundred gets a dish, and yours is {dish}. Enjoy.',
        'One visit in a hundred gets a dish, and the trolley stopped at yours: {dish}. Eat it before it goes cold.'
      ],
      yue: [
        '百分之一嘅機會。今次係{dish}。',
        '百分之一嘅機會。今次係{dish}。',
        '一百次入面得一次有點心，你今次係{dish}。',
        '一百次入面得一次有點心，你今次抽到{dish}。慢用。',
        '一百次入面得一次有點心，架點心車啱啱停咗喺你度：{dish}。趁熱食啦。'
      ]
    },
    'dimsum.dismiss': { en: ['Dismiss'], yue: ['關閉'] },
    'dimsum.turnoff': { en: ['Turn this off'], yue: ['關閉呢個功能'] },

    /* --------------------------------------------------------------- build */
    'build.heading': {
      en: ['How it is built', 'How it is built', 'How this thing was actually built'],
      yue: ['點樣砌出嚟', '點樣砌出嚟', '呢舊嘢實際係點砌出嚟']
    },
    'build.body': {
      en: [
        'A design imported from Claude, implemented as real code by orchestrating the Codex CLI, then verified against a runtime layout matrix.',
        'A design imported from Claude, implemented as real code by orchestrating the Codex CLI, then verified against a runtime layout matrix.',
        'Designed in Claude, coded by orchestrating the Codex CLI across git worktrees, then held to a runtime layout matrix that fails the build over a single clipped pixel.'
      ],
      yue: [
        '設計由 Claude 匯入，再指揮 Codex CLI 寫成真實程式碼，最後用執行期版面矩陣驗證。',
        '設計由 Claude 匯入，再指揮 Codex CLI 寫成真實程式碼，最後用執行期版面矩陣驗證。',
        '設計喺 Claude 度做，再指揮 Codex CLI 喺多個 git worktree 並行寫 code，最後交俾執行期版面矩陣審 — 差一個 pixel 都當 fail。'
      ]
    },
    'build.step1.title': { en: ['Imported the MD3 design'], yue: ['匯入 MD3 設計'] },
    'build.step1.body': {
      en: ['A full desktop-shell Material 3 design of the real Bambu Studio UI — nine screens, dialogs and tokens.'],
      yue: ['一套完整嘅桌面 Material 3 設計，覆蓋真實 Bambu Studio 介面 — 九版畫面、對話框同 token。']
    },
    'build.step2.title': { en: ['Tiny vanilla runtime'], yue: ['細細粒嘅原生執行環境'] },
    'build.step2.body': {
      en: ['A dependency-free engine reimplements the design’s template dialect with keyed patching that preserves focus and caret.'],
      yue: ['一個零依賴引擎重新實作咗設計嘅樣板語法，用鍵值修補保住焦點同游標位置。']
    },
    'build.step3.title': { en: ['Coded by the Codex CLI'], yue: ['由 Codex CLI 寫成'] },
    'build.step3.body': {
      en: ['Screens built in parallel across git worktrees, orchestrated agent by agent, then assembled and QA’d.'],
      yue: ['多個畫面喺 git worktree 並行開發，逐個代理指揮，之後合併同做 QA。']
    },
    'build.step4.title': { en: ['Checked at 444 layout cases'], yue: ['444 格版面測試'] },
    'build.step4.body': {
      en: ['156 landing cases — thirteen widths by four display scales by three language modes — plus 288 more that activate every tab, all driven in a real browser. One clipped element, one undersized control or one empty panel fails the deploy.'],
      yue: ['156 格首頁測試 — 十三種闊度 × 四種顯示比例 × 三種語言模式 — 再加 288 格逐個分頁㩒一次，全部喺真實瀏覽器行。一個元件切字、一個掣細過標準、一版空白，就當部署失敗。']
    },

    /* -------------------------------------------------------------- footer */
    'footer.launch': { en: ['Launch app'], yue: ['開啟應用'] },
    'footer.source': { en: ['Source'], yue: ['原始碼'] },
    'footer.downloads': { en: ['Downloads'], yue: ['下載'] },
    'footer.upstream': { en: ['Upstream project'], yue: ['上游專案'] },
    'footer.disclaimer': {
      en: ['An independent, open-source Material Design 3 concept redesign built on a fork of the open-source Bambu Studio project. Not affiliated with, authorized by, or endorsed by Bambu Lab. “Bambu Studio”, “Bambu Lab” and related names are trademarks of their respective owners; they are used here only to describe the software being restyled. “Material Design”, “Material You” and “Roboto” are trademarks of Google LLC.'],
      yue: ['呢個係一個獨立、開源嘅 Material Design 3 概念重新設計，建基於開源 Bambu Studio 專案嘅分支。同 Bambu Lab 冇任何從屬、授權或者背書關係。「Bambu Studio」、「Bambu Lab」及相關名稱屬各自擁有人嘅商標，喺呢度只用嚟描述被重新設計嘅軟件。「Material Design」、「Material You」同「Roboto」係 Google LLC 嘅商標。']
    }
  };

  global.BAMBU_SITE_COPY = {
    levels: 5,
    entries: copy
  };
})(typeof window !== 'undefined' ? window : globalThis);
