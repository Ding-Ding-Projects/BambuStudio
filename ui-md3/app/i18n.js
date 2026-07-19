/*
 * Dependency-free localization runtime for the MD3 browser prototype.
 * Resources live in i18n.resources.js; this file owns mode selection,
 * persistence, fallback, formatting, and bilingual presentation metadata.
 */
(function initBambuI18n(global) {
  'use strict';

  var resources = global.BAMBU_I18N_RESOURCES || {
    defaultMode: 'en', storageKey: 'bambuStudio.languageMode.v1',
    modes: [{ id: 'en', label: 'English', htmlLang: 'en' }],
    catalog: {}, patterns: [], messages: {}
  };
  var modeById = Object.create(null);
  resources.modes.forEach(function(mode) { modeById[mode.id] = mode; });

  var aliases = {
    en: 'en', english: 'en',
    yue: 'yue_HK', 'yue-hk': 'yue_HK', yue_hk: 'yue_HK',
    cantonese: 'yue_HK', 'hong-kong-cantonese': 'yue_HK',
    'zh-hk': 'yue_HK', zh_hk: 'yue_HK',
    bilingual: 'bilingual_en_yue_HK', both: 'bilingual_en_yue_HK',
    'en+yue': 'bilingual_en_yue_HK', 'en-yue': 'bilingual_en_yue_HK',
    bilingual_en_yue_hk: 'bilingual_en_yue_HK'
  };
  var compiledPatterns = (resources.patterns || []).map(function(pattern) {
    return { pattern: pattern, regex: new RegExp(pattern.source, pattern.flags || '') };
  });
  var activeMode = resources.defaultMode || 'en';

  function canonicalMode(value) {
    if (typeof value !== 'string') return null;
    var trimmed = value.trim();
    if (!trimmed) return null;
    if (modeById[trimmed]) return trimmed;
    return aliases[trimmed.toLowerCase()] || null;
  }

  function normalizeMode(value) {
    return canonicalMode(value) || resources.defaultMode || 'en';
  }

  function storageObject(candidate) {
    if (candidate) return candidate;
    try { return global.localStorage || null; } catch (_) { return null; }
  }

  function readWindowName() {
    try {
      var match = /(?:^|;)bambu-language=([^;]*)/.exec(String(global.name || ''));
      return match ? canonicalMode(decodeURIComponent(match[1])) : null;
    } catch (_) {
      return null;
    }
  }

  function writeWindowName(mode) {
    try {
      var current = String(global.name || '')
        .replace(/(?:^|;)bambu-language=[^;]*/g, '')
        .replace(/^;+|;+$/g, '');
      global.name = (current ? current + ';' : '') + 'bambu-language=' + encodeURIComponent(mode);
      return true;
    } catch (_) {
      return false;
    }
  }

  function readStoredMode(candidate) {
    var storage = storageObject(candidate);
    if (storage) {
      try {
        var stored = canonicalMode(storage.getItem(resources.storageKey));
        if (stored) return stored;
      } catch (_) {
        // file:// and hardened browsers can reject localStorage access.
      }
    }
    return readWindowName();
  }

  function saveStoredMode(value, candidate) {
    var mode = normalizeMode(value);
    var storage = storageObject(candidate);
    if (storage) {
      try {
        storage.setItem(resources.storageKey, mode);
        return true;
      } catch (_) {
        // Fall through to window.name, which survives a same-tab reload.
      }
    }
    return writeWindowName(mode);
  }

  function searchOverride(search) {
    if (typeof search !== 'string' || !search) return null;
    try {
      var raw = new URLSearchParams(search.charAt(0) === '?' ? search.slice(1) : search).get('lang');
      return raw === null ? null : (canonicalMode(raw) || raw);
    } catch (_) {
      return null;
    }
  }

  function resolveInitialMode(override, candidate) {
    // A supplied URL override is authoritative. Invalid values deliberately
    // fall back to English so QA never inherits a misleading stored mode.
    if (override !== undefined && override !== null && String(override).trim() !== '') {
      return canonicalMode(String(override)) || resources.defaultMode || 'en';
    }
    return readStoredMode(candidate) || resources.defaultMode || 'en';
  }

  function splitWhitespace(input) {
    var value = String(input == null ? '' : input);
    var match = /^(\s*)([\s\S]*?)(\s*)$/.exec(value);
    return { before: match[1], core: match[2], after: match[3] };
  }

  function normalizedEntry(rawEntry) {
    if (typeof rawEntry === 'string') return { text: rawEntry };
    if (rawEntry && typeof rawEntry.text === 'string') return rawEntry;
    return null;
  }

  function findYueEntry(english) {
    var catalog = resources.catalog && resources.catalog.yue_HK;
    // HTML templates often wrap prose across source lines. Browsers collapse
    // that whitespace visually, so use the same collapsed form for lookup.
    var lookup = String(english).replace(/\s+/g, ' ').trim();
    var direct = catalog && normalizedEntry(catalog[english] || catalog[lookup]);
    if (direct) return direct;

    for (var index = 0; index < compiledPatterns.length; index += 1) {
      var compiled = compiledPatterns[index];
      compiled.regex.lastIndex = 0;
      if (!compiled.regex.test(lookup)) continue;
      compiled.regex.lastIndex = 0;
      return {
        text: lookup.replace(compiled.regex, compiled.pattern.yue),
        display: compiled.pattern.display,
        tone: compiled.pattern.tone
      };
    }
    return null;
  }

  function describe(input, requestedMode) {
    var mode = canonicalMode(requestedMode) || activeMode || resources.defaultMode || 'en';
    var parts = splitWhitespace(input);
    if (!parts.core || mode === 'en') {
      return {
        mode: mode, text: String(input == null ? '' : input), english: parts.core,
        cantonese: '', localized: false, secondary: '', disclosure: false
      };
    }

    var entry = findYueEntry(parts.core);
    if (!entry) {
      return {
        mode: mode, text: String(input == null ? '' : input), english: parts.core,
        cantonese: '', localized: false, fallback: true,
        secondary: '', disclosure: false
      };
    }

    if (mode === 'yue_HK') {
      return {
        mode: mode, text: parts.before + entry.text + parts.after,
        english: parts.core, cantonese: entry.text, localized: true,
        secondary: '', disclosure: false, tone: entry.tone || ''
      };
    }

    var inline = entry.display === 'inline' || (
      entry.display !== 'detail' && parts.core.length <= 24 && entry.text.length <= 18
    );
    return {
      mode: mode, text: String(input == null ? '' : input),
      english: parts.core, cantonese: entry.text, localized: true,
      secondary: entry.text, disclosure: !inline, tone: entry.tone || ''
    };
  }

  function translateText(input, requestedMode) {
    return describe(input, requestedMode).text;
  }

  function translateAttribute(input, attributeName, requestedMode) {
    var info = describe(input, requestedMode);
    if (info.mode !== 'bilingual_en_yue_HK' || !info.secondary) return info.text;
    var name = String(attributeName || '').toLowerCase();
    if (name === 'title' || name === 'aria-label' || !info.disclosure) {
      return info.text + ' / ' + info.secondary;
    }
    // Long placeholders remain English-first to protect usable input width.
    return info.text;
  }

  function interpolate(template, params) {
    return String(template == null ? '' : template).replace(/\{([A-Za-z0-9_]+)\}/g, function(_, key) {
      return params && params[key] !== undefined ? String(params[key]) : '';
    });
  }

  function message(key, params, requestedMode) {
    var mode = canonicalMode(requestedMode) || activeMode || resources.defaultMode || 'en';
    var entry = resources.messages && resources.messages[key];
    if (!entry) return String(key);
    var english = interpolate(entry.en, params);
    var cantonese = interpolate(entry.yue || entry.en, params);
    if (mode === 'yue_HK') return cantonese;
    if (mode === 'bilingual_en_yue_HK' && cantonese !== english) return english + ' / ' + cantonese;
    return english;
  }

  function applyDocumentMode(mode) {
    if (!global.document || !global.document.documentElement) return;
    var root = global.document.documentElement;
    var definition = modeById[mode] || modeById.en || { htmlLang: 'en' };
    root.lang = definition.htmlLang || 'en';
    root.setAttribute('data-language-mode', mode);
    if (root.classList) root.classList.toggle('i18n-bilingual', mode === 'bilingual_en_yue_HK');
  }

  function setActiveMode(value, options) {
    var mode = normalizeMode(value);
    activeMode = mode;
    applyDocumentMode(mode);
    if (options && options.persist) saveStoredMode(mode, options.storage);
    return mode;
  }

  function initialize(options) {
    options = options || {};
    var override = options.override;
    if (override === undefined && options.search !== undefined) override = searchOverride(options.search);
    var mode = resolveInitialMode(override, options.storage);
    return setActiveMode(mode, { persist: false });
  }

  var api = {
    modes: resources.modes,
    storageKey: resources.storageKey,
    canonicalMode: canonicalMode,
    normalizeMode: normalizeMode,
    resolveInitialMode: resolveInitialMode,
    searchOverride: searchOverride,
    readStoredMode: readStoredMode,
    saveStoredMode: saveStoredMode,
    setActiveMode: setActiveMode,
    getActiveMode: function() { return activeMode; },
    initialize: initialize,
    describe: describe,
    translateText: translateText,
    translateAttribute: translateAttribute,
    message: message
  };

  global.BambuI18n = Object.freeze(api);
})(typeof window !== 'undefined' ? window : globalThis);
