/* Browser speech synthesis. User opt-in, local voice discovery, one queue. */
(function (global) {
  'use strict';
  var site = global.BambuSite;
  var synthesis = global.speechSynthesis;
  var queue = [];
  var speaking = false;
  var voiceListeners = [];
  var generation = 0;
  var lastCategory = Object.create(null);
  var MAX_TEXT = 2000;
  var MAX_QUEUE = 12;
  var COOLDOWN = 15000;

  function available(language) {
    if (!synthesis || !synthesis.getVoices) return [];
    var voices;
    try { voices = synthesis.getVoices(); } catch (error) { return []; }
    return voices.filter(function (voice) {
      return language === 'en' ? /^en(?:-|$)/i.test(voice.lang) : /^(?:yue(?:-|$)|zh-HK$)/i.test(voice.lang);
    });
  }
  function selection(language) {
    var voices = available(language);
    var selected = site.get(language === 'en' ? 'narratorVoiceEn' : 'narratorVoiceYue');
    var match = voices.filter(function (voice) { return voice.voiceURI === selected; })[0];
    return { voice: match || voices.filter(function (voice) { return voice.localService; })[0] || voices[0] || null, missing: !!selected && !match, chosen: selected || '', supported: !!synthesis };
  }
  function bounded(key, minimum, maximum, fallback) {
    var value = Number(site.get(key));
    return Number.isFinite(value) ? Math.max(minimum, Math.min(maximum, value)) : fallback;
  }
  function stop() {
    generation++;
    queue = [];
    speaking = false;
    if (synthesis && synthesis.cancel) { try { synthesis.cancel(); } catch (error) { /* platform unavailable */ } }
  }
  function pump() {
    if (speaking || !queue.length) return;
    if (!site.get('narratorEnabled') || site.get('narratorPaused') || !synthesis || !global.SpeechSynthesisUtterance) { stop(); return; }
    var item = queue.shift();
    var current = generation;
    var selected = selection(item.language);
    if (!selected.voice) { pump(); return; }
    speaking = true;
    var utterance = new global.SpeechSynthesisUtterance(item.text);
    utterance.voice = selected.voice;
    utterance.lang = selected.voice.lang;
    utterance.rate = bounded('narratorRate', 0.1, 10, 1);
    utterance.pitch = bounded('narratorPitch', 0, 2, 1);
    function finish() {
      if (current !== generation) return;
      speaking = false;
      pump();
    }
    utterance.onend = finish;
    utterance.onerror = finish;
    try { synthesis.speak(utterance); } catch (error) { finish(); }
  }
  function narrate(kind, key, params) {
    if (!site.get('narratorEnabled') || site.get('narratorPaused') || !synthesis) return false;
    var urgent = kind === 'warning' || kind === 'error';
    var now = Date.now();
    if (!urgent && lastCategory[kind] && now - lastCategory[kind] < COOLDOWN && !queue.some(function (item) { return item.kind === kind; })) return false;
    lastCategory[kind] = now;
    var mode = site.get('narratorLanguage');
    var languages = mode === 'both' ? ['en', 'yue'] : [mode === 'yue' ? 'yue' : 'en'];
    var pair = site.pair(key, params);
    var next = languages.filter(function (language) { return !!pair[language] && String(pair[language]).length <= MAX_TEXT && !!selection(language).voice; }).map(function (language) {
      return { language: language, text: String(pair[language]), kind: kind };
    });
    queue = queue.filter(function (item) { return item.kind !== kind || urgent; });
    // If the queue is full, urgent events displace non-urgent queued copy.
    while (queue.length + next.length > MAX_QUEUE) {
      var dispensable = queue.findIndex(function (item) { return item.kind !== 'warning' && item.kind !== 'error'; });
      if (dispensable < 0) { queue.shift(); } else queue.splice(dispensable, 1);
    }
    queue = queue.concat(next);
    pump();
    return next.length > 0;
  }
  function changedVoices() { voiceListeners.slice().forEach(function (listener) { listener(); }); }
  if (synthesis && synthesis.addEventListener) synthesis.addEventListener('voiceschanged', changedVoices);
  site.subscribe(function (keys) {
    if (keys.indexOf('narratorEnabled') !== -1 || keys.indexOf('narratorPaused') !== -1) stop();
  });

  function mount(host) {
    var doc = global.document;
    function row(key, explanation) {
      var shell = doc.createElement('div');
      shell.className = 'setting';
      shell.innerHTML = '<div class="setting-copy"><p class="setting-label" data-copy="' + key + '"></p><p class="setting-desc" data-copy="' + explanation + '"></p></div><div class="setting-control"></div>';
      host.appendChild(shell);
      return shell.querySelector('.setting-control');
    }
    function toggle(key, preference, explanation) {
      var control = row(key, explanation);
      var button = doc.createElement('button');
      button.type = 'button'; button.className = 'switch'; button.setAttribute('role', 'switch');
      button.setAttribute('data-copy-attr', 'aria-label:' + key);
      function paint() { button.setAttribute('aria-checked', String(!!site.get(preference))); button.classList.toggle('on', !!site.get(preference)); }
      button.addEventListener('click', function () { site.set(preference, !site.get(preference)); paint(); });
      control.appendChild(button); paint();
    }
    toggle('narrator.enabled', 'narratorEnabled', 'narrator.enabled.desc');
    toggle('narrator.pause', 'narratorPaused', 'narrator.pause.desc');
    function picker(key, preference, options, explanation) {
      var control = row(key, explanation);
      var select = doc.createElement('select'); select.className = 'select';
      select.setAttribute('data-copy-attr', 'aria-label:' + key);
      options.forEach(function (option) {
        var item = doc.createElement('option'); item.value = option.value;
        if (option.key) item.setAttribute('data-copy', option.key); else item.textContent = option.label;
        select.appendChild(item);
      });
      select.value = site.get(preference) || '';
      select.addEventListener('change', function () { site.set(preference, select.value); });
      var search = global.BambuRegex.createSearchField({ labelKey: key, items: function () { return [].slice.call(select.options).map(function (option) { return { id: option.value, text: option.textContent }; }); }, sampleProvider: function () { return [].slice.call(select.options).map(function (option) { return option.textContent; }).join('\n'); }, onResults: function (ids) { [].slice.call(select.options).forEach(function (option) { option.hidden = ids !== null && ids.indexOf(option.value) === -1 && !option.selected; }); } });
      control.appendChild(search.element); control.appendChild(select);
      return { select: select, control: control };
    }
    picker('narrator.language', 'narratorLanguage', [{ value: 'en', key: 'narrator.english' }, { value: 'yue', key: 'narrator.cantonese' }, { value: 'both', key: 'narrator.both' }], 'narrator.language.desc');
    ['en', 'yue'].forEach(function (language) {
      var preference = language === 'en' ? 'narratorVoiceEn' : 'narratorVoiceYue';
      var key = language === 'en' ? 'narrator.voice.en' : 'narrator.voice.yue';
      var mounted = picker(key, preference, [{ value: '', key: 'narrator.auto' }], 'narrator.voice.desc');
      var status = doc.createElement('p'); status.className = 'hint'; status.setAttribute('role', 'status'); mounted.control.appendChild(status);
      function paint() {
        while (mounted.select.options.length > 1) mounted.select.remove(1);
        available(language).forEach(function (voice) { var option = doc.createElement('option'); option.value = voice.voiceURI; option.textContent = voice.name + ' (' + voice.lang + ')'; mounted.select.appendChild(option); });
        var selected = selection(language);
        if (selected.missing) { var missing = doc.createElement('option'); missing.value = selected.chosen; missing.textContent = site.text('narrator.notInstalled'); mounted.select.appendChild(missing); }
        mounted.select.value = selected.chosen;
        site.setCopy(status, !selected.supported ? 'narrator.unavailable' : !selected.voice ? 'narrator.noVoice' : selected.missing ? (selected.voice.localService ? 'narrator.fallback' : 'narrator.fallbackNetwork') : selected.voice.localService ? 'narrator.activeLocal' : 'narrator.activeNetwork', { name: selected.voice ? selected.voice.name : '' });
      }
      mounted.select.addEventListener('change', paint); voiceListeners.push(paint); paint();
    });
    [{ key: 'narrator.rate', preference: 'narratorRate', min: 0.1, max: 10 }, { key: 'narrator.pitch', preference: 'narratorPitch', min: 0, max: 2 }].forEach(function (spec) {
      var control = row(spec.key, spec.key + '.desc');
      var slider = doc.createElement('input'); slider.type = 'range'; slider.min = spec.min; slider.max = spec.max; slider.step = '0.1'; slider.value = site.get(spec.preference); slider.setAttribute('data-copy-attr', 'aria-label:' + spec.key);
      var value = doc.createElement('output'); value.textContent = slider.value;
      slider.addEventListener('input', function () { site.set(spec.preference, Number(slider.value)); value.textContent = slider.value; });
      control.appendChild(slider); control.appendChild(value);
    });
    var control = row('narrator.preview', 'narrator.preview.desc');
    var preview = doc.createElement('button'); preview.type = 'button'; preview.className = 'btn btn-outline'; preview.setAttribute('data-copy', 'narrator.preview');
    preview.addEventListener('click', function () { narrate('preview', 'narrator.preview.line'); });
    control.appendChild(preview);
    var cancel = doc.createElement('button'); cancel.type = 'button'; cancel.className = 'btn btn-outline'; cancel.setAttribute('data-copy', 'narrator.stop'); cancel.addEventListener('click', stop); control.appendChild(cancel);
    function previewAvailability() {
      var language = site.get('narratorLanguage');
      var voiceAvailable = language === 'both' ? selection('en').voice || selection('yue').voice : selection(language === 'yue' ? 'yue' : 'en').voice;
      preview.disabled = !site.get('narratorEnabled') || site.get('narratorPaused') || !voiceAvailable;
      preview.title = preview.disabled ? site.text(!synthesis ? 'narrator.unavailable' : !voiceAvailable ? 'narrator.noVoice' : 'narrator.preview.desc') : '';
    }
    site.subscribe(previewAvailability); voiceListeners.push(previewAvailability); previewAvailability();
    site.applyCopy(host);
  }
  global.addEventListener && global.addEventListener('pagehide', function () { stop(); voiceListeners = []; if (synthesis && synthesis.removeEventListener) synthesis.removeEventListener('voiceschanged', changedVoices); });
  global.BambuNarration = { available: available, selection: selection, narrate: narrate, stop: stop, mount: mount, queueLength: function () { return queue.length; } };
})(typeof window !== 'undefined' ? window : globalThis);
