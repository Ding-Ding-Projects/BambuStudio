/* Local-only wording import. No mappings or source-file metadata ship here. */
(function (global) {
  'use strict';
  var LIMITS = Object.freeze({ bytes: 262144, depth: 3, entries: 1024, key: 160, value: 320 });
  var CACHE_KEY = 'bambuStudio.site.wording.v1';
  var active = null;
  var cached = false;

  // Parse before JSON.parse can discard duplicate object keys. Diagnostics are
  // deliberately generic: a rejected file must not leak any part of its data.
  function parse(raw) {
    if (typeof raw !== 'string' || new TextEncoder().encode(raw).length > LIMITS.bytes) throw new Error('size');
    var offset = 0;
    function space() { while (/\s/.test(raw.charAt(offset)) && offset < raw.length) offset++; }
    function string() {
      var start = offset++;
      while (offset < raw.length) {
        var character = raw.charAt(offset++);
        if (character === '\\') { offset++; continue; }
        if (character === '"') return JSON.parse(raw.slice(start, offset));
      }
      throw new Error('syntax');
    }
    function value(depth) {
      if (depth > LIMITS.depth) throw new Error('depth');
      space();
      if (raw.charAt(offset) === '"') return string();
      if (raw.charAt(offset) === '{') {
        offset++;
        var object = Object.create(null);
        space();
        if (raw.charAt(offset) === '}') { offset++; return object; }
        while (offset < raw.length) {
          space();
          if (raw.charAt(offset) !== '"') throw new Error('syntax');
          var key = string();
          if (Object.prototype.hasOwnProperty.call(object, key) || ['__proto__', 'prototype', 'constructor'].indexOf(key) !== -1) throw new Error('key');
          space();
          if (raw.charAt(offset++) !== ':') throw new Error('syntax');
          object[key] = value(depth + 1);
          space();
          var delimiter = raw.charAt(offset++);
          if (delimiter === '}') return object;
          if (delimiter !== ',') throw new Error('syntax');
        }
        throw new Error('syntax');
      }
      var match = /^(?:true|false|null|-?(?:0|[1-9]\d*)(?:\.\d+)?(?:[eE][+-]?\d+)?)/.exec(raw.slice(offset));
      if (!match) throw new Error('syntax');
      offset += match[0].length;
      return JSON.parse(match[0]);
    }
    var result = value(0);
    space();
    if (offset !== raw.length) throw new Error('syntax');
    if (!result || Object.keys(result).sort().join(',') !== 'entries,schemaVersion' || result.schemaVersion !== 1) throw new Error('schema');
    if (!result.entries || typeof result.entries !== 'object' || Array.isArray(result.entries)) throw new Error('schema');
    var keys = Object.keys(result.entries);
    if (!keys.length || keys.length > LIMITS.entries) throw new Error('entries');
    keys.forEach(function (key) {
      var replacement = result.entries[key];
      if (!key.length || key.length > LIMITS.key || typeof replacement !== 'string' || !replacement.length || replacement.length > LIMITS.value || /[\u0000-\u001f\u007f]/.test(key + replacement)) throw new Error('entry');
    });
    return result;
  }

  function restore() {
    active = null;
    cached = false;
    try {
      var raw = global.localStorage.getItem(CACHE_KEY);
      if (raw) { active = parse(raw); cached = true; }
    } catch (error) {
      try { global.localStorage.removeItem(CACHE_KEY); } catch (ignored) { /* storage unavailable */ }
    }
  }
  function changed() {
    if (global.BambuSite) { global.BambuSite.applyCopy(global.document.body); global.BambuSite.emit(['personalWording']); }
  }
  function load(raw) {
    var next = parse(raw);
    // Persist only the validated cache; no filename, path or rejected payload.
    cached = false;
    try { global.localStorage.setItem(CACHE_KEY, JSON.stringify(next)); cached = true; } catch (error) { /* session-only */ }
    active = next;
    changed();
    return { loaded: true, persistent: cached };
  }
  function clear() {
    // A failed removal must not claim reset while leaving a restorable cache.
    global.localStorage.removeItem(CACHE_KEY);
    active = null;
    cached = false;
    changed();
  }
  function replace(text) {
    if (!active) return text;
    var original = String(text);
    var protectedSpans = [];
    var protectedSyntax = /\{[A-Za-z0-9_]+\}|https?:\/\/[^\s<>]+|`[^`]*`|(?:[A-Za-z]:[\\/]|\.\.?\/)[^\s<>]+/g;
    var protectedMatch;
    while ((protectedMatch = protectedSyntax.exec(original))) protectedSpans.push([protectedMatch.index, protectedMatch.index + protectedMatch[0].length]);
    // Longest-first, one pass: a replacement is never fed into another mapping.
    var keys = Object.keys(active.entries).sort(function (a, b) { return b.length - a.length; });
    var expression = new RegExp(keys.map(function (key) { return key.replace(/[\\^$.*+?()[\]{}|]/g, '\\$&'); }).join('|'), 'gu');
    return original.replace(expression, function (match, offset, source) {
      if (protectedSpans.some(function (span) { return offset < span[1] && offset + match.length > span[0]; })) return match;
      var before = source.charAt(offset - 1);
      var after = source.charAt(offset + match.length);
      if ((/[\p{L}\p{N}_]/u.test(match.charAt(0)) && /[\p{L}\p{N}_]/u.test(before)) || (/[\p{L}\p{N}_]/u.test(match.charAt(match.length - 1)) && /[\p{L}\p{N}_]/u.test(after))) return match;
      return active.entries[match];
    });
  }

  function mount(host) {
    var site = global.BambuSite;
    var doc = global.document;
    host.innerHTML = '<div class="setting"><div class="setting-copy"><p class="setting-label" data-copy="wording.upload"></p><p class="setting-desc" data-copy="wording.explanation"></p><p class="hint" role="status" aria-live="polite"></p></div><div class="setting-control"><input type="file" accept="application/json,.json" data-copy-attr="aria-label:wording.upload"><button type="button" class="btn btn-outline" data-copy="wording.clear"></button></div></div>';
    var picker = host.querySelector('input');
    var clearButton = host.querySelector('button');
    var status = host.querySelector('[role="status"]');
    function paint(key) {
      site.setCopy(status, key || (!active ? 'wording.empty' : cached ? 'wording.loaded' : 'wording.session'));
      clearButton.disabled = !active;
      picker.setAttribute('data-copy-attr', 'aria-label:' + (active ? 'wording.replace' : 'wording.upload'));
      site.applyCopy(host);
    }
    var generation = 0;
    picker.addEventListener('change', async function () {
      var file = picker.files && picker.files[0];
      var current = ++generation;
      picker.value = '';
      if (!file) return;
      try {
        if (file.size > LIMITS.bytes) throw new Error('size');
        var raw = await file.text();
        if (current !== generation) return;
        load(raw);
        paint();
      } catch (error) { if (current === generation) paint('wording.invalid'); }
    });
    clearButton.addEventListener('click', function () {
      generation++;
      try { clear(); paint(); } catch (error) { paint('wording.clearFailed'); }
    });
    paint();
  }
  restore();
  global.BambuWording = { LIMITS: LIMITS, parse: parse, load: load, clear: clear, restore: restore, replace: replace, mount: mount, status: function () { return { loaded: !!active, persistent: cached }; } };
})(typeof window !== 'undefined' ? window : globalThis);
