/* Two independently operated keys and a full-range slider, scoped to one action. */
(function (global) {
  'use strict';
  var site = global.BambuSite;
  var doc = global.document;
  var serial = 0;
  function createAuthorization(run) {
    var keys = [false, false];
    var consumed = false;
    var cancelled = false;
    return {
      key: function (index, value) { if (index !== 0 && index !== 1 || consumed || cancelled || typeof value !== 'boolean') return false; keys[index] = value; return true; },
      ready: function () { return keys[0] && keys[1] && !consumed && !cancelled; },
      move: function (value) {
        if (typeof value !== 'number' || value !== 100 || !keys[0] || !keys[1] || consumed || cancelled) return false;
        consumed = true; run(); return true;
      },
      cancel: function () { if (consumed) return false; cancelled = true; return true; },
      consumed: function () { return consumed; }
    };
  }
  function ask(options) {
    if (!options || typeof options.run !== 'function' || !options.actionKey || !options.opener || !site.known(options.actionKey)) throw new Error('A known scoped action, callback and opener are required');
    var opener = options.opener;
    var id = 'site-confirm-' + (++serial);
    var panel = doc.createElement('section'); panel.className = 'site-confirmation';
    panel.setAttribute('role', 'dialog'); panel.setAttribute('aria-labelledby', id + '-title'); panel.setAttribute('aria-describedby', id + '-affected');
    panel.innerHTML = '<h2 id="' + id + '-title" data-copy="confirm.heading"></h2><p id="' + id + '-affected" data-copy="' + options.actionKey + '"></p><p data-copy="confirm.explanation"></p><div class="confirmation-search"></div><div class="confirmation-stage" data-stage="key1"><label><input type="checkbox" data-key="0"><span data-copy="confirm.key1"></span></label></div><div class="confirmation-stage" data-stage="key2"><label><input type="checkbox" data-key="1"><span data-copy="confirm.key2"></span></label></div><div class="confirmation-stage" data-stage="slider"><label><span data-copy="confirm.slider"></span><input type="range" min="0" max="100" step="1" value="0" disabled data-copy-attr="aria-label:confirm.slider"></label><progress max="100" value="0" data-copy-attr="aria-label:confirm.progress"></progress><p class="hint" data-copy="confirm.locked"></p></div><p class="empty confirmation-empty" hidden data-copy="confirm.empty"></p><p class="confirmation-status" role="status" aria-live="polite"></p><button type="button" class="btn btn-outline" data-copy="confirm.exit"></button>';
    if (options.params) panel.querySelector('#' + id + '-affected').setAttribute('data-copy-params', JSON.stringify(options.params));
    doc.body.appendChild(panel);
    var slider = panel.querySelector('input[type=range]');
    var progress = panel.querySelector('progress');
    var status = panel.querySelector('.confirmation-status');
    var explanation = panel.querySelector('[data-copy="confirm.locked"]');
    var busy = false;
    var closed = false;
    function close() {
      if (closed) return;
      closed = true;
      global.removeEventListener('resize', position); global.removeEventListener('scroll', position, true); doc.removeEventListener('keydown', onKey);
      panel.remove();
      if (doc.contains(opener)) opener.focus();
    }
    var authorization = createAuthorization(function () {
      if (busy || closed) return;
      busy = true; slider.disabled = true;
      panel.querySelectorAll('input[type=checkbox]').forEach(function (key) { key.disabled = true; });
      site.setCopy(status, 'confirm.complete');
      panel.classList.add('confirmation-complete');
      Promise.resolve().then(options.run).then(function () { close(); if (options.onComplete) options.onComplete(); }, function () {
        site.setCopy(status, 'confirm.failed'); busy = false;
        // This authorization is consumed. A new prompt is required for retry.
      });
    });
    panel.querySelectorAll('input[type=checkbox]').forEach(function (key) {
      key.addEventListener('change', function () {
        authorization.key(Number(key.dataset.key), key.checked);
        slider.value = '0'; progress.value = 0; slider.disabled = !authorization.ready();
        site.setCopy(explanation, authorization.ready() ? 'confirm.ready' : 'confirm.locked');
      });
    });
    slider.addEventListener('input', function () {
      if (!authorization.ready() || busy || closed) { slider.value = '0'; return; }
      var value = Number(slider.value); progress.value = value;
      panel.style.setProperty('--confirmation-progress', String(value / 100));
      authorization.move(value);
    });
    panel.querySelector('button[data-copy="confirm.exit"]').addEventListener('click', function () { authorization.cancel(); close(); });
    function onKey(event) {
      if (event.key === 'Escape') { event.preventDefault(); authorization.cancel(); close(); }
      if (event.key === 'Tab' && panel.getAttribute('aria-modal') === 'true') {
        var focusable = [].slice.call(panel.querySelectorAll('input:not(:disabled), button:not(:disabled), select:not(:disabled), textarea:not(:disabled), a[href]')).filter(function (item) { return item.getClientRects().length > 0; });
        var first = focusable[0]; var last = focusable[focusable.length - 1];
        if (event.shiftKey && doc.activeElement === first) { event.preventDefault(); last.focus(); }
        else if (!event.shiftKey && doc.activeElement === last) { event.preventDefault(); first.focus(); }
      }
    }
    function position() {
      var anchor = opener.getBoundingClientRect();
      var width = Math.min(620, Math.max(280, global.innerWidth - 16));
      panel.style.width = width + 'px'; panel.style.maxHeight = Math.max(180, global.innerHeight - 16) + 'px';
      var height = Math.min(panel.scrollHeight, global.innerHeight - 16);
      var left = Math.max(8, Math.min(anchor.left, global.innerWidth - width - 8));
      var below = global.innerHeight - anchor.bottom - 16;
      var above = anchor.top - 16;
      if (height <= below) { panel.style.top = (anchor.bottom + 8) + 'px'; panel.setAttribute('aria-modal', 'false'); }
      else if (height <= above) { panel.style.top = Math.max(8, anchor.top - height - 8) + 'px'; panel.setAttribute('aria-modal', 'false'); }
      else { panel.style.top = '8px'; panel.setAttribute('aria-modal', 'true'); }
      panel.style.left = left + 'px';
    }
    var rows = [].slice.call(panel.querySelectorAll('.confirmation-stage'));
    var labels = { key1: 'confirm.key1', key2: 'confirm.key2', slider: 'confirm.slider' };
    var search = global.BambuRegex.createSearchField({ labelKey: 'confirm.search', items: function () { return rows.map(function (row) { return { id: row.dataset.stage, text: site.text(labels[row.dataset.stage]) }; }); }, sampleProvider: function () { return rows.map(function (row) { return site.text(labels[row.dataset.stage]); }).join('\n'); }, onResults: function (ids) {
      rows.forEach(function (row) { row.hidden = ids !== null && ids.indexOf(row.dataset.stage) === -1; });
      panel.querySelector('.confirmation-empty').hidden = ids === null || ids.length > 0; position();
    } });
    panel.querySelector('.confirmation-search').appendChild(search.element);
    doc.addEventListener('keydown', onKey); global.addEventListener('resize', position); global.addEventListener('scroll', position, true);
    site.applyCopy(panel); position(); panel.querySelector('input[type=checkbox]').focus();
    return { close: function () { authorization.cancel(); close(); } };
  }
  global.BambuConfirmation = { createAuthorization: createAuthorization, ask: ask };
})(typeof window !== 'undefined' ? window : globalThis);
