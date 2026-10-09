// Text of the sender page in English and Hong Kong Cantonese, and the three
// language modes: English, Cantonese, or both (English first, Cantonese on
// its own line). $1 to $9 are substitutions. No network, no DOM: the tests
// import this module directly.

export const MESSAGES = Object.freeze({
  en: Object.freeze({
    appName: 'LAN model drop',
    pageTitle: 'Send a model to Bambu Studio',
    stationLine: 'Sending to $1',
    languageLabel: 'Language',
    formHeading: 'Send models',
    codeLabel: 'Drop code',
    codeHelp: 'Shown in Bambu Studio on the computer, in Preferences, LAN model drop.',
    codeMissing: 'Type the drop code shown in Bambu Studio.',
    codeFormat: 'The drop code is 4 to 12 digits.',
    senderLabel: 'Your name (optional)',
    senderHelp: 'Shown with your models, so the person at the computer knows who sent them.',
    dropTitle: 'Drag models here',
    dropHint: '3MF, STL, STEP, OBJ or AMF, up to $1 each',
    chooseFiles: 'Choose models',
    listLabel: 'Chosen models',
    emptyList: 'No models chosen yet.',
    removeFile: 'Remove $1',
    clearList: 'Clear list',
    send: 'Send',
    sendCount: 'Send $1',
    stateReady: 'Ready',
    stateWaiting: 'Waiting',
    stateSending: 'Sending $1%',
    stateSent: 'Sent',
    stateNotSent: 'Not sent: $1',
    progressLabel: 'Sending $1',
    reasonType: 'not a 3MF, STL, STEP, OBJ or AMF model',
    reasonEmpty: 'the file is empty',
    reasonTooLarge: 'larger than $1',
    reasonWrongCode: 'the drop code is not right',
    reasonLocked: 'too many wrong drop codes',
    reasonFull: 'the drop box is full',
    reasonName: 'the file name was not accepted',
    reasonNetwork: 'the drop box could not be reached',
    reasonServer: 'the drop box answered $1',
    liveAdded: 'Added $1. Ready to send: $2.',
    liveSending: 'Sending $1 of $2: $3',
    liveDone: 'Sent $1 of $2. The person at the computer decides what happens next; nothing is printed by itself.',
    liveWrongCode: 'The drop code is not right. Check it in Bambu Studio and try again.',
    liveLocked: 'Too many wrong drop codes from this device. Try again in $1 minutes.',
    liveFull: 'The drop box is full. Ask the person at the computer to open or discard some models first.',
    liveNetwork: 'Could not reach the drop box. Check that this device is on the same network as the computer.',
    liveNothing: 'Choose at least one model to send.',
    howHeading: 'How it works',
    howStep1: 'On the computer, open Bambu Studio, Preferences, LAN model drop, and read the drop code.',
    howStep2: 'Type the code here, choose your models and send them.',
    howStep3: 'Bambu Studio asks the person at the computer to open or discard each model. Nothing is sliced or printed automatically.',
    privacyHeading: 'Privacy',
    privacyText: 'Models go only to this drop box on your local network. They are removed when Bambu Studio collects them, or after $1 hours. This page loads nothing from the internet and keeps no record of you; only your language choice is remembered on this device.',
    footer: 'Bambu Studio MD3 · LAN model drop · protocol $1',
    unitBytes: '$1 B',
    unitKB: '$1 KB',
    unitMB: '$1 MB',
    unitGB: '$1 GB',
  }),
  yue_HK: Object.freeze({
    appName: 'LAN 模型投遞',
    pageTitle: '傳送模型去 Bambu Studio',
    stationLine: '傳送去 $1',
    languageLabel: '語言',
    formHeading: '傳送模型',
    codeLabel: '投遞碼',
    codeHelp: '喺部電腦嘅 Bambu Studio 入面，「偏好設定」嘅「LAN 模型投遞」會顯示。',
    codeMissing: '請輸入 Bambu Studio 顯示嘅投遞碼。',
    codeFormat: '投遞碼係 4 至 12 個數字。',
    senderLabel: '你嘅名（可以唔填）',
    senderHelp: '會同你嘅模型一齊顯示，等用緊部電腦嘅人知道係邊個傳嘅。',
    dropTitle: '將模型拖入嚟呢度',
    dropHint: '3MF、STL、STEP、OBJ 或者 AMF，每個最多 $1',
    chooseFiles: '揀模型',
    listLabel: '已揀嘅模型',
    emptyList: '仲未揀任何模型。',
    removeFile: '移除 $1',
    clearList: '清除清單',
    send: '傳送',
    sendCount: '傳送 $1 個',
    stateReady: '準備好',
    stateWaiting: '排緊隊',
    stateSending: '傳送緊 $1%',
    stateSent: '已傳送',
    stateNotSent: '冇傳送：$1',
    progressLabel: '傳送緊 $1',
    reasonType: '唔係 3MF、STL、STEP、OBJ 或者 AMF 模型',
    reasonEmpty: '檔案係空嘅',
    reasonTooLarge: '大過 $1',
    reasonWrongCode: '投遞碼唔啱',
    reasonLocked: '錯咗太多次投遞碼',
    reasonFull: '投遞箱滿咗',
    reasonName: '檔名唔獲接受',
    reasonNetwork: '連唔到投遞箱',
    reasonServer: '投遞箱回覆 $1',
    liveAdded: '加咗 $1 個。可以傳送：$2 個。',
    liveSending: '傳送緊第 $1 個（共 $2 個）：$3',
    liveDone: '傳送咗 $1 個（共 $2 個）。之後點處理由用緊部電腦嘅人決定；唔會自己打印。',
    liveWrongCode: '投遞碼唔啱。請喺 Bambu Studio 再睇清楚，然後再試。',
    liveLocked: '呢部裝置錯咗太多次投遞碼。請 $1 分鐘後再試。',
    liveFull: '投遞箱滿咗。請先叫用緊部電腦嘅人開啟或者棄置部分模型。',
    liveNetwork: '連唔到投遞箱。請檢查呢部裝置同部電腦係咪喺同一個網絡。',
    liveNothing: '請至少揀一個模型嚟傳送。',
    howHeading: '點樣運作',
    howStep1: '喺部電腦打開 Bambu Studio，去「偏好設定」嘅「LAN 模型投遞」，睇投遞碼。',
    howStep2: '喺呢度輸入投遞碼，揀好模型再傳送。',
    howStep3: 'Bambu Studio 會問用緊部電腦嘅人要開啟定棄置每個模型。唔會自動切片或者打印。',
    privacyHeading: '私隱',
    privacyText: '模型只會去你本地網絡上面呢個投遞箱。Bambu Studio 攞走之後，或者 $1 個鐘之後，就會被移除。呢個網頁唔會由互聯網載入任何嘢，亦唔會記錄你；呢部裝置只會記住你揀嘅語言。',
    footer: 'Bambu Studio MD3 · LAN 模型投遞 · 協定 $1',
    unitBytes: '$1 B',
    unitKB: '$1 KB',
    unitMB: '$1 MB',
    unitGB: '$1 GB',
  }),
});

export const LANGUAGE_MODES = Object.freeze([
  Object.freeze({ id: 'en', label: 'English', lang: 'en' }),
  Object.freeze({ id: 'yue_HK', label: '粵語', lang: 'yue-HK' }),
  Object.freeze({ id: 'bilingual', label: 'English + 粵語', lang: 'en' }),
]);

export function normalizeMode(value) {
  return LANGUAGE_MODES.some((mode) => mode.id === value) ? value : null;
}

// The browser's own preference decides first: Cantonese or Hong Kong
// Chinese gets Cantonese, everything else English.
export function modeFromLanguages(languages) {
  for (const raw of languages ?? []) {
    const tag = String(raw).replace(/_/g, '-').toLowerCase();
    if (tag === 'yue' || tag.startsWith('yue-') || tag === 'zh-hk' || tag.startsWith('zh-hant-hk') || tag === 'zh-yue') return 'yue_HK';
    if (tag === 'en' || tag.startsWith('en-')) return 'en';
  }
  return 'en';
}

export function format(template, values = []) {
  const list = Array.isArray(values) ? values : [values];
  return String(template).replace(/\$([1-9])/g, (match, index) => {
    const value = list[Number(index) - 1];
    return value === undefined || value === null ? '' : String(value);
  });
}

// One message in the chosen mode. A substitution written { key, values } is
// translated into the same language as the message around it.
export function createTranslator(mode) {
  const chosen = normalizeMode(mode) ?? 'en';
  const english = MESSAGES.en;
  const cantonese = MESSAGES.yue_HK;
  const resolve = (values, inLanguage) => (Array.isArray(values) ? values : [values])
    .map((value) => (value && typeof value === 'object' && typeof value.key === 'string' ? inLanguage(value.key, value.values) : value));
  const en = (key, values) => format(english[key] ?? key, resolve(values, en));
  const yue = (key, values) => format(cantonese[key] ?? english[key] ?? key, resolve(values, yue));

  function parts(key, values) {
    if (chosen === 'yue_HK') return { primary: yue(key, values), primaryLang: 'yue-HK', secondary: null };
    if (chosen === 'bilingual') {
      const first = en(key, values);
      const second = yue(key, values);
      return { primary: first, primaryLang: 'en', secondary: second === first ? null : second, secondaryLang: 'yue-HK' };
    }
    return { primary: en(key, values), primaryLang: 'en', secondary: null };
  }

  // Single-line places (title, labels read aloud, placeholders) join the
  // two languages with " / ".
  function text(key, values) {
    const { primary, secondary } = parts(key, values);
    return secondary ? `${primary} / ${secondary}` : primary;
  }

  const htmlLang = chosen === 'yue_HK' ? 'yue-HK' : 'en';
  return { mode: chosen, htmlLang, parts, text };
}

// File sizes in the units people expect on a phone: 1 KB = 1024 bytes.
export function sizeParts(bytes) {
  if (bytes < 1024) return { key: 'unitBytes', value: String(bytes) };
  const units = [['unitKB', 1024], ['unitMB', 1024 ** 2], ['unitGB', 1024 ** 3]];
  let chosen = units[0];
  for (const unit of units) if (bytes >= unit[1]) chosen = unit;
  const amount = bytes / chosen[1];
  const value = amount >= 100 ? amount.toFixed(0) : amount.toFixed(1).replace(/\.0$/, '');
  return { key: chosen[0], value };
}
