/*
 * The Tools tab and the site-wide helpers it explains: the command palette
 * (Ctrl+Shift+F), the Base64 toolkit, the accessibility tools and the
 * Animation and transition settings.
 *
 * The palette lists tabs, settings (as live switches, right in the results),
 * every documentation article once the bundle is loaded, the feature cards and
 * the sections of the current page; choosing one teleports to the exact
 * element and focuses it. Everything the toolkit does stays in this browser.
 */
(function (global) {
  'use strict';

  var site = global.BambuSite;
  var doc = global.document;

  /* ------------------------------------------------------------ palette */

  var palette = null;

  function paletteItems(query) {
    var items = [];
    var tabs = global.BambuSiteTabs ? global.BambuSiteTabs.ids() : [];
    tabs.forEach(function (id) {
      items.push({ kind: 'palette.kind.tab', label: site.pair('tab.' + id), run: function () { global.BambuSiteTabs.activate(id, { focus: true }); } });
    });
    var switches = [
      { key: 'reducedMotion', label: 'access.motion', on: 'on', off: 'system' },
      { key: 'highContrast', label: 'access.contrast', on: true, off: false },
      { key: 'focusAlways', label: 'access.focus', on: true, off: false },
      { key: 'largerTargets', label: 'access.targets', on: true, off: false },
      { key: 'motionHero', label: 'motion.hero', on: true, off: false },
      { key: 'motionHover', label: 'motion.hover', on: true, off: false },
      { key: 'theme', label: 'you.dark', on: 'dark', off: 'light' },
      { key: 'notifications', label: 'settings.notify.enabled', on: true, off: false }
    ];
    switches.forEach(function (s) {
      items.push({ kind: 'palette.kind.setting', label: site.pair(s.label), setting: s, run: function () {
        site.set(s.key, site.get(s.key) === s.on ? s.off : s.on);
        site.applyAppearance();
      } });
    });
    (global.BambuFeatures ? global.BambuFeatures.FEATURES : []).forEach(function (feature) {
      items.push({ kind: 'palette.kind.feature', label: site.pair('feature.' + feature.id), run: function () {
        global.BambuSiteTabs.activate('features');
        var card = doc.querySelector('[data-feature="' + feature.id + '"]');
        if (card) { card.scrollIntoView({ block: 'center' }); card.querySelector('a').focus(); }
      } });
    });
    (global.BambuDocs ? global.BambuDocs.articles() : []).forEach(function (article) {
      items.push({ kind: 'palette.kind.article', label: { en: article.title, yue: article.titleYue || article.title }, run: function () { global.BambuDocs.open(article.id); } });
    });
    doc.querySelectorAll('main .panel:not([hidden]) h2, main .panel:not([hidden]) h3').forEach(function (heading) {
      var text = heading.textContent.trim();
      if (!text) return;
      items.push({ kind: 'palette.kind.section', label: { en: text, yue: '' }, run: function () {
        heading.scrollIntoView({ block: 'start' });
        heading.setAttribute('tabindex', '-1');
        heading.focus();
        heading.classList.add('teleported');
        global.setTimeout(function () { heading.classList.remove('teleported'); }, 1600);
      } });
    });
    var matcher = global.BambuRegex.createMatcher(query, { regex: false });
    return items.filter(function (item) {
      return matcher.empty || matcher.test(item.label.en + ' ' + item.label.yue);
    }).slice(0, 40);
  }

  function ensurePalette() {
    if (palette) return palette;
    var host = doc.createElement('div');
    host.className = 'palette';
    host.hidden = true;
    host.innerHTML =
      '<div class="palette-scrim"></div>' +
      '<div class="palette-box" role="dialog" aria-modal="true" data-copy-attr="aria-label:palette.heading">' +
        '<div class="palette-row">' +
          '<span data-icon aria-hidden="true">search</span>' +
          '<input class="palette-input" type="text" autocomplete="off" spellcheck="false" data-copy-attr="placeholder:palette.placeholder;aria-label:palette.placeholder">' +
          '<kbd class="mono">Esc</kbd>' +
        '</div>' +
        '<div class="palette-list" role="listbox"></div>' +
      '</div>';
    doc.body.appendChild(host);
    var input = host.querySelector('.palette-input');
    var list = host.querySelector('.palette-list');
    var current = [];
    var selected = 0;

    function paint() {
      current = paletteItems(input.value);
      selected = Math.min(selected, Math.max(0, current.length - 1));
      list.innerHTML = current.length ? current.map(function (item, index) {
        var live = '';
        if (item.setting) {
          var on = site.get(item.setting.key) === item.setting.on;
          live = '<span class="palette-switch" aria-hidden="true" data-on="' + on + '"></span>';
        }
        return '<button type="button" class="palette-item" role="option" aria-selected="' + (index === selected) + '" data-index="' + index + '">' +
          '<span class="palette-kind" data-copy="' + item.kind + '"></span>' +
          '<span class="palette-label"><span>' + escapeHtml(item.label.en) + '</span>' +
            (item.label.yue && item.label.yue !== item.label.en ? '<span class="palette-yue" lang="yue-Hant-HK">' + escapeHtml(item.label.yue) + '</span>' : '') + '</span>' +
          live + '</button>';
      }).join('') : '<p class="empty" data-copy="palette.empty"></p>';
      site.applyCopy(list);
    }

    function run(index) {
      var item = current[index];
      if (!item) return;
      if (item.setting) { item.run(); paint(); return; }
      close();
      item.run();
    }

    function close() {
      host.hidden = true;
      doc.body.classList.remove('palette-open');
    }

    input.addEventListener('input', function () { selected = 0; paint(); });
    input.addEventListener('keydown', function (event) {
      if (event.key === 'ArrowDown') { event.preventDefault(); selected = Math.min(current.length - 1, selected + 1); paint(); }
      else if (event.key === 'ArrowUp') { event.preventDefault(); selected = Math.max(0, selected - 1); paint(); }
      else if (event.key === 'Enter') { event.preventDefault(); run(selected); }
      else if (event.key === 'Escape') { event.preventDefault(); close(); }
    });
    list.addEventListener('click', function (event) {
      var button = event.target.closest('.palette-item');
      if (button) run(Number(button.dataset.index));
    });
    host.querySelector('.palette-scrim').addEventListener('click', close);
    host.addEventListener('keydown', function (event) { if (event.key === 'Escape') close(); });

    palette = {
      open: function () {
        host.hidden = false;
        doc.body.classList.add('palette-open');
        site.applyCopy(host);
        input.value = '';
        selected = 0;
        paint();
        if (global.BambuDocs) global.BambuDocs.load().then(paint);
        input.focus();
      },
      close: close,
      isOpen: function () { return !host.hidden; }
    };
    return palette;
  }

  function openPalette() { ensurePalette().open(); }

  doc.addEventListener('keydown', function (event) {
    if (event.ctrlKey && event.shiftKey && (event.key === 'F' || event.key === 'f')) {
      event.preventDefault();
      if (palette && palette.isOpen()) palette.close(); else openPalette();
    }
  });

  /* ------------------------------------------------------------- base64 */

  function utf8ToBase64(text) {
    var bytes = new global.TextEncoder().encode(text);
    var binary = '';
    for (var i = 0; i < bytes.length; i++) binary += String.fromCharCode(bytes[i]);
    return global.btoa(binary);
  }

  function base64ToUtf8(text) {
    var clean = text.replace(/-/g, '+').replace(/_/g, '/').replace(/\s+/g, '');
    while (clean.length % 4) clean += '=';
    var binary = global.atob(clean);
    var bytes = new Uint8Array(binary.length);
    for (var i = 0; i < binary.length; i++) bytes[i] = binary.charCodeAt(i);
    return new global.TextDecoder('utf-8', { fatal: true }).decode(bytes);
  }

  function urlSafe(text) { return text.replace(/\+/g, '-').replace(/\//g, '_').replace(/=+$/, ''); }

  function formatBytes(bytes) {
    if (bytes < 1024) return bytes + ' B';
    if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + ' KB';
    return (bytes / (1024 * 1024)).toFixed(1) + ' MB';
  }

  function escapeHtml(text) {
    return String(text).replace(/[&<>"']/g, function (c) {
      return { '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c];
    });
  }

  function base64Section() {
    return '<section class="tool-card reveal" id="base64">' +
      '<h2><span data-icon aria-hidden="true">code</span><span data-copy="base64.heading"></span>' +
        '<button type="button" class="infobtn" data-info="base64.body" data-copy-attr="aria-label:shell.info;title:shell.info"><span data-icon aria-hidden="true">info</span></button></h2>' +
      '<p data-copy="base64.body"></p>' +
      '<div class="b64-grid">' +
        '<label class="b64-field"><span data-copy="base64.text"></span><textarea class="b64-plain mono" rows="5" spellcheck="false"></textarea></label>' +
        '<label class="b64-field"><span data-copy="base64.encoded"></span><textarea class="b64-coded mono" rows="5" spellcheck="false"></textarea></label>' +
      '</div>' +
      '<div class="cta-row">' +
        '<button type="button" class="btn btn-filled b64-encode"><span data-icon aria-hidden="true">arrow_forward</span><span data-copy="base64.encode"></span></button>' +
        '<button type="button" class="btn btn-tonal b64-decode"><span data-icon aria-hidden="true">arrow_back</span><span data-copy="base64.decode"></span></button>' +
        '<label class="check b64-urlsafe"><input type="checkbox"><span data-copy="base64.urlsafe"></span></label>' +
        '<button type="button" class="btn btn-outline b64-copy"><span data-icon aria-hidden="true">content_copy</span><span data-copy="base64.copy"></span></button>' +
        '<label class="btn btn-outline b64-filelabel" for="b64File"><span data-icon aria-hidden="true">upload_file</span><span data-copy="base64.file"></span></label>' +
        '<input type="file" id="b64File" class="b64-file" hidden>' +
      '</div>' +
      '<p class="sf-status b64-status" role="status"></p>' +
      '<div class="b64-preview" hidden></div>' +
      '<h3><span data-copy="base64.history"></span></h3>' +
      '<div class="b64-history"></div>' +
      '<p><button type="button" class="btn btn-outline b64-clear"><span data-icon aria-hidden="true">delete</span><span data-copy="base64.history.clear"></span></button></p>' +
    '</section>';
  }

  function wireBase64(panel) {
    var plain = panel.querySelector('.b64-plain');
    var coded = panel.querySelector('.b64-coded');
    var status = panel.querySelector('.b64-status');
    var preview = panel.querySelector('.b64-preview');
    var history = panel.querySelector('.b64-history');
    var safe = panel.querySelector('.b64-urlsafe input');

    function remember(entry) {
      var list = (site.get('base64History') || []).slice(0, 9);
      list.unshift(entry);
      site.set('base64History', list);
      paintHistory();
    }
    function paintHistory() {
      var list = site.get('base64History') || [];
      history.innerHTML = list.length ? list.map(function (entry, index) {
        return '<button type="button" class="b64-entry" data-index="' + index + '">' +
          '<span class="mono b64-entry-kind">' + escapeHtml(entry.kind) + '</span>' +
          '<span class="b64-entry-text">' + escapeHtml(String(entry.output).slice(0, 80)) + (String(entry.output).length > 80 ? '…' : '') + '</span>' +
          '</button>';
      }).join('') : '<p class="empty" data-copy="base64.history.empty"></p>';
      site.applyCopy(history);
    }
    function say(key, params) {
      site.setCopy(status, key || '', params);
      status.className = 'sf-status b64-status' + (key ? ' status-invalid' : '');
    }

    panel.querySelector('.b64-encode').addEventListener('click', function () {
      var out = utf8ToBase64(plain.value);
      if (safe.checked) out = urlSafe(out);
      coded.value = out;
      say('');
      preview.hidden = true;
      if (plain.value) remember({ kind: 'encode', input: plain.value.slice(0, 200), output: out });
    });
    panel.querySelector('.b64-decode').addEventListener('click', function () {
      try {
        var text = base64ToUtf8(coded.value);
        plain.value = text;
        say('');
        preview.hidden = true;
        if (coded.value) remember({ kind: 'decode', input: coded.value.slice(0, 200), output: text });
      } catch (error) {
        say('base64.invalid');
      }
    });
    panel.querySelector('.b64-copy').addEventListener('click', function () {
      site.copyText(coded.value || plain.value).then(function () { site.notify('info', 'notify.base64.copied'); });
    });
    panel.querySelector('.b64-file').addEventListener('change', function (event) {
      var file = event.target.files && event.target.files[0];
      if (!file) return;
      if (file.size > 8 * 1024 * 1024) { say('base64.toolarge'); return; }
      var reader = new global.FileReader();
      reader.onload = function () {
        var dataUrl = String(reader.result);
        var out = dataUrl.slice(dataUrl.indexOf(',') + 1);
        coded.value = safe.checked ? urlSafe(out) : out;
        plain.value = '';
        say('');
        var fact = site.text('base64.filefact', { name: file.name, size: formatBytes(file.size), type: file.type || 'application/octet-stream' });
        var inner = '<p class="rowlbl"><span data-copy="base64.preview"></span></p><p class="mono">' + escapeHtml(fact) + '</p>';
        if (/^image\//.test(file.type)) inner += '<img class="b64-image" src="' + dataUrl + '" alt="">';
        else if (file.type === 'application/pdf') inner += '<object class="b64-pdf" data="' + dataUrl + '" type="application/pdf" aria-label="PDF preview"></object>';
        preview.innerHTML = inner;
        preview.hidden = false;
        site.applyCopy(preview);
        remember({ kind: 'file', input: file.name, output: coded.value });
      };
      reader.readAsDataURL(file);
    });
    history.addEventListener('click', function (event) {
      var entry = event.target.closest('.b64-entry');
      if (!entry) return;
      var item = (site.get('base64History') || [])[Number(entry.dataset.index)];
      if (!item) return;
      if (item.kind === 'decode') { coded.value = item.input; plain.value = item.output; }
      else { plain.value = item.kind === 'encode' ? item.input : ''; coded.value = item.output; }
      say('');
    });
    panel.querySelector('.b64-clear').addEventListener('click', function () { site.set('base64History', []); paintHistory(); });
    paintHistory();
  }

  /* --------------------------------------------------------- accessibility */

  function switchRow(key, labelKey, descKey) {
    return '<label class="setting access-row" data-search-id="' + key + '">' +
      '<span class="setting-text"><span class="setting-label" data-copy="' + labelKey + '"></span>' +
      (descKey ? '<span class="setting-desc" data-copy="' + descKey + '"></span>' : '') + '</span>' +
      '<input type="checkbox" class="switch" data-key="' + key + '"></label>';
  }

  function accessSection() {
    return '<section class="tool-card reveal" id="access">' +
      '<h2><span data-icon aria-hidden="true">accessibility_new</span><span data-copy="access.heading"></span>' +
        '<button type="button" class="infobtn" data-info="access.body" data-copy-attr="aria-label:shell.info;title:shell.info"><span data-icon aria-hidden="true">info</span></button></h2>' +
      '<p data-copy="access.body"></p>' +
      '<div class="setting"><span class="setting-text"><span class="setting-label" data-copy="access.textsize"></span></span>' +
        '<div class="seg" data-scale>' +
          [100, 125, 150, 200].map(function (n) { return '<button type="button" data-val="' + n + '">' + n + '%</button>'; }).join('') +
        '</div></div>' +
      '<div class="setting"><span class="setting-text"><span class="setting-label" data-copy="access.motion"></span></span>' +
        '<div class="seg" data-motion>' +
          '<button type="button" data-val="system" data-copy="access.motion.system"></button>' +
          '<button type="button" data-val="on" data-copy="access.motion.on"></button>' +
          '<button type="button" data-val="off" data-copy="access.motion.off"></button>' +
        '</div></div>' +
      switchRow('highContrast', 'access.contrast') +
      switchRow('focusAlways', 'access.focus') +
      switchRow('largerTargets', 'access.targets') +
      '<div class="setting"><span class="setting-text"><span class="setting-label" data-copy="access.readaloud"></span>' +
        '<span class="setting-desc" data-copy="access.readaloud.desc"></span></span>' +
        '<button type="button" class="btn btn-tonal access-aloud" aria-pressed="false"><span data-icon aria-hidden="true">record_voice_over</span><span data-copy="access.readaloud"></span></button></div>' +
    '</section>';
  }

  function motionSection() {
    return '<section class="tool-card reveal" id="motion">' +
      '<h2><span data-icon aria-hidden="true">animation</span><span data-copy="motion.heading"></span>' +
        '<button type="button" class="infobtn" data-info="motion.body" data-copy-attr="aria-label:shell.info;title:shell.info"><span data-icon aria-hidden="true">info</span></button></h2>' +
      '<p data-copy="motion.body"></p>' +
      '<label class="setting"><span class="setting-text"><span class="setting-label" data-copy="motion.reveal"></span>' +
        '<span class="setting-desc" data-copy="motion.reveal.desc"></span></span>' +
        '<span class="range-wrap"><input type="range" class="motion-reveal" min="0" max="800" step="20"><output class="mono motion-reveal-out"></output></span></label>' +
      '<div class="setting"><span class="setting-text"><span class="setting-label" data-copy="motion.tab"></span></span>' +
        '<div class="seg" data-tabmotion>' +
          '<button type="button" data-val="fade" data-copy="motion.tab.fade"></button>' +
          '<button type="button" data-val="slide" data-copy="motion.tab.slide"></button>' +
          '<button type="button" data-val="none" data-copy="motion.tab.none"></button>' +
        '</div></div>' +
      switchRow('motionHero', 'motion.hero') +
      switchRow('motionHover', 'motion.hover') +
      '<label class="setting"><span class="setting-text"><span class="setting-label" data-copy="motion.preview"></span></span>' +
        '<input type="checkbox" class="switch motion-preview"></label>' +
    '</section>';
  }

  function paletteSection() {
    return '<section class="tool-card reveal" id="palette">' +
      '<h2><span data-icon aria-hidden="true">keyboard_command_key</span><span data-copy="palette.heading"></span>' +
        '<button type="button" class="infobtn" data-info="palette.body" data-copy-attr="aria-label:shell.info;title:shell.info"><span data-icon aria-hidden="true">info</span></button></h2>' +
      '<p data-copy="palette.body"></p>' +
      '<p><button type="button" class="btn btn-filled palette-launch"><span data-icon aria-hidden="true">search</span><span data-copy="palette.open"></span> <kbd class="mono">Ctrl+Shift+F</kbd></button></p>' +
    '</section>';
  }

  function wireSettings(panel) {
    panel.querySelectorAll('input.switch[data-key]').forEach(function (input) {
      var key = input.dataset.key;
      input.checked = !!site.get(key);
      input.addEventListener('change', function () {
        site.set(key, input.checked);
        site.notify('info', /^motion/.test(key) ? 'notify.motion.saved' : 'notify.access.saved');
      });
      site.subscribe(function (keys) { if (keys.indexOf(key) !== -1) input.checked = !!site.get(key); });
    });
    function seg(selector, key, after) {
      var group = panel.querySelector(selector);
      if (!group) return;
      function paint() {
        group.querySelectorAll('button').forEach(function (button) {
          button.setAttribute('aria-pressed', String(String(site.get(key)) === button.dataset.val));
        });
      }
      group.addEventListener('click', function (event) {
        var button = event.target.closest('button');
        if (!button) return;
        site.set(key, after ? after(button.dataset.val) : button.dataset.val);
        site.applyAppearance();
        paint();
        site.notify('info', key === 'motionTab' ? 'notify.motion.saved' : 'notify.access.saved');
      });
      paint();
      site.subscribe(function (keys) { if (keys.indexOf(key) !== -1) paint(); });
    }
    seg('.seg[data-scale]', 'fontScale', Number);
    seg('.seg[data-motion]', 'reducedMotion');
    seg('.seg[data-tabmotion]', 'motionTab');

    var reveal = panel.querySelector('.motion-reveal');
    var out = panel.querySelector('.motion-reveal-out');
    reveal.value = site.get('motionReveal');
    out.textContent = reveal.value + ' ms';
    reveal.addEventListener('input', function () { out.textContent = reveal.value + ' ms'; });
    reveal.addEventListener('change', function () { site.set('motionReveal', Number(reveal.value)); site.notify('info', 'notify.motion.saved'); });

    var preview = panel.querySelector('.motion-preview');
    preview.checked = !!(global.BambuMotion && global.BambuMotion.previewing());
    preview.addEventListener('change', function () { if (global.BambuMotion) global.BambuMotion.setPreview(preview.checked); });

    var aloud = panel.querySelector('.access-aloud');
    var reading = false;
    aloud.addEventListener('click', function () {
      if (!global.speechSynthesis) return;
      if (reading) { global.speechSynthesis.cancel(); reading = false; aloud.setAttribute('aria-pressed', 'false'); return; }
      var active = doc.querySelector('main .panel:not([hidden])');
      var utterance = new global.SpeechSynthesisUtterance((active ? active.textContent : '').slice(0, 12000));
      utterance.lang = site.languageMode() === 'yue_HK' ? 'zh-HK' : 'en-US';
      utterance.rate = Number(site.get('narratorRate')) || 1;
      utterance.pitch = Number(site.get('narratorPitch')) || 1;
      utterance.onend = function () { reading = false; aloud.setAttribute('aria-pressed', 'false'); };
      reading = true;
      aloud.setAttribute('aria-pressed', 'true');
      global.speechSynthesis.cancel();
      global.speechSynthesis.speak(utterance);
    });

    panel.querySelector('.palette-launch').addEventListener('click', openPalette);
    panel.addEventListener('click', function (event) {
      var info = event.target.closest('.infobtn');
      if (info) site.notify('info', info.dataset.info);
    });
  }

  function renderTools(panel) {
    panel.innerHTML =
      '<div class="section-head"><h2 data-copy="tools.heading"></h2><p data-copy="tools.body"></p></div>' +
      '<div class="tool-grid">' + paletteSection() + base64Section() + accessSection() + motionSection() + '</div>';
    wireBase64(panel);
    wireSettings(panel);
    if (global.BambuMotion) global.BambuMotion.watch(panel);
  }

  global.BambuTools = {
    renderTools: renderTools,
    openPalette: openPalette,
    paletteItems: paletteItems,
    utf8ToBase64: utf8ToBase64,
    base64ToUtf8: base64ToUtf8,
    urlSafe: urlSafe
  };
})(typeof window !== 'undefined' ? window : globalThis);
