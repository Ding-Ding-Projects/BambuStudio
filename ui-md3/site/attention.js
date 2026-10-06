/* Independent, opt-in interface accommodations for this visitor's page. */
(function (global) {
  'use strict';
  var site = global.BambuSite;
  var doc = global.document;
  var startTime = Date.now();
  var lastChange = startTime;
  var lastPrompt = 0;
  var timer = null;
  var bar = null;
  var currentFocus = null;
  var MODES = ['attentionFocus', 'attentionLow', 'attentionTime', 'attentionOne', 'attentionMomentum'];

  function elapsed(now) { return { session: Math.max(0, Math.floor((now - startTime) / 60000)), unchanged: Math.max(0, Math.floor((now - lastChange) / 60000)) }; }
  function setNextAction(value) {
    if (typeof value !== 'string' || value.length > 512 || /[\u0000-\u001f\u007f]/.test(value)) return false;
    site.set('attentionNextAction', value);
    return true;
  }
  function snooze(now) {
    site.set('attentionSnoozeUntil', now + 30 * 60000);
    return now + 30 * 60000;
  }
  function shouldPrompt(now) {
    return !!site.get('attentionMomentum') && !site.get('attentionLow') && now - lastChange >= 20 * 60000 && now >= Number(site.get('attentionSnoozeUntil') || 0) && now - lastPrompt >= 20 * 60000;
  }
  function clearFocus() {
    if (currentFocus) currentFocus.classList.remove('attention-current');
    currentFocus = null;
    doc.querySelectorAll('.panel').forEach(function (panel) { panel.classList.remove('attention-focus-active'); });
  }
  function focus(target) {
    if (!site.get('attentionFocus') || !target || !target.closest) return;
    var item = target.closest('.setting, .card, .release, .screen-card, .section-head');
    if (!item || !item.closest('.panel')) return;
    clearFocus();
    currentFocus = item;
    item.classList.add('attention-current');
    item.closest('.panel').classList.add('attention-focus-active');
  }
  function tick(now) {
    if (bar) {
      var clock = bar.querySelector('.attention-clock');
      clock.hidden = !site.get('attentionTime');
      var times = elapsed(now);
      site.setCopy(clock, 'attention.clock.line', { session: times.session, unchanged: times.unchanged });
      var action = bar.querySelector('.attention-action');
      action.hidden = !site.get('attentionOne');
      var value = site.get('attentionNextAction');
      action.querySelector('.attention-current-action').textContent = value || site.text('attention.action.empty');
      bar.hidden = !site.get('attentionTime') && !site.get('attentionOne');
    }
    if (shouldPrompt(now)) {
      lastPrompt = now;
      site.notify('info', 'attention.momentum.line', { minutes: elapsed(now).unchanged }, { action: { key: 'attention.snooze', run: function () { snooze(Date.now()); } } });
    }
  }
  function apply() {
    doc.documentElement.setAttribute('data-low-stimulation', String(!!site.get('attentionLow')));
    if (!site.get('attentionFocus')) clearFocus();
    tick(Date.now());
  }
  function start() {
    if (timer) return;
    bar = doc.createElement('div'); bar.className = 'attention-bar wrap';
    bar.innerHTML = '<p class="attention-clock" role="timer" aria-live="off"></p><div class="attention-action"><span data-copy="attention.action.current"></span><span class="attention-current-action"></span><button type="button" class="btn btn-outline" data-copy="attention.action.edit"></button></div>';
    var header = doc.querySelector('header');
    if (header) header.appendChild(bar); else doc.body.insertBefore(bar, doc.body.firstChild);
    bar.querySelector('button').addEventListener('click', function () {
      if (global.BambuSiteTabs) global.BambuSiteTabs.activate('settings', { focus: true });
      var input = doc.querySelector('[data-attention-next-action]');
      if (input) { input.scrollIntoView({ block: 'center' }); input.focus(); }
    });
    doc.addEventListener('focusin', function (event) { focus(event.target); });
    doc.addEventListener('pointerdown', function (event) { focus(event.target); });
    doc.addEventListener('input', function (event) { if (event.isTrusted) lastChange = Date.now(); });
    site.subscribe(function (keys) {
      if (keys.some(function (key) { return key !== 'notifications' && key !== 'attentionSnoozeUntil'; })) lastChange = Date.now();
      apply();
    });
    apply(); site.applyCopy(bar);
    timer = global.setInterval(function () { tick(Date.now()); }, 30000);
    global.addEventListener('pagehide', function () { global.clearInterval(timer); timer = null; });
  }
  function mount(host) {
    var labels = ['attention.focus', 'attention.low', 'attention.time', 'attention.one', 'attention.momentum'];
    labels.forEach(function (key, index) {
      var row = doc.createElement('div'); row.className = 'setting';
      row.innerHTML = '<div class="setting-copy"><p class="setting-label" data-copy="' + key + '"></p><p class="setting-desc" data-copy="' + key + '.desc"></p></div><div class="setting-control"><button type="button" class="switch" role="switch" data-copy-attr="aria-label:' + key + '"></button></div>';
      var button = row.querySelector('button');
      function paint() { button.setAttribute('aria-checked', String(!!site.get(MODES[index]))); button.classList.toggle('on', !!site.get(MODES[index])); }
      button.addEventListener('click', function () { site.set(MODES[index], !site.get(MODES[index])); paint(); });
      site.subscribe(paint); paint(); host.appendChild(row);
    });
    var next = doc.createElement('div'); next.className = 'setting';
    next.innerHTML = '<div class="setting-copy"><p class="setting-label" data-copy="attention.action.current"></p><p class="setting-desc" data-copy="attention.action.desc"></p></div><div class="setting-control"><input type="text" maxlength="512" data-attention-next-action data-copy-attr="aria-label:attention.action.current"><button type="button" class="btn btn-outline" data-copy="attention.focus.clear"></button></div>';
    var input = next.querySelector('input'); input.value = site.get('attentionNextAction') || '';
    input.addEventListener('input', function () { if (!setNextAction(input.value)) input.setCustomValidity(site.text('attention.action.invalid')); else input.setCustomValidity(''); });
    next.querySelector('button').addEventListener('click', clearFocus);
    host.appendChild(next);
    var snoozeRow = doc.createElement('div'); snoozeRow.className = 'setting';
    snoozeRow.innerHTML = '<div class="setting-copy"><p class="setting-label" data-copy="attention.snooze"></p><p class="setting-desc" data-copy="attention.snooze.desc"></p></div><div class="setting-control"><button type="button" class="btn btn-outline" data-copy="attention.snooze"></button></div>';
    snoozeRow.querySelector('button').addEventListener('click', function () { snooze(Date.now()); }); host.appendChild(snoozeRow);
    site.applyCopy(host);
  }
  global.BambuAttention = { MODES: MODES, elapsed: elapsed, setNextAction: setNextAction, snooze: snooze, shouldPrompt: shouldPrompt, tick: tick, start: start, mount: mount };
})(typeof window !== 'undefined' ? window : globalThis);
