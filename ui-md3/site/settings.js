/*
 * Settings surface and the shared appearance controls.
 *
 * Every control here writes straight through to the preference store and takes
 * effect immediately; nothing waits for an Apply button. The search bar at the
 * top is the same component every other search bar on this site uses, so plain
 * text is the default and the full regex builder is one button away.
 *
 * The search covers this surface AND the controls that live on other tabs: when
 * a match is somewhere else the result says which tab it is on and offers to go
 * there, rather than quietly returning nothing.
 */
(function (global) {
  'use strict';

  var site = global.BambuSite;
  var doc = global.document;

  var ACCENTS = ['#22c55e', '#7c5cff', '#14b8a6', '#3b82f6', '#f97316', '#ec4899'];

  /* ------------------------------------------------- shared controls */

  function mountThemeControls(root) {
    root.querySelectorAll('.seg[data-group]').forEach(function (group) {
      var name = group.dataset.group;
      function paint() {
        group.querySelectorAll('button').forEach(function (button) {
          button.setAttribute('aria-pressed', String(site.get(name) === button.dataset.val));
        });
      }
      group.addEventListener('click', function (event) {
        var button = event.target.closest('button');
        if (!button) return;
        site.set(name, button.dataset.val);
        site.applyAppearance();
        paint();
      });
      paint();
      site.subscribe(function (keys) { if (keys.indexOf(name) !== -1) paint(); });
    });

    root.querySelectorAll('.swatches').forEach(function (host) {
      host.innerHTML = ACCENTS.map(function (color) {
        return '<button type="button" class="swatch" data-color="' + color + '" ' +
          'title="' + color + '" aria-label="' + color + '" style="background:' + color + '"></button>';
      }).join('');
      function paint() {
        host.querySelectorAll('.swatch').forEach(function (swatch) {
          swatch.setAttribute('aria-pressed', String(swatch.dataset.color === site.get('accent')));
        });
      }
      host.addEventListener('click', function (event) {
        var swatch = event.target.closest('.swatch');
        if (!swatch) return;
        site.set('accent', swatch.dataset.color);
        site.applyAppearance();
        paint();
      });
      paint();
      site.subscribe(function (keys) { if (keys.indexOf('accent') !== -1) paint(); });
    });
  }

  /* --------------------------------------------------- font detection */

  function availableFonts() {
    // Labels stay short enough to read inside a closed select at 160 CSS px;
    // which faces are bundled is stated in the setting's description instead.
    var candidates = [
      { id: 'roboto', label: 'Roboto', probe: null },
      { id: 'mono', label: 'Roboto Mono', probe: null },
      { id: 'system', label: 'System UI', probe: null },
      { id: 'segoe', label: 'Segoe UI', probe: '12px "Segoe UI"' },
      { id: 'arial', label: 'Arial', probe: '12px Arial' },
      { id: 'georgia', label: 'Georgia', probe: '12px Georgia' }
    ];
    return candidates.filter(function (candidate) {
      if (!candidate.probe) return true;
      try { return doc.fonts && doc.fonts.check(candidate.probe); } catch (error) { return true; }
    });
  }

  /* ------------------------------------------------------- settings ui */

  var ELEMENT_TARGETS = [
    { id: 'tabstrip', copy: 'settings.element.tabstrip' },
    { id: 'cards', copy: 'settings.element.cards' },
    { id: 'hero', copy: 'settings.element.hero' },
    { id: 'toasts', copy: 'settings.element.toasts' }
  ];

  // Controls that live on other tabs, so a settings search can still find them.
  var ELSEWHERE = [
    { tab: 'materialyou', copy: 'settings.theme', tabCopy: 'tab.materialyou' },
    { tab: 'materialyou', copy: 'settings.density', tabCopy: 'tab.materialyou' },
    { tab: 'materialyou', copy: 'settings.accent', tabCopy: 'tab.materialyou' },
    { tab: 'changelog', copy: 'changelog.search', tabCopy: 'tab.changelog' },
    { tab: 'changelog', copy: 'changelog.from', tabCopy: 'tab.changelog' },
    { tab: 'changelog', copy: 'changelog.to', tabCopy: 'tab.changelog' },
    { tab: 'regex', copy: 'regex.flags', tabCopy: 'tab.regex' },
    { tab: 'regex', copy: 'regex.sample', tabCopy: 'tab.regex' }
  ];

  function searchTextFor(host) {
    var keys = [].slice.call(host.querySelectorAll('[data-copy]')).map(function (node) {
      return node.getAttribute('data-copy');
    });
    var parts = [];
    keys.forEach(function (key) {
      var both = site.pair(key);
      parts.push(both.en, both.yue, key);
    });
    parts.push(host.dataset.value || '');
    return parts.join(' ');
  }

  function renderSettings(panel) {
    panel.innerHTML =
      '<div class="section-head">' +
        '<h2 data-copy="settings.heading"></h2>' +
        '<p data-copy="settings.body"></p>' +
      '</div>' +
      '<div class="settings-search"></div>' +
      '<p class="hint settings-search-hint" data-copy="settings.search.hint"></p>' +
      '<div class="settings-elsewhere" hidden></div>' +
      '<div class="settings-groups"></div>';

    var groupHost = panel.querySelector('.settings-groups');
    var elsewhereHost = panel.querySelector('.settings-elsewhere');

    var groups = [
      { key: 'settings.group.language', build: buildLanguageGroup },
      { key: 'settings.group.appearance', build: buildAppearanceGroup },
      { key: 'settings.group.typography', build: buildTypographyGroup },
      { key: 'settings.group.elements', build: buildElementGroup },
      { key: 'settings.group.surprises', build: buildSurpriseGroup },
      { key: 'settings.group.data', build: buildDataGroup }
    ];

    groups.forEach(function (group) {
      var section = doc.createElement('section');
      section.className = 'settings-group';
      section.dataset.group = group.key;
      var heading = doc.createElement('h3');
      heading.className = 'settings-group-title';
      heading.setAttribute('data-copy', group.key);
      section.appendChild(heading);
      var body = doc.createElement('div');
      body.className = 'settings-group-body';
      section.appendChild(body);
      group.build(body);
      groupHost.appendChild(section);
    });

    // Each row and each off-tab control is an addressable item, so an opt-in
    // regex can be evaluated inside the terminable worker rather than here.
    function searchableItems() {
      var items = [].slice.call(groupHost.querySelectorAll('.setting'))
        .map(function (setting, index) {
          setting.dataset.searchId = 'row-' + index;
          return { id: 'row-' + index, text: searchTextFor(setting) };
        });
      ELSEWHERE.forEach(function (entry, index) {
        var both = site.pair(entry.copy);
        items.push({ id: 'away-' + index, text: both.en + ' ' + both.yue });
      });
      return items;
    }

    var searchField = global.BambuRegex.createSearchField({
      labelKey: 'settings.search',
      sampleProvider: function () {
        return [].slice.call(groupHost.querySelectorAll('.setting'))
          .map(searchTextFor).join('\n');
      },
      items: searchableItems,
      onResults: function (ids) { filter(ids); }
    });
    panel.querySelector('.settings-search').appendChild(searchField.element);

    function filter(ids) {
      var anyVisible = false;
      groupHost.querySelectorAll('.settings-group').forEach(function (section) {
        var visible = 0;
        section.querySelectorAll('.setting').forEach(function (setting) {
          var hit = ids === null || ids.indexOf(setting.dataset.searchId) !== -1;
          setting.hidden = !hit;
          if (hit) visible++;
        });
        section.hidden = visible === 0;
        if (visible) anyVisible = true;
      });

      // A setting the user is looking for may live on another tab entirely.
      var elsewhere = ids === null ? [] : ELSEWHERE.filter(function (entry, index) {
        return ids.indexOf('away-' + index) !== -1;
      });
      elsewhereHost.innerHTML = '';
      elsewhereHost.hidden = elsewhere.length === 0;
      var byTab = {};
      elsewhere.forEach(function (entry) {
        byTab[entry.tab] = byTab[entry.tab] || { count: 0, tabCopy: entry.tabCopy };
        byTab[entry.tab].count++;
      });
      Object.keys(byTab).forEach(function (tab) {
        var note = doc.createElement('button');
        note.type = 'button';
        note.className = 'elsewhere-note';
        note.innerHTML = '<span data-icon aria-hidden="true">north_east</span>' +
          '<span data-copy="settings.search.elsewhere" data-copy-params=\'' +
          JSON.stringify({ count: byTab[tab].count, group: site.text(byTab[tab].tabCopy) }) + '\'></span>';
        note.addEventListener('click', function () {
          if (global.BambuSiteTabs) global.BambuSiteTabs.activate(tab, { focus: true });
        });
        elsewhereHost.appendChild(note);
      });
      site.applyCopy(elsewhereHost);

      var empty = panel.querySelector('.settings-empty');
      if (!anyVisible && ids !== null && !elsewhere.length) {
        if (!empty) {
          empty = doc.createElement('p');
          empty.className = 'empty settings-empty';
          empty.setAttribute('data-copy', 'settings.search.empty');
          groupHost.appendChild(empty);
        }
        empty.hidden = false;
      } else if (empty) {
        empty.hidden = true;
      }
      site.applyCopy(panel);
    }

    site.applyCopy(panel);
  }

  /* --------------------------------------------------- group builders */

  function settingShell(host, key, descKey) {
    var setting = doc.createElement('div');
    setting.className = 'setting';
    setting.innerHTML =
      '<div class="setting-copy">' +
        '<p class="setting-label" data-copy="' + key + '"></p>' +
        (descKey ? '<p class="setting-desc" data-copy="' + descKey + '"></p>' : '') +
      '</div>';
    var control = doc.createElement('div');
    control.className = 'setting-control';
    setting.appendChild(control);
    host.appendChild(setting);
    return { setting: setting, control: control };
  }

  function buildLanguageGroup(host) {
    var mode = settingShell(host, 'settings.language.mode', 'settings.language.desc');
    /*
     * A wrapping radio group rather than a native <select>: a closed select can
     * only truncate "English + 廣東話" at 160 CSS px, and truncation is exactly
     * what this site refuses to do. These buttons wrap onto their own rows.
     */
    var group = doc.createElement('div');
    group.className = 'seg langseg';
    group.setAttribute('role', 'radiogroup');
    group.setAttribute('data-copy-attr', 'aria-label:settings.language.mode');
    var MODES = [['en', 'English'], ['yue_HK', '廣東話'], ['bilingual_en_yue_HK', 'English + 廣東話']];
    MODES.forEach(function (option) {
      var button = doc.createElement('button');
      button.type = 'button';
      button.setAttribute('role', 'radio');
      button.dataset.mode = option[0];
      button.textContent = option[1];
      button.lang = option[0] === 'yue_HK' ? 'yue-Hant-HK' : 'en';
      button.addEventListener('click', function () {
        site.setLanguageMode(option[0]);
        paintModes();
        site.notify('info', 'notify.saved', { name: site.text('settings.language.mode') });
      });
      group.appendChild(button);
    });
    function paintModes() {
      var active = site.languageMode();
      group.querySelectorAll('button').forEach(function (button) {
        button.setAttribute('aria-checked', String(button.dataset.mode === active));
        button.setAttribute('aria-pressed', String(button.dataset.mode === active));
      });
    }
    paintModes();
    site.subscribe(function (keys) { if (keys.indexOf('languageMode') !== -1) paintModes(); });
    mode.control.appendChild(group);

    [['funnyEn', 'settings.funny.en', 'en'], ['funnyYue', 'settings.funny.yue', 'yue']]
      .forEach(function (entry) {
        var shell = settingShell(host, entry[1], 'settings.funny.desc');
        var slider = doc.createElement('input');
        slider.type = 'range';
        slider.min = '1';
        slider.max = String(site.LEVELS);
        slider.step = '1';
        slider.className = 'slider';
        slider.value = String(site.get(entry[0]));
        slider.setAttribute('data-copy-attr', 'aria-label:' + entry[1]);
        var readout = doc.createElement('p');
        readout.className = 'slider-readout mono';
        var preview = doc.createElement('p');
        preview.className = 'slider-preview';
        preview.lang = entry[2] === 'yue' ? 'yue-Hant-HK' : 'en';
        function paint() {
          var level = Number(slider.value);
          readout.textContent = site.text('settings.funny.current', { level: level });
          // The preview shows a real message, so the level's effect is visible
          // before it is committed — including on a warning, which also varies.
          preview.textContent = site.preview('hero.headline', entry[2], level);
        }
        slider.addEventListener('input', function () {
          site.set(entry[0], Number(slider.value));
          paint();
        });
        slider.addEventListener('change', function () {
          site.notify('info', 'notify.saved', { name: site.text(entry[1]) });
        });
        site.subscribe(function (keys) {
          if (keys.indexOf(entry[0]) !== -1) {
            slider.value = String(site.get(entry[0]));
            paint();
          }
          if (keys.indexOf('languageMode') !== -1) paint();
        });
        var scale = doc.createElement('div');
        scale.className = 'slider-scale';
        scale.innerHTML = '<span data-copy="settings.funny.level1"></span>' +
          '<span data-copy="settings.funny.level5"></span>';
        shell.control.appendChild(slider);
        shell.control.appendChild(scale);
        shell.control.appendChild(readout);
        shell.control.appendChild(preview);
        paint();
      });
  }

  function buildAppearanceGroup(host) {
    var theme = settingShell(host, 'settings.theme', 'settings.theme.desc');
    theme.control.innerHTML =
      '<div class="seg" data-group="theme">' +
        '<button type="button" data-val="light"><span data-icon aria-hidden="true">light_mode</span>' +
          '<span data-copy="you.light"></span></button>' +
        '<button type="button" data-val="dark"><span data-icon aria-hidden="true">dark_mode</span>' +
          '<span data-copy="you.dark"></span></button>' +
      '</div>';

    var density = settingShell(host, 'settings.density', 'settings.density.desc');
    density.control.innerHTML =
      '<div class="seg" data-group="density">' +
        '<button type="button" data-val="comfortable"><span data-copy="you.comfortable"></span></button>' +
        '<button type="button" data-val="compact"><span data-copy="you.compact"></span></button>' +
      '</div>';

    var accent = settingShell(host, 'settings.accent', 'settings.accent.desc');
    accent.control.innerHTML = '<div class="swatches"></div>';
    var custom = doc.createElement('label');
    custom.className = 'colorpicker';
    custom.innerHTML = '<span data-copy="settings.accent.custom"></span>' +
      '<input type="color" value="' + site.get('accent') + '">';
    custom.querySelector('input').addEventListener('input', function (event) {
      site.set('accent', event.target.value);
      site.applyAppearance();
    });
    accent.control.appendChild(custom);

    mountThemeControls(host);
  }

  function buildTypographyGroup(host) {
    var family = settingShell(host, 'settings.font.family', 'settings.font.family.desc');
    var select = doc.createElement('select');
    select.className = 'select';
    select.setAttribute('data-copy-attr', 'aria-label:settings.font.family');
    availableFonts().forEach(function (font) {
      var option = doc.createElement('option');
      option.value = font.id;
      option.textContent = font.label;
      select.appendChild(option);
    });
    select.value = site.get('fontFamily');
    select.addEventListener('change', function () {
      site.set('fontFamily', select.value);
      site.applyAppearance();
      site.notify('info', 'notify.saved', { name: site.text('settings.font.family') });
    });
    family.control.appendChild(select);

    var size = settingShell(host, 'settings.font.size', 'settings.font.size.desc');
    var sizeSlider = doc.createElement('input');
    sizeSlider.type = 'range';
    sizeSlider.min = '85';
    sizeSlider.max = '140';
    sizeSlider.step = '5';
    sizeSlider.className = 'slider';
    sizeSlider.value = String(site.get('fontScale'));
    sizeSlider.setAttribute('data-copy-attr', 'aria-label:settings.font.size');
    var sizeReadout = doc.createElement('p');
    sizeReadout.className = 'slider-readout mono';
    sizeReadout.textContent = site.get('fontScale') + '%';
    sizeSlider.addEventListener('input', function () {
      site.set('fontScale', Number(sizeSlider.value));
      sizeReadout.textContent = sizeSlider.value + '%';
      site.applyAppearance();
    });
    size.control.appendChild(sizeSlider);
    size.control.appendChild(sizeReadout);

    var weight = settingShell(host, 'settings.font.weight', 'settings.font.weight.desc');
    var weightSelect = doc.createElement('select');
    weightSelect.className = 'select';
    weightSelect.setAttribute('data-copy-attr', 'aria-label:settings.font.weight');
    [['400', 'Regular'], ['500', 'Medium'], ['700', 'Bold']].forEach(function (option) {
      var node = doc.createElement('option');
      node.value = option[0];
      node.textContent = option[1];
      weightSelect.appendChild(node);
    });
    weightSelect.value = String(site.get('fontWeight'));
    weightSelect.addEventListener('change', function () {
      site.set('fontWeight', Number(weightSelect.value));
      site.applyAppearance();
    });
    weight.control.appendChild(weightSelect);
  }

  function buildElementGroup(host) {
    var note = doc.createElement('p');
    note.className = 'setting-desc';
    note.setAttribute('data-copy', 'settings.elements.desc');
    host.appendChild(note);

    var pick = settingShell(host, 'settings.elements.pick');
    var select = doc.createElement('select');
    select.className = 'select';
    select.setAttribute('data-copy-attr', 'aria-label:settings.elements.pick');
    /*
     * A native <option> cannot wrap and cannot carry a secondary label, so in
     * bilingual mode it shows the primary language alone rather than a composed
     * string that the closed control would have to truncate at 160 CSS px.
     * The setting's own label and description are still fully bilingual.
     */
    function optionLabel(key) {
      var both = site.pair(key);
      return site.languageMode() === 'yue_HK' ? (both.yue || both.en) : both.en;
    }

    ELEMENT_TARGETS.forEach(function (target) {
      var option = doc.createElement('option');
      option.value = target.id;
      option.textContent = optionLabel(target.copy);
      select.appendChild(option);
    });
    pick.control.appendChild(select);
    // <option> text is not a data-copy target, so it is repainted explicitly
    // whenever the language mode or either funny level changes.
    site.subscribe(function (keys) {
      if (keys.indexOf('languageMode') === -1 && keys.indexOf('funnyEn') === -1 &&
          keys.indexOf('funnyYue') === -1) {
        return;
      }
      ELEMENT_TARGETS.forEach(function (target, index) {
        select.options[index].textContent = optionLabel(target.copy);
      });
    });

    /*
     * `size` is consumed as a bare multiplier inside calc(), so it is stored
     * unitless and only shown as a percentage. Writing "100%" into
     * `calc(44px * 100%)` makes the whole declaration invalid, which silently
     * collapsed the hero headline to the inherited body size.
     */
    var editors = [
      { property: 'radius', copy: 'settings.elements.radius', min: 0, max: 32, step: 2, unit: 'px', ratio: false },
      { property: 'spacing', copy: 'settings.elements.spacing', min: 0, max: 32, step: 2, unit: 'px', ratio: false },
      { property: 'size', copy: 'settings.elements.size', min: 80, max: 140, step: 5, unit: '%', ratio: true }
    ];
    var controls = {};
    editors.forEach(function (editor) {
      var shell = settingShell(host, editor.copy);
      var slider = doc.createElement('input');
      slider.type = 'range';
      slider.className = 'slider';
      slider.min = String(editor.min);
      slider.max = String(editor.max);
      slider.step = String(editor.step);
      slider.setAttribute('data-copy-attr', 'aria-label:' + editor.copy);
      var readout = doc.createElement('p');
      readout.className = 'slider-readout mono';
      slider.addEventListener('input', function () {
        var stored = editor.ratio
          ? String(Number(slider.value) / 100)
          : slider.value + editor.unit;
        site.elementStyle(select.value, editor.property, stored);
        readout.textContent = slider.value + editor.unit;
      });
      shell.control.appendChild(slider);
      shell.control.appendChild(readout);
      controls[editor.property] = { slider: slider, readout: readout, editor: editor };
    });

    var colorShell = settingShell(host, 'settings.elements.color');
    var color = doc.createElement('input');
    color.type = 'color';
    color.setAttribute('data-copy-attr', 'aria-label:settings.elements.color');
    color.addEventListener('input', function () {
      site.elementStyle(select.value, 'color', color.value);
    });
    colorShell.control.appendChild(color);

    var resetShell = settingShell(host, 'settings.elements.reset');
    var reset = doc.createElement('button');
    reset.type = 'button';
    reset.className = 'btn btn-outline';
    reset.setAttribute('data-copy', 'settings.elements.reset');
    reset.addEventListener('click', function () {
      site.resetElement(select.value);
      load();
      site.notify('info', 'notify.saved', { name: site.text('settings.elements.reset') });
    });
    resetShell.control.appendChild(reset);

    function load() {
      var stored = (site.get('elementStyles') || {})[select.value] || {};
      Object.keys(controls).forEach(function (property) {
        var entry = controls[property];
        var raw = stored[property];
        var value = raw === undefined
          ? defaultFor(property, entry.editor)
          : (entry.editor.ratio ? Math.round(parseFloat(raw) * 100) : parseFloat(raw));
        entry.slider.value = String(value);
        entry.readout.textContent = value + entry.editor.unit;
      });
      color.value = stored.color || readableDefaultColor();
    }
    function defaultFor(property, editor) {
      return property === 'size' ? 100 : (property === 'radius' ? 16 : 12);
    }
    function readableDefaultColor() {
      var computed = getComputedStyle(doc.documentElement).getPropertyValue('--md-on-surface').trim();
      return /^#[0-9a-f]{6}$/i.test(computed) ? computed : '#e8e7ee';
    }
    select.addEventListener('change', load);
    load();
  }

  function buildSurpriseGroup(host) {
    toggle(host, 'settings.dimsum', 'settings.dimsum.desc', 'dimSum', function (value) {
      if (!value) site.notify('info', 'notify.dimsum.off');
    });
    toggle(host, 'settings.notify.enabled', 'settings.notify.desc', 'notifications');
  }

  function toggle(host, key, descKey, prefKey, onChange) {
    var shell = settingShell(host, key, descKey);
    var button = doc.createElement('button');
    button.type = 'button';
    button.className = 'switch';
    button.setAttribute('role', 'switch');
    button.setAttribute('data-copy-attr', 'aria-label:' + key);
    function paint() {
      var on = Boolean(site.get(prefKey));
      button.setAttribute('aria-checked', String(on));
      button.classList.toggle('on', on);
    }
    button.addEventListener('click', function () {
      var next = !site.get(prefKey);
      site.set(prefKey, next);
      paint();
      if (onChange) onChange(next);
    });
    paint();
    shell.control.appendChild(button);
  }

  function buildDataGroup(host) {
    var shell = settingShell(host, 'settings.reset', 'settings.reset.desc');
    var button = doc.createElement('button');
    button.type = 'button';
    button.className = 'btn btn-danger';
    button.setAttribute('data-copy', 'settings.reset');
    button.addEventListener('click', function () { confirmReset(button); });
    shell.control.appendChild(button);
  }

  /*
   * A blocking dialog is correct here and only here: deleting stored settings is
   * a decision the user must make before anything happens. Everything else on
   * this site reports through non-blocking notifications.
   */
  function confirmReset(opener) {
    var scrim = doc.createElement('div');
    scrim.className = 'scrim';
    var dialog = doc.createElement('div');
    dialog.className = 'dialog';
    dialog.setAttribute('role', 'alertdialog');
    dialog.setAttribute('aria-modal', 'true');
    dialog.setAttribute('aria-labelledby', 'reset-title');
    dialog.setAttribute('aria-describedby', 'reset-desc');
    dialog.innerHTML =
      '<h2 id="reset-title" data-copy="settings.reset"></h2>' +
      '<p id="reset-desc" data-copy="settings.reset.desc"></p>' +
      '<div class="dialog-actions">' +
        '<button type="button" class="btn btn-outline" data-act="cancel" data-copy="settings.reset.cancel"></button>' +
        '<button type="button" class="btn btn-danger" data-act="confirm" data-copy="settings.reset.confirm"></button>' +
      '</div>';
    scrim.appendChild(dialog);
    doc.body.appendChild(scrim);
    site.applyCopy(scrim);

    function close() {
      doc.body.removeChild(scrim);
      doc.removeEventListener('keydown', onKey);
      if (opener) opener.focus();
    }
    function onKey(event) {
      if (event.key === 'Escape') close();
      if (event.key === 'Tab') {
        var focusable = dialog.querySelectorAll('button');
        var first = focusable[0];
        var last = focusable[focusable.length - 1];
        if (event.shiftKey && doc.activeElement === first) {
          event.preventDefault();
          last.focus();
        } else if (!event.shiftKey && doc.activeElement === last) {
          event.preventDefault();
          first.focus();
        }
      }
    }
    dialog.querySelector('[data-act="cancel"]').addEventListener('click', close);
    dialog.querySelector('[data-act="confirm"]').addEventListener('click', function () {
      site.resetAll();
      site.applyAppearance();
      close();
      site.notify('success', 'notify.reset.done');
      global.location.reload();
    });
    scrim.addEventListener('click', function (event) { if (event.target === scrim) close(); });
    doc.addEventListener('keydown', onKey);
    dialog.querySelector('[data-act="cancel"]').focus();
  }

  global.BambuControls = {
    ACCENTS: ACCENTS,
    mountThemeControls: mountThemeControls,
    availableFonts: availableFonts,
    renderSettings: renderSettings
  };
})(typeof window !== 'undefined' ? window : globalThis);
