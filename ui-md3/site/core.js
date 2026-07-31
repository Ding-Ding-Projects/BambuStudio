/*
 * Site core: preference storage, copy resolution, theming and notifications.
 *
 * Everything here is dependency-free and synchronous. Preferences live in this
 * browser's localStorage and nowhere else; when storage is blocked the site
 * still works for the session and says so once, in a notification, instead of
 * failing silently or pretending the setting was saved.
 */
(function (global) {
  'use strict';

  var STORAGE_KEY = 'bambuStudio.site.v1';
  var LEVELS = 5;
  var i18n = global.BambuI18n || null;
  // The language mode is shared with the interactive app, so its runtime owns
  // selection and persistence; initialising here keeps `?lang=` working before
  // the first line of copy is rendered.
  if (i18n && i18n.initialize) {
    try { i18n.initialize({ search: global.location.search }); } catch (error) { /* file:// */ }
  }

  var DEFAULTS = {
    funnyEn: 3,
    funnyYue: 4,
    theme: 'dark',
    density: 'comfortable',
    accent: '#22c55e',
    fontFamily: 'roboto',
    fontScale: 100,
    fontWeight: 400,
    dimSum: true,
    notifications: true,
    tabOrder: [],
    tabPinned: [],
    tabGroups: {},
    activeTab: '',
    elementStyles: {}
  };

  /* ------------------------------------------------------------ storage */

  var storageBlocked = false;
  function readStorage() {
    try {
      var raw = global.localStorage.getItem(STORAGE_KEY);
      return raw ? JSON.parse(raw) : {};
    } catch (error) {
      storageBlocked = true;
      return {};
    }
  }
  function writeStorage(value) {
    try {
      global.localStorage.setItem(STORAGE_KEY, JSON.stringify(value));
      return true;
    } catch (error) {
      storageBlocked = true;
      return false;
    }
  }

  var state = (function () {
    var stored = readStorage();
    var merged = {};
    Object.keys(DEFAULTS).forEach(function (key) {
      merged[key] = stored[key] === undefined ? clone(DEFAULTS[key]) : stored[key];
    });
    return merged;
  })();

  function clone(value) {
    return typeof value === 'object' && value !== null ? JSON.parse(JSON.stringify(value)) : value;
  }

  var listeners = [];
  function subscribe(listener) {
    listeners.push(listener);
    return function () {
      listeners = listeners.filter(function (candidate) { return candidate !== listener; });
    };
  }
  function emit(keys) {
    listeners.forEach(function (listener) {
      try { listener(keys); } catch (error) { /* one bad listener must not stop the rest */ }
    });
  }

  function get(key) {
    return state[key];
  }
  function set(key, value, options) {
    if (JSON.stringify(state[key]) === JSON.stringify(value)) return false;
    state[key] = value;
    writeStorage(state);
    if (!options || options.emit !== false) emit([key]);
    return true;
  }
  function resetAll() {
    Object.keys(DEFAULTS).forEach(function (key) { state[key] = clone(DEFAULTS[key]); });
    try {
      global.localStorage.removeItem(STORAGE_KEY);
      global.localStorage.removeItem(HISTORY_KEY);
    } catch (error) { storageBlocked = true; }
    history = [];
    if (i18n) i18n.setActiveMode(i18n.getDefaultMode ? i18n.getDefaultMode() : 'en', { persist: true });
    emit(Object.keys(DEFAULTS).concat(['languageMode']));
  }

  function languageMode() {
    return i18n ? i18n.getActiveMode() : 'en';
  }
  function setLanguageMode(mode) {
    if (i18n) i18n.setActiveMode(mode, { persist: true });
    emit(['languageMode']);
  }

  /* --------------------------------------------------------------- copy */

  var catalog = (global.BAMBU_SITE_COPY && global.BAMBU_SITE_COPY.entries) || {};

  /*
   * Ladder expansion. A language may supply 1..5 variants; the active funny
   * level picks one. Fewer variants simply widen the bands, which is why an
   * atomic label with a single variant reads identically at every level.
   */
  function variantIndex(count, level) {
    var bounded = Math.min(LEVELS, Math.max(1, level | 0));
    if (count >= LEVELS) return bounded - 1;
    if (count <= 1) return 0;
    if (count === 2) return bounded <= 2 ? 0 : 1;
    if (count === 3) return bounded <= 2 ? 0 : (bounded === 3 ? 1 : 2);
    return bounded <= 2 ? 0 : bounded - 2; // four variants: 1-2, 3, 4, 5
  }

  function interpolate(text, params) {
    if (!params) return text;
    return String(text).replace(/\{([A-Za-z0-9_]+)\}/g, function (whole, key) {
      return params[key] === undefined ? whole : String(params[key]);
    });
  }

  function variantFor(key, language, params) {
    var entry = catalog[key];
    if (!entry) return '';
    var variants = entry[language] || entry.en || [];
    if (!variants.length) return '';
    var level = language === 'yue' ? get('funnyYue') : get('funnyEn');
    return interpolate(variants[variantIndex(variants.length, level)], params);
  }

  /** One key at an explicit language and level — used by the settings preview. */
  function preview(key, language, level, params) {
    var entry = catalog[key];
    if (!entry) return '';
    var variants = entry[language] || [];
    if (!variants.length) return '';
    return interpolate(variants[variantIndex(variants.length, level)], params);
  }

  /** English and Cantonese for one key, each already at its own funny level. */
  function pair(key, params) {
    return { en: variantFor(key, 'en', params), yue: variantFor(key, 'yue', params) };
  }

  /**
   * The single string this key renders as in the active language mode.
   * Bilingual mode composes both, separated by a slash: callers that write
   * straight into textContent have nowhere to put a companion element, and
   * returning English alone would silently drop Cantonese from every status
   * line and error message on the site.
   */
  function text(key, params) {
    var both = pair(key, params);
    var mode = languageMode();
    if (mode === 'yue_HK') return both.yue || both.en;
    if (mode === 'bilingual_en_yue_HK' && both.yue && both.yue !== both.en) {
      return both.en + ' / ' + both.yue;
    }
    return both.en;
  }

  /**
   * Binds a key to a node instead of stamping a string into it, so the node
   * re-renders on a language or funny-level change like every other label.
   * Passing no key clears both the binding and the text.
   */
  function setCopy(node, key, params) {
    if (!node) return;
    if (!key) {
      node.removeAttribute('data-copy');
      node.removeAttribute('data-copy-params');
      node.textContent = '';
      return;
    }
    node.setAttribute('data-copy', key);
    if (params) node.setAttribute('data-copy-params', JSON.stringify(params));
    else node.removeAttribute('data-copy-params');
    applyCopy(node.parentNode || node);
  }

  /** The Cantonese companion shown in bilingual mode, or '' when not applicable. */
  function secondary(key, params) {
    if (languageMode() !== 'bilingual_en_yue_HK') return '';
    var both = pair(key, params);
    return both.yue && both.yue !== both.en ? both.yue : '';
  }

  function known(key) {
    return Object.prototype.hasOwnProperty.call(catalog, key);
  }

  /* Short companions sit inline; long ones become their own quiet line so a
   * narrow viewport wraps them instead of squeezing the primary label. */
  var INLINE_LIMIT = 26;

  function applyCopy(root) {
    var scope = root || global.document;
    var mode = languageMode();
    scope.querySelectorAll('[data-copy]').forEach(function (element) {
      var key = element.getAttribute('data-copy');
      var params = parseParams(element.getAttribute('data-copy-params'));
      var both = pair(key, params);
      /*
       * `text()` composes "English / 廣東話" for callers that write into a
       * textContent and have nowhere to put a companion element. This is not
       * one of those callers: it renders the companion itself, so taking the
       * composed form here printed the Cantonese twice on every label — and
       * doubled label widths pushed the tab strip into icons-only at 1280px.
       */
      var primary = mode === 'yue_HK' ? (both.yue || both.en) : both.en;
      var companion = secondary(key, params);
      element.textContent = '';
      var main = global.document.createElement('span');
      main.className = 'copy-main';
      main.textContent = primary;
      element.appendChild(main);
      if (companion) {
        var extra = global.document.createElement('span');
        extra.className = 'copy-secondary' + (companion.length <= INLINE_LIMIT ? ' inline' : ' block');
        extra.lang = 'yue-Hant-HK';
        extra.textContent = companion;
        element.appendChild(extra);
      }
      element.setAttribute('lang', mode === 'yue_HK' ? 'yue-Hant-HK' : 'en');
    });
    scope.querySelectorAll('[data-copy-attr]').forEach(function (element) {
      var params = parseParams(element.getAttribute('data-copy-params'));
      element.getAttribute('data-copy-attr').split(';').forEach(function (rule) {
        var parts = rule.split(':');
        if (parts.length !== 2) return;
        var attribute = parts[0].trim();
        var key = parts[1].trim();
        var both = pair(key, params);
        var value = mode === 'yue_HK' ? (both.yue || both.en) : both.en;
        if (mode === 'bilingual_en_yue_HK' && both.yue && both.yue !== both.en) {
          value = both.en + ' / ' + both.yue;
        }
        element.setAttribute(attribute, value);
      });
    });
  }

  function parseParams(raw) {
    if (!raw) return null;
    try { return JSON.parse(raw); } catch (error) { return null; }
  }

  /* ------------------------------------------------------------ theming */

  function hexToHsl(hex) {
    var normalized = String(hex || '').replace('#', '');
    if (normalized.length === 3) {
      normalized = normalized.split('').map(function (c) { return c + c; }).join('');
    }
    var r = parseInt(normalized.slice(0, 2), 16) / 255;
    var g = parseInt(normalized.slice(2, 4), 16) / 255;
    var b = parseInt(normalized.slice(4, 6), 16) / 255;
    if ([r, g, b].some(isNaN)) return { h: 140, s: 60, l: 45 };
    var max = Math.max(r, g, b);
    var min = Math.min(r, g, b);
    var hue = 0;
    var saturation = 0;
    var lightness = (max + min) / 2;
    if (max !== min) {
      var delta = max - min;
      saturation = lightness > 0.5 ? delta / (2 - max - min) : delta / (max + min);
      if (max === r) hue = (g - b) / delta + (g < b ? 6 : 0);
      else if (max === g) hue = (b - r) / delta + 2;
      else hue = (r - g) / delta + 4;
      hue /= 6;
    }
    return {
      h: Math.round(hue * 360),
      s: Math.round(saturation * 100),
      l: Math.round(lightness * 100)
    };
  }

  var FONT_STACKS = {
    roboto: "'Roboto'",
    system: 'system-ui',
    segoe: "'Segoe UI'",
    arial: 'Arial',
    georgia: 'Georgia',
    mono: "'Roboto Mono'"
  };
  // Latin faces are useless for Cantonese, so a CJK-capable stack always follows.
  var CJK_FALLBACK = "'Noto Sans HK','PingFang HK','Microsoft JhengHei','Microsoft YaHei',sans-serif";

  function applyAppearance() {
    var root = global.document.documentElement;
    root.setAttribute('data-theme', get('theme'));
    root.setAttribute('data-density', get('density'));

    var seed = hexToHsl(get('accent'));
    var hue = seed.h;
    var saturation = Math.max(32, Math.min(92, seed.s));
    var dark = get('theme') === 'dark';
    var tone = function (lightness) { return 'hsl(' + hue + ' ' + saturation + '% ' + lightness + '%)'; };
    root.style.setProperty('--md-primary', dark ? tone(76) : tone(36));
    root.style.setProperty('--md-on-primary', dark ? tone(16) : '#ffffff');
    root.style.setProperty(
      '--md-primary-container',
      'hsl(' + hue + ' ' + Math.round(saturation * (dark ? 0.9 : 0.7)) + '% ' + (dark ? 28 : 88) + '%)'
    );
    root.style.setProperty('--md-on-primary-container', dark ? tone(90) : tone(12));
    root.style.setProperty(
      '--md-secondary-container',
      'hsl(' + hue + ' ' + Math.round(saturation * (dark ? 0.35 : 0.45)) + '% ' + (dark ? 26 : 90) + '%)'
    );
    root.style.setProperty('--md-on-secondary-container', dark ? tone(88) : tone(20));

    var family = FONT_STACKS[get('fontFamily')] || FONT_STACKS.roboto;
    root.style.setProperty('--site-font', family + ',' + CJK_FALLBACK);
    root.style.setProperty('--site-font-scale', (Math.max(75, Math.min(160, get('fontScale'))) / 100).toFixed(3));
    root.style.setProperty('--site-font-weight', String(get('fontWeight')));

    var elements = get('elementStyles') || {};
    Object.keys(elements).forEach(function (element) {
      var values = elements[element] || {};
      Object.keys(values).forEach(function (property) {
        if (values[property] === '' || values[property] === null) return;
        root.style.setProperty('--el-' + element + '-' + property, String(values[property]));
      });
    });
    applyPerTabStyles(elements);
  }

  /*
   * A single tab is styled directly rather than through a custom property: CSS
   * cannot compose a variable name from an attribute, so per-tab properties
   * would otherwise need one hand-written rule per tab per property. Writing
   * the element's own style keeps the editor honest — what you set is what the
   * tab gets — and an unset property is removed rather than left behind.
   */
  var TAB_STYLE_PROPERTIES = {
    radius: 'borderRadius',
    size: 'fontSize',
    color: 'color',
    font: 'fontFamily',
    weight: 'fontWeight',
    spacing: 'padding'
  };

  function applyPerTabStyles(elements) {
    var tabs = global.document.querySelectorAll('#tabstrip .tab');
    for (var index = 0; index < tabs.length; index++) {
      var tab = tabs[index];
      var values = elements['tab-' + tab.dataset.tab] || {};
      Object.keys(TAB_STYLE_PROPERTIES).forEach(function (property) {
        var name = TAB_STYLE_PROPERTIES[property];
        var value = values[property];
        if (value === undefined || value === '' || value === null) {
          tab.style[name] = '';
          return;
        }
        // `size` is stored unitless, like every other multiplier on the site.
        tab.style[name] = property === 'size'
          ? 'calc(' + (13.5 * Number(value)).toFixed(2) + 'px * var(--site-font-scale))'
          : String(value);
      });
    }
  }

  function elementStyle(element, property, value) {
    var all = clone(get('elementStyles') || {});
    all[element] = all[element] || {};
    if (value === null) delete all[element][property];
    else all[element][property] = value;
    if (!Object.keys(all[element]).length) delete all[element];
    set('elementStyles', all);
    if (value === null) {
      global.document.documentElement.style.removeProperty('--el-' + element + '-' + property);
    }
    applyAppearance();
  }

  function resetElement(element) {
    var all = clone(get('elementStyles') || {});
    var removed = all[element] ? Object.keys(all[element]) : [];
    delete all[element];
    set('elementStyles', all);
    removed.forEach(function (property) {
      global.document.documentElement.style.removeProperty('--el-' + element + '-' + property);
    });
    applyAppearance();
  }

  /* ------------------------------------------------------ notifications */

  /*
   * The centre survives a reload. A notification that only exists until the
   * page is refreshed is not a history, and the one thing a user reaches the
   * centre for is the message they missed — often after reloading to see
   * whether the setting really took.
   *
   * Records store their copy key and parameters, never rendered text, so a
   * restored history re-renders in whatever language and funny level is active
   * now rather than freezing the wording it was written in.
   */
  var HISTORY_KEY = 'bambuStudio.site.notifications.v1';
  var HISTORY_LIMIT = 50;
  var history = (function () {
    try {
      var raw = global.localStorage.getItem(HISTORY_KEY);
      var parsed = raw ? JSON.parse(raw) : [];
      return Array.isArray(parsed) ? parsed.slice(0, HISTORY_LIMIT) : [];
    } catch (error) {
      return [];
    }
  })();
  function persistHistory() {
    try {
      global.localStorage.setItem(HISTORY_KEY, JSON.stringify(history));
    } catch (error) {
      storageBlocked = true;
    }
  }
  var toastHost = null;
  var AUTO_DISMISS = { info: 6000, success: 5000 };

  function ensureHost() {
    if (toastHost && global.document.body.contains(toastHost)) return toastHost;
    toastHost = global.document.createElement('div');
    toastHost.className = 'toast-host';
    // No live region on the host: each toast carries its own role (status or
    // alert). Nesting one inside the other makes assistive technology announce
    // twice and lets the polite host soften an error's urgency.
    global.document.body.appendChild(toastHost);
    return toastHost;
  }

  /**
   * kind: 'info' | 'success' | 'warning' | 'error'
   * Warnings and errors never auto-dismiss; the funny level styles their voice
   * but the caller's facts (file, count, engine message) come through params.
   */
  function notify(kind, key, params, options) {
    var settings = options || {};
    var record = {
      kind: kind,
      key: key,
      params: params || null,
      at: new Date().toISOString(),
      action: settings.action || null
    };
    history.unshift(record);
    if (history.length > HISTORY_LIMIT) history.length = HISTORY_LIMIT;
    persistHistory();
    emit(['notifications']);
    if (!get('notifications') && kind !== 'error' && kind !== 'warning') return record;
    renderToast(record);
    return record;
  }

  function renderToast(record) {
    var host = ensureHost();
    var toast = global.document.createElement('div');
    toast.className = 'toast toast-' + record.kind;
    toast.setAttribute('role', record.kind === 'error' || record.kind === 'warning' ? 'alert' : 'status');

    var icon = global.document.createElement('span');
    icon.className = 'toast-icon';
    icon.setAttribute('data-icon', '');
    icon.setAttribute('aria-hidden', 'true');
    icon.textContent = {
      info: 'info', success: 'check_circle', warning: 'warning', error: 'error'
    }[record.kind] || 'info';

    var body = global.document.createElement('div');
    body.className = 'toast-body';
    var message = global.document.createElement('p');
    message.className = 'toast-text';
    message.setAttribute('data-copy', record.key);
    if (record.params) message.setAttribute('data-copy-params', JSON.stringify(record.params));
    body.appendChild(message);

    if (record.action) {
      var action = global.document.createElement('button');
      action.type = 'button';
      action.className = 'toast-action';
      action.setAttribute('data-copy', record.action.key);
      action.addEventListener('click', function () {
        record.action.run();
        dismiss();
      });
      body.appendChild(action);
    }

    var close = global.document.createElement('button');
    close.type = 'button';
    close.className = 'toast-close iconbtn';
    close.setAttribute('data-copy-attr', 'aria-label:shell.notifications.dismiss');
    close.innerHTML = '<span data-icon aria-hidden="true">close</span>';
    close.addEventListener('click', function () { dismiss(); });

    toast.appendChild(icon);
    toast.appendChild(body);
    toast.appendChild(close);
    // Rendered before it is attached, so an alert enters the accessibility tree
    // already carrying its text instead of announcing an empty container first.
    applyCopy(toast);
    host.appendChild(toast);

    var timer = null;
    function dismiss() {
      if (timer) clearTimeout(timer);
      if (toast.parentNode) toast.parentNode.removeChild(toast);
    }
    var delay = AUTO_DISMISS[record.kind];
    if (delay) timer = setTimeout(dismiss, delay);
  }

  function notificationHistory() {
    return history.slice();
  }
  function clearNotifications() {
    history = [];
    persistHistory();
    if (toastHost) toastHost.innerHTML = '';
    emit(['notifications']);
  }

  /* ------------------------------------------------------------ clipboard */

  function copyText(value, element) {
    var done = function () { notify('success', 'notify.copied'); };
    var failed = function () {
      if (element && global.getSelection) {
        var range = global.document.createRange();
        range.selectNodeContents(element);
        var selection = global.getSelection();
        selection.removeAllRanges();
        selection.addRange(range);
      }
      notify('warning', 'notify.copyfailed');
    };
    if (global.navigator && global.navigator.clipboard && global.navigator.clipboard.writeText) {
      global.navigator.clipboard.writeText(value).then(done, failed);
    } else {
      failed();
    }
  }

  function downloadText(filename, value) {
    var blob = new Blob([value], { type: 'text/markdown;charset=utf-8' });
    var url = URL.createObjectURL(blob);
    var link = global.document.createElement('a');
    link.href = url;
    link.download = filename;
    global.document.body.appendChild(link);
    link.click();
    global.document.body.removeChild(link);
    setTimeout(function () { URL.revokeObjectURL(url); }, 1000);
  }

  global.BambuSite = {
    LEVELS: LEVELS,
    DEFAULTS: DEFAULTS,
    get: get,
    set: set,
    resetAll: resetAll,
    subscribe: subscribe,
    emit: emit,
    storageBlocked: function () { return storageBlocked; },
    languageMode: languageMode,
    setLanguageMode: setLanguageMode,
    text: text,
    pair: pair,
    preview: preview,
    secondary: secondary,
    known: known,
    variantIndex: variantIndex,
    applyCopy: applyCopy,
    setCopy: setCopy,
    applyAppearance: applyAppearance,
    elementStyle: elementStyle,
    resetElement: resetElement,
    notify: notify,
    notificationHistory: notificationHistory,
    clearNotifications: clearNotifications,
    copyText: copyText,
    downloadText: downloadText,
    fontStacks: FONT_STACKS
  };
})(typeof window !== 'undefined' ? window : globalThis);
