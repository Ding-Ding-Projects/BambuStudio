/*
 * The shared regex builder.
 *
 * One implementation serves three roles:
 *   - the "Regex lab" tab, mounted full width with its own sample text;
 *   - the disclosure panel under every search bar on the site;
 *   - the matcher those search bars actually filter with.
 *
 * Engine and dialect: the browser's own ECMAScript `RegExp`. Flags offered are
 * exactly the ones this engine accepts (g i m s u y). Literal text inserted
 * through the guided controls is escaped with `escapeLiteral` below, so a typed
 * "." means a full stop rather than "any character".
 *
 * Denial-of-service posture: a pattern the user writes can backtrack
 * catastrophically, and a regex engine cannot be interrupted mid-`exec`. Two
 * bounds apply. Evaluation prefers a Web Worker that is terminated after a hard
 * timeout, so a runaway pattern costs a discarded worker rather than a frozen
 * page. Where a worker cannot be created the site falls back to inline matching
 * against a smaller sample with a between-items time budget, and says which
 * path ran. Pattern and sample lengths are capped in both paths.
 */
(function (global) {
  'use strict';

  var site = global.BambuSite;
  var FLAGS = ['g', 'i', 'm', 's', 'u', 'y'];
  var MAX_PATTERN = 512;
  var MAX_SAMPLE = 20000;
  var MAX_SAMPLE_INLINE = 4000;
  var MAX_MATCHES = 500;
  var TIMEOUT_MS = 400;

  function escapeLiteral(value) {
    return String(value).replace(/[\\^$.|?*+()[\]{}/]/g, '\\$&');
  }

  function escapeHtml(value) {
    return String(value).replace(/[&<>"']/g, function (character) {
      return { '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[character];
    });
  }

  /* ------------------------------------------------------------- worker */

  var WORKER_SOURCE = [
    'self.onmessage = function (event) {',
    '  var data = event.data;',
    '  try {',
    '    var flags = data.flags.indexOf("g") === -1 ? data.flags + "g" : data.flags;',
    '    var regex = new RegExp(data.pattern, flags);',
    '    var matches = [];',
    '    var match;',
    '    var guard = 0;',
    '    while ((match = regex.exec(data.sample)) !== null) {',
    '      matches.push({',
    '        index: match.index,',
    '        text: match[0],',
    '        groups: Array.prototype.slice.call(match, 1),',
    '        named: match.groups ? Object.assign({}, match.groups) : null',
    '      });',
    '      if (match[0] === "") regex.lastIndex++;',
    '      if (++guard >= ' + MAX_MATCHES + ') break;',
    '    }',
    '    self.postMessage({ ok: true, matches: matches, truncated: guard >= ' + MAX_MATCHES + ' });',
    '  } catch (error) {',
    '    self.postMessage({ ok: false, message: String(error && error.message || error) });',
    '  }',
    '};'
  ].join('\n');

  var workerUrl = null;
  var workerUnavailable = false;
  function workerFactory() {
    if (workerUnavailable || typeof global.Worker !== 'function') return null;
    try {
      if (!workerUrl) {
        workerUrl = URL.createObjectURL(new Blob([WORKER_SOURCE], { type: 'text/javascript' }));
      }
      return new global.Worker(workerUrl);
    } catch (error) {
      workerUnavailable = true;
      return null;
    }
  }

  /**
   * Evaluates a pattern against a sample and always resolves — never rejects —
   * with an explicit status the caller can render verbatim.
   */
  function evaluate(pattern, flags, sample) {
    var trimmedPattern = String(pattern || '').slice(0, MAX_PATTERN);
    var patternTruncated = String(pattern || '').length > MAX_PATTERN;
    var worker = workerFactory();
    var limit = worker ? MAX_SAMPLE : MAX_SAMPLE_INLINE;
    var text = String(sample == null ? '' : sample).slice(0, limit);
    var sampleTruncated = String(sample == null ? '' : sample).length > limit;
    var started = (global.performance || Date).now();

    if (!trimmedPattern) {
      if (worker) worker.terminate();
      return Promise.resolve({
        status: 'empty', matches: [], engine: worker ? 'worker' : 'inline',
        sampleTruncated: sampleTruncated, patternTruncated: patternTruncated, limit: limit
      });
    }

    if (worker) {
      return new Promise(function (resolve) {
        var settled = false;
        var timer = setTimeout(function () {
          if (settled) return;
          settled = true;
          worker.terminate();
          resolve({
            status: 'timeout', matches: [], timeout: TIMEOUT_MS, engine: 'worker',
            sampleTruncated: sampleTruncated, patternTruncated: patternTruncated, limit: limit
          });
        }, TIMEOUT_MS);
        worker.onmessage = function (event) {
          if (settled) return;
          settled = true;
          clearTimeout(timer);
          worker.terminate();
          var data = event.data;
          resolve(data.ok ? {
            status: data.matches.length ? 'ok' : 'nomatch',
            matches: data.matches, truncated: data.truncated, engine: 'worker',
            elapsed: Math.round((global.performance || Date).now() - started),
            sampleTruncated: sampleTruncated, patternTruncated: patternTruncated, limit: limit
          } : {
            status: 'invalid', message: data.message, matches: [], engine: 'worker',
            sampleTruncated: sampleTruncated, patternTruncated: patternTruncated, limit: limit
          });
        };
        worker.onerror = function (error) {
          if (settled) return;
          settled = true;
          clearTimeout(timer);
          worker.terminate();
          resolve({
            status: 'invalid', message: String(error && error.message || 'worker error'),
            matches: [], engine: 'worker', sampleTruncated: sampleTruncated,
            patternTruncated: patternTruncated, limit: limit
          });
        };
        worker.postMessage({ pattern: trimmedPattern, flags: flags || '', sample: text });
      });
    }

    // Inline fallback: a smaller sample, and the loop stops at the same budget.
    return Promise.resolve((function () {
      var regex;
      try {
        regex = new RegExp(trimmedPattern, flags.indexOf('g') === -1 ? flags + 'g' : flags);
      } catch (error) {
        return {
          status: 'invalid', message: String(error && error.message || error), matches: [],
          engine: 'inline', sampleTruncated: sampleTruncated, patternTruncated: patternTruncated, limit: limit
        };
      }
      var matches = [];
      var match;
      var deadline = (global.performance || Date).now() + TIMEOUT_MS;
      while ((match = regex.exec(text)) !== null) {
        matches.push({
          index: match.index,
          text: match[0],
          groups: Array.prototype.slice.call(match, 1),
          named: match.groups ? Object.assign({}, match.groups) : null
        });
        if (match[0] === '') regex.lastIndex++;
        if (matches.length >= MAX_MATCHES) break;
        if ((global.performance || Date).now() > deadline) {
          return {
            status: 'timeout', matches: matches, timeout: TIMEOUT_MS, engine: 'inline',
            sampleTruncated: sampleTruncated, patternTruncated: patternTruncated, limit: limit
          };
        }
      }
      return {
        status: matches.length ? 'ok' : 'nomatch', matches: matches,
        truncated: matches.length >= MAX_MATCHES, engine: 'inline',
        elapsed: Math.round((global.performance || Date).now() - started),
        sampleTruncated: sampleTruncated, patternTruncated: patternTruncated, limit: limit
      };
    })());
  }

  /**
   * The matcher search bars filter with. Plain text is the default everywhere;
   * regex only applies when the caller says the user opted in.
   */
  function createMatcher(query, options) {
    var settings = options || {};
    if (!query) return { ok: true, test: function () { return true; }, empty: true };
    if (!settings.regex) {
      var needle = String(query).toLowerCase();
      return {
        ok: true,
        empty: false,
        test: function (value) { return String(value == null ? '' : value).toLowerCase().indexOf(needle) !== -1; }
      };
    }
    var regex;
    try {
      regex = new RegExp(String(query).slice(0, MAX_PATTERN), settings.flags || 'i');
    } catch (error) {
      return {
        ok: false,
        empty: false,
        message: String(error && error.message || error),
        test: function () { return true; }
      };
    }
    // Between-items budget: a runaway pattern stops the filter instead of the tab.
    var deadline = (global.performance || Date).now() + TIMEOUT_MS;
    var expired = false;
    return {
      ok: true,
      empty: false,
      expired: function () { return expired; },
      test: function (value) {
        if (expired) return true;
        if ((global.performance || Date).now() > deadline) {
          expired = true;
          return true;
        }
        regex.lastIndex = 0;
        return regex.test(String(value == null ? '' : value).slice(0, MAX_SAMPLE_INLINE));
      }
    };
  }

  /* ------------------------------------------------------------ builder */

  var CLASS_PRESETS = [
    { label: 'Digit', insert: '\\d' },
    { label: 'Not digit', insert: '\\D' },
    { label: 'Word', insert: '\\w' },
    { label: 'Whitespace', insert: '\\s' },
    { label: 'Any character', insert: '.' },
    { label: 'a–z', insert: '[a-z]' },
    { label: 'A–Z', insert: '[A-Z]' },
    { label: 'Han', insert: '\\p{Script=Han}' }
  ];
  var ANCHORS = [
    { label: 'Start ^', insert: '^' },
    { label: 'End $', insert: '$' },
    { label: 'Word boundary \\b', insert: '\\b' },
    { label: 'Not a boundary \\B', insert: '\\B' }
  ];
  var QUANTIFIERS = [
    { label: '0 or more *', insert: '*' },
    { label: '1 or more +', insert: '+' },
    { label: 'Optional ?', insert: '?' },
    { label: 'Exactly {n}', insert: '{2}' },
    { label: 'At least {n,}', insert: '{2,}' },
    { label: 'Between {n,m}', insert: '{2,4}' },
    { label: 'Lazy +?', insert: '+?' }
  ];

  var builderSeq = 0;

  /**
   * Mounts the builder into `host`.
   * options: { pattern, flags, sample, sampleLabel, onApply(pattern, flags),
   *            onChange(pattern, flags), compact }
   */
  function mountBuilder(host, options) {
    var settings = options || {};
    var id = 'rb' + (++builderSeq);
    var stateValue = {
      pattern: settings.pattern || '',
      flags: settings.flags || 'gi',
      sample: settings.sample || ''
    };

    host.classList.add('regex-builder');
    if (settings.compact) host.classList.add('compact');
    host.innerHTML =
      '<div class="rb-engine" data-copy="regex.engine"></div>' +
      '<div class="rb-field">' +
        '<label class="rb-label" for="' + id + '-pattern" data-copy="regex.pattern"></label>' +
        '<input id="' + id + '-pattern" class="rb-input mono" type="text" spellcheck="false" ' +
          'autocomplete="off" maxlength="' + MAX_PATTERN + '">' +
      '</div>' +
      '<div class="rb-field">' +
        '<span class="rb-label" data-copy="regex.flags"></span>' +
        '<div class="rb-flags" role="group"></div>' +
      '</div>' +
      '<div class="rb-parts"></div>' +
      '<div class="rb-field">' +
        '<label class="rb-label" for="' + id + '-sample" data-copy="regex.sample"></label>' +
        '<textarea id="' + id + '-sample" class="rb-sample mono" rows="4" spellcheck="false"></textarea>' +
      '</div>' +
      '<p class="rb-status" role="status"></p>' +
      '<div class="rb-results"></div>' +
      '<div class="rb-actions"></div>';

    var patternInput = host.querySelector('.rb-input');
    var sampleInput = host.querySelector('.rb-sample');
    var flagHost = host.querySelector('.rb-flags');
    var partHost = host.querySelector('.rb-parts');
    var statusLine = host.querySelector('.rb-status');
    var resultHost = host.querySelector('.rb-results');
    var actionHost = host.querySelector('.rb-actions');

    patternInput.value = stateValue.pattern;
    sampleInput.value = stateValue.sample;

    FLAGS.forEach(function (flag) {
      var label = global.document.createElement('label');
      label.className = 'rb-flag';
      label.innerHTML =
        '<input type="checkbox" value="' + flag + '"' +
        (stateValue.flags.indexOf(flag) !== -1 ? ' checked' : '') + '>' +
        '<span class="mono">' + flag + '</span>';
      label.querySelector('input').addEventListener('change', function (event) {
        var next = stateValue.flags.replace(flag, '');
        stateValue.flags = event.target.checked ? next + flag : next;
        changed();
      });
      flagHost.appendChild(label);
    });

    function partGroup(titleKey, controls) {
      var group = global.document.createElement('div');
      group.className = 'rb-part';
      var heading = global.document.createElement('p');
      heading.className = 'rb-part-title';
      heading.setAttribute('data-copy', titleKey);
      group.appendChild(heading);
      var row = global.document.createElement('div');
      row.className = 'rb-part-row';
      controls.forEach(function (control) { row.appendChild(control); });
      group.appendChild(row);
      partHost.appendChild(group);
      return group;
    }

    function chip(label, onClick) {
      var button = global.document.createElement('button');
      button.type = 'button';
      button.className = 'rb-chip mono';
      button.textContent = label;
      button.addEventListener('click', onClick);
      return button;
    }

    function insert(fragment) {
      var start = patternInput.selectionStart == null ? patternInput.value.length : patternInput.selectionStart;
      var end = patternInput.selectionEnd == null ? start : patternInput.selectionEnd;
      patternInput.value = patternInput.value.slice(0, start) + fragment + patternInput.value.slice(end);
      stateValue.pattern = patternInput.value;
      patternInput.focus();
      patternInput.selectionStart = patternInput.selectionEnd = start + fragment.length;
      changed();
    }

    function wrapSelection(before, after) {
      var start = patternInput.selectionStart == null ? 0 : patternInput.selectionStart;
      var end = patternInput.selectionEnd == null ? start : patternInput.selectionEnd;
      var selected = patternInput.value.slice(start, end);
      patternInput.value = patternInput.value.slice(0, start) + before + selected + after + patternInput.value.slice(end);
      stateValue.pattern = patternInput.value;
      patternInput.focus();
      patternInput.selectionStart = start + before.length;
      patternInput.selectionEnd = start + before.length + selected.length;
      changed();
    }

    // Literal text — escaped on insert, which is the whole point of the control.
    var literalInput = global.document.createElement('input');
    literalInput.type = 'text';
    literalInput.className = 'rb-input rb-literal';
    literalInput.setAttribute('data-copy-attr', 'aria-label:regex.part.literal');
    var literalButton = global.document.createElement('button');
    literalButton.type = 'button';
    literalButton.className = 'btn btn-tonal rb-insert';
    literalButton.setAttribute('data-copy', 'regex.insert');
    literalButton.addEventListener('click', function () {
      if (!literalInput.value) return;
      insert(escapeLiteral(literalInput.value));
      literalInput.value = '';
    });
    var literalGroup = partGroup('regex.part.literal', [literalInput, literalButton]);
    var literalNote = global.document.createElement('p');
    literalNote.className = 'rb-note';
    literalNote.setAttribute('data-copy', 'regex.escapenote');
    literalGroup.appendChild(literalNote);

    partGroup('regex.part.class', CLASS_PRESETS.map(function (preset) {
      return chip(preset.insert, function () { insert(preset.insert); });
    }));
    partGroup('regex.part.anchor', ANCHORS.map(function (anchor) {
      return chip(anchor.insert, function () { insert(anchor.insert); });
    }));
    partGroup('regex.part.group', [
      chip('( … )', function () { wrapSelection('(', ')'); }),
      chip('(?: … )', function () { wrapSelection('(?:', ')'); }),
      chip('(?<name> … )', function () { wrapSelection('(?<name>', ')'); }),
      chip('(?= … )', function () { wrapSelection('(?=', ')'); }),
      chip('(?! … )', function () { wrapSelection('(?!', ')'); })
    ]);
    partGroup('regex.part.alternation', [
      chip('a|b', function () { insert('|'); }),
      chip('(?:a|b)', function () { wrapSelection('(?:', '|)'); })
    ]);
    partGroup('regex.part.quantifier', QUANTIFIERS.map(function (quantifier) {
      return chip(quantifier.insert, function () { insert(quantifier.insert); });
    }));

    function action(copyKey, className, onClick) {
      var button = global.document.createElement('button');
      button.type = 'button';
      button.className = 'btn ' + className;
      button.setAttribute('data-copy', copyKey);
      button.addEventListener('click', onClick);
      actionHost.appendChild(button);
      return button;
    }

    if (settings.onApply) {
      action('regex.apply', 'btn-filled', function () {
        settings.onApply(stateValue.pattern, stateValue.flags);
      });
    }
    action('regex.copy', 'btn-outline', function () {
      site.copyText(stateValue.pattern, patternInput);
    });
    action('regex.export', 'btn-outline', function () {
      site.downloadText('bambu-regex-pattern.md', [
        '# Regex pattern',
        '',
        '- Engine: ECMAScript RegExp (browser)',
        '- Pattern: `' + stateValue.pattern + '`',
        '- Flags: `' + stateValue.flags + '`',
        '- Exported from: ' + global.location.href,
        '',
        '## Sample text',
        '',
        '```',
        stateValue.sample,
        '```',
        ''
      ].join('\n'));
      site.notify('success', 'notify.exported', { count: 1 });
    });
    action('regex.clear', 'btn-outline', function () {
      patternInput.value = '';
      stateValue.pattern = '';
      changed();
    });

    patternInput.addEventListener('input', function () {
      stateValue.pattern = patternInput.value;
      changed();
    });
    sampleInput.addEventListener('input', function () {
      stateValue.sample = sampleInput.value;
      changed();
    });

    var pending = null;
    function changed() {
      if (settings.onChange) settings.onChange(stateValue.pattern, stateValue.flags);
      if (pending) clearTimeout(pending);
      pending = setTimeout(run, 120);
    }

    function run() {
      evaluate(stateValue.pattern, stateValue.flags, stateValue.sample).then(render);
    }

    function render(result) {
      statusLine.className = 'rb-status status-' + result.status;
      if (result.status === 'invalid') {
        statusLine.textContent = site.text('regex.invalid', { message: result.message });
        resultHost.innerHTML = '';
        return;
      }
      if (result.status === 'timeout') {
        statusLine.textContent = site.text('regex.timeout', { ms: result.timeout });
        resultHost.innerHTML = '';
        return;
      }
      if (result.status === 'empty') {
        statusLine.textContent = '';
        resultHost.innerHTML = '';
        return;
      }
      if (result.status === 'nomatch') {
        statusLine.textContent = site.text('regex.nomatch');
        resultHost.innerHTML = '';
        return;
      }
      statusLine.textContent = site.text('regex.matches') + ': ' + result.matches.length +
        (result.truncated ? '+' : '') + ' · ' + result.elapsed + ' ms';
      if (result.sampleTruncated) {
        statusLine.textContent += ' · ' + site.text('regex.toolong', { limit: result.limit });
      }
      resultHost.innerHTML = result.matches.slice(0, 40).map(function (match) {
        var groups = match.groups.filter(function (group) { return group !== undefined; });
        var named = match.named ? Object.keys(match.named) : [];
        return '<div class="rb-match">' +
          '<code class="rb-match-text">' + escapeHtml(match.text) + '</code>' +
          '<span class="rb-match-index mono">@' + match.index + '</span>' +
          (groups.length
            ? '<span class="rb-match-groups">' + site.text('regex.groups') + ': ' +
              groups.map(function (group, index) {
                var name = named[index] ? escapeHtml(named[index]) + '=' : (index + 1) + ': ';
                return '<code>' + name + escapeHtml(String(group)) + '</code>';
              }).join(' ') + '</span>'
            : '') +
          '</div>';
      }).join('');
    }

    site.applyCopy(host);
    run();

    return {
      element: host,
      setPattern: function (pattern, flags) {
        stateValue.pattern = pattern;
        patternInput.value = pattern;
        if (flags !== undefined) stateValue.flags = flags;
        run();
      },
      setSample: function (sample) {
        stateValue.sample = sample;
        sampleInput.value = sample;
        run();
      },
      state: function () { return { pattern: stateValue.pattern, flags: stateValue.flags }; },
      refresh: function () { site.applyCopy(host); run(); }
    };
  }

  /* -------------------------------------------------------- search field */

  var fieldSeq = 0;

  /**
   * A search bar with plain text as the default, an explicit regex opt-in, and
   * the full builder one button away. Query, pattern, flags, validation and
   * mode stay synchronised in both directions: typing in the field updates the
   * builder's pattern, and applying a pattern from the builder updates the
   * field and switches it to regex mode.
   *
   * options: { labelKey, onChange(matcher, rawQuery), sampleProvider() }
   */
  function createSearchField(options) {
    var settings = options || {};
    var id = 'sf' + (++fieldSeq);
    var element = global.document.createElement('div');
    element.className = 'searchfield';
    element.innerHTML =
      '<div class="sf-row">' +
        '<span class="sf-icon" data-icon aria-hidden="true">search</span>' +
        // aria-label rather than a visually-hidden <label>: a 1px label element
        // is content wider than its box, which the clipping gate rightly flags.
        '<input id="' + id + '" class="sf-input" type="search" autocomplete="off" spellcheck="false" ' +
          'data-copy-attr="placeholder:' + (settings.labelKey || 'settings.search') +
          ';aria-label:' + (settings.labelKey || 'settings.search') + '">' +
        '<button type="button" class="sf-toggle mono" aria-pressed="false" ' +
          'data-copy-attr="aria-label:regex.mode.regex;title:regex.mode.regex">.*</button>' +
        '<button type="button" class="sf-builder iconbtn" aria-expanded="false" ' +
          'data-copy-attr="aria-label:regex.open;title:regex.open">' +
          '<span data-icon aria-hidden="true">tune</span></button>' +
      '</div>' +
      '<p class="sf-status" role="status"></p>' +
      '<div class="sf-panel" hidden></div>';

    var input = element.querySelector('.sf-input');
    var toggle = element.querySelector('.sf-toggle');
    var builderButton = element.querySelector('.sf-builder');
    var statusLine = element.querySelector('.sf-status');
    var panel = element.querySelector('.sf-panel');
    var builder = null;
    var regexMode = false;

    function emitChange() {
      var matcher = createMatcher(input.value, { regex: regexMode, flags: 'i' });
      statusLine.textContent = matcher.ok
        ? ''
        : site.text('regex.invalid', { message: matcher.message });
      statusLine.className = 'sf-status' + (matcher.ok ? '' : ' status-invalid');
      if (settings.onChange) settings.onChange(matcher, input.value, regexMode);
      if (builder) builder.setPattern(input.value);
    }

    input.addEventListener('input', emitChange);
    toggle.addEventListener('click', function () {
      regexMode = !regexMode;
      toggle.setAttribute('aria-pressed', String(regexMode));
      toggle.classList.toggle('active', regexMode);
      emitChange();
    });
    builderButton.addEventListener('click', function () {
      var open = panel.hasAttribute('hidden');
      if (open) {
        panel.removeAttribute('hidden');
        if (!builder) {
          builder = mountBuilder(panel, {
            pattern: input.value,
            flags: 'gi',
            compact: true,
            sample: settings.sampleProvider ? settings.sampleProvider() : '',
            onApply: function (pattern) {
              input.value = pattern;
              if (!regexMode) {
                regexMode = true;
                toggle.setAttribute('aria-pressed', 'true');
                toggle.classList.add('active');
              }
              emitChange();
            }
          });
        } else if (settings.sampleProvider) {
          builder.setSample(settings.sampleProvider());
        }
      } else {
        panel.setAttribute('hidden', '');
      }
      builderButton.setAttribute('aria-expanded', String(open));
    });

    site.applyCopy(element);

    return {
      element: element,
      value: function () { return input.value; },
      isRegex: function () { return regexMode; },
      matcher: function () { return createMatcher(input.value, { regex: regexMode, flags: 'i' }); },
      refresh: function () {
        site.applyCopy(element);
        if (builder) builder.refresh();
      }
    };
  }

  global.BambuRegex = {
    FLAGS: FLAGS,
    MAX_PATTERN: MAX_PATTERN,
    MAX_SAMPLE: MAX_SAMPLE,
    TIMEOUT_MS: TIMEOUT_MS,
    escapeLiteral: escapeLiteral,
    evaluate: evaluate,
    createMatcher: createMatcher,
    mountBuilder: mountBuilder,
    createSearchField: createSearchField
  };
})(typeof window !== 'undefined' ? window : globalThis);
