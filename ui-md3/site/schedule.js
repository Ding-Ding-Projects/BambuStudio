/* Temporary local scheduled overrides. User base preferences never move. */
(function (global) {
  'use strict';
  var site = global.BambuSite;
  var doc = global.document;
  var MAX_RULES = 128;
  var effective = Object.create(null);
  var matching = [];
  var timer = null;
  var lastInvalid = false;
  var FIELDS = {
    languageMode: { type: 'select', values: ['en', 'yue_HK', 'bilingual_en_yue_HK'], copy: 'settings.language.mode' },
    funnyEn: { type: 'number', min: 1, max: 5, copy: 'settings.funny.en' },
    funnyYue: { type: 'number', min: 1, max: 5, copy: 'settings.funny.yue' },
    theme: { type: 'select', values: ['light', 'dark'], copy: 'settings.theme' },
    density: { type: 'select', values: ['comfortable', 'compact'], copy: 'settings.density' },
    accent: { type: 'color', copy: 'settings.accent' },
    fontFamily: { type: 'select', values: Object.keys(site.fontStacks), copy: 'settings.font.family' },
    fontScale: { type: 'number', min: 75, max: 160, copy: 'settings.font.size' },
    fontWeight: { type: 'select', values: [400, 500, 700], copy: 'settings.font.weight' },
    elementStyles: { type: 'elementStyles', copy: 'settings.group.elements' },
    messageEmojis: { type: 'boolean', copy: 'settings.emoji' },
    attentionFocus: { type: 'boolean', copy: 'attention.focus' },
    attentionLow: { type: 'boolean', copy: 'attention.low' },
    attentionTime: { type: 'boolean', copy: 'attention.time' },
    attentionOne: { type: 'boolean', copy: 'attention.one' },
    attentionMomentum: { type: 'boolean', copy: 'attention.momentum' },
    narratorEnabled: { type: 'boolean', copy: 'narrator.enabled' },
    narratorPaused: { type: 'boolean', copy: 'narrator.pause' },
    narratorLanguage: { type: 'select', values: ['en', 'yue', 'both'], copy: 'narrator.language' },
    narratorRate: { type: 'number', min: 0.1, max: 10, step: 0.1, copy: 'narrator.rate' },
    narratorPitch: { type: 'number', min: 0, max: 2, step: 0.1, copy: 'narrator.pitch' }
  };
  function localZone() { try { return Intl.DateTimeFormat().resolvedOptions().timeZone || 'UTC'; } catch (error) { return 'UTC'; } }
  function defaults() { return { version: 1, timeZone: localZone(), rules: [] }; }
  function exact(object, keys) {
    if (!object || typeof object !== 'object' || Array.isArray(object) || Object.keys(object).sort().join(',') !== keys.slice().sort().join(',')) throw new Error('schema');
  }
  function validDate(value) {
    if (value === '') return true;
    if (typeof value !== 'string' || !/^\d{4}-\d{2}-\d{2}$/.test(value)) return false;
    var date = new Date(value + 'T12:00:00Z');
    return !isNaN(date.getTime()) && date.toISOString().slice(0, 10) === value;
  }
  function minute(value) {
    if (typeof value !== 'string' || !/^(?:[01]\d|2[0-3]):[0-5]\d$/.test(value)) return null;
    return Number(value.slice(0, 2)) * 60 + Number(value.slice(3));
  }
  function validateValue(key, value) {
    var field = FIELDS[key];
    if (!field) throw new Error('field');
    if (field.type === 'elementStyles') {
      if (!value || typeof value !== 'object' || Array.isArray(value) || Object.keys(value).length > 256) throw new Error('elementStyles');
      var targets = global.BambuControls && global.BambuControls.appearanceTargets ? global.BambuControls.appearanceTargets() : [];
      Object.keys(value).forEach(function (id) {
        var target = targets.filter(function (entry) { return entry.id === id; })[0];
        if (!target || !value[id] || typeof value[id] !== 'object' || Array.isArray(value[id])) throw new Error('elementTarget');
        Object.keys(value[id]).forEach(function (property) {
          var input = value[id][property];
          if (target.properties.indexOf(property) === -1) throw new Error('elementProperty');
          if (property === 'color' && typeof input === 'string' && /^#[0-9a-f]{6}$/i.test(input)) return;
          if (property === 'font' && Object.keys(site.fontStacks).some(function (font) { return site.fontStacks[font] === input; })) return;
          if (property === 'weight' && ['400', '500', '700'].indexOf(String(input)) !== -1) return;
          if (property === 'size' && /^(?:0\.\d+|1(?:\.\d+)?)$/.test(String(input)) && Number(input) >= 0.8 && Number(input) <= 1.4) return;
          if ((property === 'radius' || property === 'spacing') && typeof input === 'string' && /^\d{1,2}px$/.test(input) && parseInt(input, 10) <= 32) return;
          throw new Error('elementValue');
        });
      });
      return;
    }
    if (field.type === 'boolean' && typeof value === 'boolean') return;
    if (field.type === 'color' && typeof value === 'string' && /^#[0-9a-f]{6}$/i.test(value)) return;
    if (field.type === 'select' && field.values.indexOf(value) !== -1) return;
    if (field.type === 'number' && typeof value === 'number' && Number.isFinite(value) && value >= field.min && value <= field.max && (field.step || Number.isInteger(value))) return;
    throw new Error('value');
  }
  function validate(state) {
    exact(state, ['version', 'timeZone', 'rules']);
    if (state.version !== 1 || typeof state.timeZone !== 'string' || state.timeZone.length > 100 || !Array.isArray(state.rules) || state.rules.length > MAX_RULES) throw new Error('schema');
    try { new Intl.DateTimeFormat('en', { timeZone: state.timeZone }).format(new Date()); } catch (error) { throw new Error('timezone'); }
    var ids = [];
    state.rules.forEach(function (rule) {
      exact(rule, ['id', 'label', 'enabled', 'priority', 'startDate', 'endDate', 'startTime', 'endTime', 'allDay', 'everyDay', 'weekdays', 'source', 'values']);
      if (typeof rule.id !== 'string' || !/^[A-Za-z0-9_-]{1,80}$/.test(rule.id) || ids.indexOf(rule.id) !== -1) throw new Error('id');
      ids.push(rule.id);
      if (typeof rule.label !== 'string' || !rule.label.trim() || new TextEncoder().encode(rule.label).length > 128 || /[\u0000-\u001f\u007f]/.test(rule.label)) throw new Error('label');
      if (typeof rule.enabled !== 'boolean' || typeof rule.allDay !== 'boolean' || typeof rule.everyDay !== 'boolean' || !Number.isInteger(rule.priority) || rule.priority < 0 || rule.priority > 999) throw new Error('rule');
      if (!validDate(rule.startDate) || !validDate(rule.endDate) || rule.startDate && rule.endDate && rule.startDate > rule.endDate) throw new Error('date');
      if (rule.allDay ? rule.startTime !== '' || rule.endTime !== '' : minute(rule.startTime) === null || minute(rule.endTime) === null || rule.startTime === rule.endTime) throw new Error('time');
      if (!Array.isArray(rule.weekdays) || rule.weekdays.some(function (day) { return !Number.isInteger(day) || day < 0 || day > 6; }) || new Set(rule.weekdays).size !== rule.weekdays.length || (rule.everyDay ? rule.weekdays.length !== 0 : rule.weekdays.length === 0)) throw new Error('weekdays');
      // An unpaired browser has no privileged network/settings-source boundary.
      if (rule.source !== 'local') throw new Error('source-unavailable');
      if (!rule.values || typeof rule.values !== 'object' || Array.isArray(rule.values) || !Object.keys(rule.values).length || Object.keys(rule.values).length > Object.keys(FIELDS).length) throw new Error('values');
      Object.keys(rule.values).forEach(function (key) { validateValue(key, rule.values[key]); });
    });
    return state;
  }
  function parse(raw) { return validate(global.BambuWording.parseJson(raw, 262144, 6)); }
  function wallTime(date, zone) {
    var parts = new Intl.DateTimeFormat('en-CA', { timeZone: zone, year: 'numeric', month: '2-digit', day: '2-digit', hour: '2-digit', minute: '2-digit', hourCycle: 'h23' }).formatToParts(date);
    var fields = {}; parts.forEach(function (part) { fields[part.type] = part.value; });
    var day = fields.year + '-' + fields.month + '-' + fields.day;
    return { day: day, weekday: new Date(day + 'T12:00:00Z').getUTCDay(), minute: Number(fields.hour) * 60 + Number(fields.minute) };
  }
  function evaluate(state, date) {
    validate(state);
    if (!(date instanceof Date) || !Number.isFinite(date.getTime())) throw new Error('clock');
    var wall = wallTime(date, state.timeZone);
    var matches = state.rules.map(function (rule, order) { return { rule: rule, order: order }; }).filter(function (entry) {
      var rule = entry.rule;
      if (!rule.enabled) return false;
      var day = wall.day; var weekday = wall.weekday;
      if (!rule.allDay) {
        var start = minute(rule.startTime); var end = minute(rule.endTime);
        if (start < end) { if (wall.minute < start || wall.minute >= end) return false; }
        else {
          if (wall.minute < end) {
            var previous = new Date(wall.day + 'T12:00:00Z'); previous.setUTCDate(previous.getUTCDate() - 1);
            day = previous.toISOString().slice(0, 10); weekday = previous.getUTCDay();
          } else if (wall.minute < start) return false;
        }
      }
      if (rule.startDate && day < rule.startDate || rule.endDate && day > rule.endDate) return false;
      return rule.everyDay || rule.weekdays.indexOf(weekday) !== -1;
    }).sort(function (a, b) { return a.rule.priority - b.rule.priority || a.order - b.order; });
    var values = Object.create(null);
    matches.forEach(function (entry) { Object.keys(entry.rule.values).forEach(function (key) { values[key] = entry.rule.values[key]; }); });
    return { values: values, matchingIds: matches.map(function (entry) { return entry.rule.id; }) };
  }
  function stored() { return site.getBase('scheduledSettings') || defaults(); }
  function save(value) { validate(value); site.set('scheduledSettings', JSON.parse(JSON.stringify(value)), { requirePersistence: true }); }
  function tick() {
    var next;
    try { next = evaluate(stored(), new Date()); lastInvalid = false; }
    catch (error) {
      next = { values: Object.create(null), matchingIds: [] };
      if (!lastInvalid) { lastInvalid = true; site.notify('warning', 'schedule.invalid'); }
    }
    var old = effective;
    effective = next.values; matching = next.matchingIds;
    var keys = Array.from(new Set(Object.keys(old).concat(Object.keys(effective)))).filter(function (key) { return JSON.stringify(old[key]) !== JSON.stringify(effective[key]); });
    if (keys.length) { site.applyAppearance(); site.emit(keys.concat(['scheduledOverride'])); }
  }
  function start() {
    if (timer) return;
    site.subscribe(function (keys) { if (keys.indexOf('scheduledSettings') !== -1) tick(); });
    timer = global.setInterval(tick, 30000); tick();
    global.addEventListener('pagehide', function () { global.clearInterval(timer); timer = null; });
  }
  function searchablePicker(host, key, entries, current, onChange) {
    var select = doc.createElement('select'); select.className = 'select'; select.setAttribute('data-copy-attr', 'aria-label:' + key);
    entries.forEach(function (entry) { var option = doc.createElement('option'); option.value = String(entry.value); option.textContent = entry.label; option.disabled = !!entry.disabled; select.appendChild(option); });
    select.value = String(current);
    select.addEventListener('change', function () { onChange(select.value); });
    var search = global.BambuRegex.createSearchField({ labelKey: key, items: function () { return [].slice.call(select.options).map(function (option) { return { id: option.value, text: option.textContent }; }); }, sampleProvider: function () { return [].slice.call(select.options).map(function (option) { return option.textContent; }).join('\n'); }, onResults: function (ids) { [].slice.call(select.options).forEach(function (option) { option.hidden = ids !== null && ids.indexOf(option.value) === -1 && !option.selected; }); } });
    host.appendChild(search.element); host.appendChild(select); return select;
  }
  function mount(host) {
    host.innerHTML = '<div class="setting"><div class="setting-copy"><p class="setting-label" data-copy="schedule.heading"></p><p class="setting-desc" data-copy="schedule.explanation"></p><p class="hint" data-copy="schedule.sourcesUnavailable"></p></div><div class="setting-control"><button type="button" class="btn btn-outline" data-copy="schedule.add"></button><input type="file" accept="application/json,.json" data-copy-attr="aria-label:schedule.import"><button type="button" class="btn btn-outline" data-copy="schedule.export"></button></div></div><div class="setting schedule-zone"><div class="setting-copy"><p class="setting-label" data-copy="schedule.timezone"></p><p class="setting-desc" data-copy="schedule.timezone.desc"></p></div><div class="setting-control"></div></div><div class="schedule-search"></div><div class="schedule-rules"></div><p class="empty schedule-empty" data-copy="schedule.empty"></p><p class="hint schedule-status" role="status" aria-live="polite"></p>';
    var list = host.querySelector('.schedule-rules'); var status = host.querySelector('.schedule-status');
    var current;
    try { current = JSON.parse(JSON.stringify(validate(stored()))); } catch (error) { current = defaults(); site.setCopy(status, 'schedule.invalid'); }
    var zones = typeof Intl.supportedValuesOf === 'function' ? Intl.supportedValuesOf('timeZone') : [localZone(), 'UTC'];
    if (zones.indexOf(current.timeZone) === -1) zones.push(current.timeZone);
    var zoneSelect = searchablePicker(host.querySelector('.schedule-zone .setting-control'), 'schedule.timezone', zones.map(function (zone) { return { value: zone, label: zone }; }), current.timeZone, function (zone) { current.timeZone = zone; saveCurrent(); });
    var importGeneration = 0;
    function saveCurrent() {
      try { save(current); site.setCopy(status, 'schedule.saved'); return true; }
      catch (error) { site.setCopy(status, 'schedule.invalid'); return false; }
    }
    function labelInput(parent, key, type, value, callback) {
      var label = doc.createElement('label'); label.className = 'schedule-field';
      var text = doc.createElement('span'); text.setAttribute('data-copy', key); label.appendChild(text);
      var input = doc.createElement('input'); input.type = type; input.setAttribute('data-copy-attr', 'aria-label:' + key);
      if (type === 'checkbox') input.checked = value; else input.value = value;
      input.addEventListener(type === 'checkbox' ? 'change' : 'input', function () { callback(type === 'checkbox' ? input.checked : input.value, input); });
      label.appendChild(input); parent.appendChild(label); return input;
    }
    function render() {
      list.innerHTML = '';
      host.querySelector('.schedule-empty').hidden = current.rules.length > 0;
      current.rules.forEach(function (rule) {
        var card = doc.createElement('section'); card.className = 'setting schedule-rule'; card.dataset.ruleId = rule.id;
        var body = doc.createElement('div'); body.className = 'schedule-form'; card.appendChild(body);
        labelInput(body, 'schedule.label', 'text', rule.label, function (value, input) { rule.label = value; input.setCustomValidity(saveCurrent() ? '' : site.text('schedule.invalid')); });
        labelInput(body, 'schedule.enabled', 'checkbox', rule.enabled, function (value) { rule.enabled = value; saveCurrent(); });
        var priority = labelInput(body, 'schedule.priority', 'number', rule.priority, function (value) { rule.priority = Number(value); saveCurrent(); }); priority.min = '0'; priority.max = '999'; priority.step = '1';
        labelInput(body, 'schedule.startDate', 'date', rule.startDate, function (value) { rule.startDate = value; saveCurrent(); });
        labelInput(body, 'schedule.endDate', 'date', rule.endDate, function (value) { rule.endDate = value; saveCurrent(); });
        var start = labelInput(body, 'schedule.startTime', 'time', rule.startTime, function (value) { rule.startTime = value; saveCurrent(); });
        var end = labelInput(body, 'schedule.endTime', 'time', rule.endTime, function (value) { rule.endTime = value; saveCurrent(); });
        function paintAllDay() { start.value = rule.startTime; end.value = rule.endTime; start.disabled = rule.allDay; end.disabled = rule.allDay; start.title = end.title = rule.allDay ? site.text('schedule.allDay.desc') : ''; }
        labelInput(body, 'schedule.allDay', 'checkbox', rule.allDay, function (value) { rule.allDay = value; rule.startTime = value ? '' : '09:00'; rule.endTime = value ? '' : '17:00'; paintAllDay(); saveCurrent(); }); paintAllDay();
        var weekdays = doc.createElement('div'); weekdays.className = 'schedule-weekdays'; body.appendChild(weekdays);
        function paintDays() { weekdays.querySelectorAll('input').forEach(function (input) { input.disabled = rule.everyDay; input.title = rule.everyDay ? site.text('schedule.everyDay.desc') : ''; input.checked = rule.weekdays.indexOf(Number(input.dataset.day)) !== -1; }); }
        labelInput(body, 'schedule.everyDay', 'checkbox', rule.everyDay, function (value) { rule.everyDay = value; rule.weekdays = value ? [] : [1, 2, 3, 4, 5]; paintDays(); saveCurrent(); });
        for (var day = 0; day < 7; day++) (function (weekday) { var input = labelInput(weekdays, 'schedule.day.' + weekday, 'checkbox', rule.weekdays.indexOf(weekday) !== -1, function (value) { rule.weekdays = rule.weekdays.filter(function (candidate) { return candidate !== weekday; }); if (value) rule.weekdays.push(weekday); saveCurrent(); }); input.dataset.day = String(weekday); })(day);
        paintDays();
        var source = doc.createElement('div'); source.className = 'schedule-source'; body.appendChild(source);
        searchablePicker(source, 'schedule.source', [{ value: 'local', label: site.text('schedule.local') }, { value: 'api', label: site.text('schedule.apiUnavailable'), disabled: true }, { value: 'homeAssistant', label: site.text('schedule.homeUnavailable'), disabled: true }], 'local', function () {});
        var valuesHost = doc.createElement('div'); valuesHost.className = 'schedule-values'; body.appendChild(valuesHost);
        Object.keys(rule.values).forEach(function (key) {
          var spec = FIELDS[key];
          if (spec.type === 'elementStyles') {
            var captureStyle = doc.createElement('button'); captureStyle.type = 'button'; captureStyle.className = 'btn btn-outline'; captureStyle.setAttribute('data-copy', 'schedule.captureStyles');
            captureStyle.addEventListener('click', function () { rule.values[key] = JSON.parse(JSON.stringify(site.getBase('elementStyles') || {})); saveCurrent(); });
            valuesHost.appendChild(captureStyle);
          }
          else if (spec.type === 'select') searchablePicker(valuesHost, spec.copy, spec.values.map(function (value) { return { value: value, label: String(value) }; }), rule.values[key], function (value) { rule.values[key] = typeof spec.values[0] === 'number' ? Number(value) : value; saveCurrent(); });
          else if (spec.type === 'boolean') labelInput(valuesHost, spec.copy, 'checkbox', rule.values[key], function (value) { rule.values[key] = value; saveCurrent(); });
          else { var input = labelInput(valuesHost, spec.copy, spec.type === 'color' ? 'color' : 'number', rule.values[key], function (value) { rule.values[key] = spec.type === 'number' ? Number(value) : value; saveCurrent(); }); if (spec.type === 'number') { input.min = spec.min; input.max = spec.max; input.step = spec.step || '1'; } }
        });
        var target = doc.createElement('div'); target.className = 'schedule-target'; body.appendChild(target);
        var availableTargets = Object.keys(FIELDS).filter(function (key) { return !Object.prototype.hasOwnProperty.call(rule.values, key); });
        var targetSelect = searchablePicker(target, 'schedule.addValue', availableTargets.map(function (key) { return { value: key, label: site.text(FIELDS[key].copy) }; }), availableTargets[0] || '', function () {});
        var addValue = doc.createElement('button'); addValue.type = 'button'; addValue.className = 'btn btn-outline'; addValue.setAttribute('data-copy', 'schedule.addValue');
        addValue.disabled = targetSelect.options.length === 0; addValue.title = addValue.disabled ? site.text('schedule.allValues') : '';
        addValue.addEventListener('click', function () { if (!FIELDS[targetSelect.value]) return; rule.values[targetSelect.value] = targetSelect.value === 'languageMode' ? site.languageMode() : site.getBase(targetSelect.value); if (saveCurrent()) render(); }); target.appendChild(addValue);
        var remove = doc.createElement('button'); remove.type = 'button'; remove.className = 'btn btn-outline'; remove.setAttribute('data-copy', 'schedule.remove');
        remove.addEventListener('click', function () { global.BambuConfirmation.ask({ opener: remove, actionKey: 'schedule.remove.desc', params: { label: rule.label }, run: function () { current.rules = current.rules.filter(function (item) { return item.id !== rule.id; }); if (!saveCurrent()) throw new Error('Could not store schedule'); }, onComplete: render }); }); body.appendChild(remove);
        list.appendChild(card);
      });
      site.applyCopy(host);
    }
    var search = global.BambuRegex.createSearchField({ labelKey: 'schedule.search', items: function () { return current.rules.map(function (rule) { return { id: rule.id, text: rule.label + ' ' + Object.keys(rule.values).map(function (key) { return site.text(FIELDS[key].copy) + ' ' + String(rule.values[key]); }).join(' ') }; }); }, sampleProvider: function () { return current.rules.map(function (rule) { return rule.label; }).join('\n'); }, onResults: function (ids) { list.querySelectorAll('.schedule-rule').forEach(function (card) { card.hidden = ids !== null && ids.indexOf(card.dataset.ruleId) === -1; }); host.querySelector('.schedule-empty').hidden = ids === null ? current.rules.length > 0 : ids.length > 0; } });
    host.querySelector('.schedule-search').appendChild(search.element);
    var add = host.querySelector('[data-copy="schedule.add"]');
    add.disabled = !global.crypto || !global.crypto.randomUUID; add.title = add.disabled ? site.text('schedule.idUnavailable') : '';
    add.addEventListener('click', function () {
      if (current.rules.length >= MAX_RULES) { site.setCopy(status, 'schedule.limit'); return; }
      current.rules.push({ id: global.crypto.randomUUID(), label: site.text('schedule.new'), enabled: false, priority: 0, startDate: '', endDate: '', startTime: '', endTime: '', allDay: true, everyDay: true, weekdays: [], source: 'local', values: { theme: site.DEFAULTS.theme } });
      if (saveCurrent()) render();
    });
    host.querySelector('input[type=file]').addEventListener('change', async function (event) {
      var file = event.target.files && event.target.files[0]; event.target.value = '';
      var generation = ++importGeneration;
      if (!file) return;
      try { if (file.size > 262144) throw new Error('size'); var next = parse(await file.text()); if (generation !== importGeneration) return; save(next); current = JSON.parse(JSON.stringify(next)); if (![].slice.call(zoneSelect.options).some(function (option) { return option.value === next.timeZone; })) { var zoneOption = doc.createElement('option'); zoneOption.value = zoneOption.textContent = next.timeZone; zoneSelect.appendChild(zoneOption); } zoneSelect.value = next.timeZone; render(); site.setCopy(status, 'schedule.saved'); }
      catch (error) { if (generation === importGeneration) site.setCopy(status, 'schedule.invalid'); }
    });
    host.querySelector('[data-copy="schedule.export"]').addEventListener('click', function () {
      try { validate(current); site.downloadText('scheduled-settings.json', JSON.stringify(current, null, 2)); }
      catch (error) { site.setCopy(status, 'schedule.invalid'); }
    });
    render();
  }
  global.BambuSchedule = { FIELDS: FIELDS, MAX_RULES: MAX_RULES, defaults: defaults, validate: validate, parse: parse, evaluate: evaluate, save: save, start: start, mount: mount, override: function (key) { return Object.prototype.hasOwnProperty.call(effective, key) ? effective[key] : undefined; }, matchingIds: function () { return matching.slice(); } };
})(typeof window !== 'undefined' ? window : globalThis);
