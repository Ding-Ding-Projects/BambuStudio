/*
 * Changelog viewer.
 *
 * Shows every published release, not only the newest. The data comes from
 * changelog.data.js, which is generated from the GitHub Releases API and the
 * commits between release tags — so a version with no recorded changes says
 * exactly that instead of being padded with a plausible line.
 *
 * The date filter and the search compose: narrowing one never silently widens
 * the other, and the export writes out precisely what the filtered view shows,
 * with the active range stated in the file.
 */
(function (global) {
  'use strict';

  var site = global.BambuSite;
  var doc = global.document;
  var data = global.BAMBU_CHANGELOG || { releases: [], releaseCount: 0 };
  var views = global.BambuViews;

  var PRESETS = [
    { key: 'changelog.preset.all', days: null },
    { key: 'changelog.preset.7', days: 7 },
    { key: 'changelog.preset.30', days: 30 },
    { key: 'changelog.preset.90', days: 90 }
  ];

  /* --------------------------------------------------------- date parsing */

  function isoOf(date) {
    return date.getFullYear() + '-' +
      String(date.getMonth() + 1).padStart(2, '0') + '-' +
      String(date.getDate()).padStart(2, '0');
  }

  /** Locale field order, derived from the platform rather than assumed. */
  var localeOrder = (function () {
    try {
      var parts = new Intl.DateTimeFormat(undefined, {
        year: 'numeric', month: '2-digit', day: '2-digit'
      }).formatToParts(new Date(2000, 0, 2));
      return parts.filter(function (part) {
        return part.type === 'year' || part.type === 'month' || part.type === 'day';
      }).map(function (part) { return part.type; });
    } catch (error) {
      return ['year', 'month', 'day'];
    }
  })();

  /**
   * Accepts a plain ISO date, or the locale's own numeric format. Returns null
   * for anything else; the caller keeps whatever the user typed and says so.
   */
  function parseTypedDate(value) {
    var text = String(value || '').trim();
    if (!text) return '';
    var iso = /^(\d{4})-(\d{1,2})-(\d{1,2})$/.exec(text);
    if (iso) return validated(Number(iso[1]), Number(iso[2]), Number(iso[3]));
    var pieces = text.split(/[^\d]+/).filter(Boolean).map(Number);
    if (pieces.length !== 3) return null;
    var fields = {};
    localeOrder.forEach(function (field, index) { fields[field] = pieces[index]; });
    if (fields.year < 100) fields.year += 2000;
    return validated(fields.year, fields.month, fields.day);
  }

  function validated(year, month, day) {
    if (!year || !month || !day || month < 1 || month > 12 || day < 1 || day > 31) return null;
    var date = new Date(year, month - 1, day);
    if (date.getFullYear() !== year || date.getMonth() !== month - 1 || date.getDate() !== day) return null;
    return isoOf(date);
  }

  /* -------------------------------------------------------------- render */

  function render(panel) {
    var stateValue = { from: '', to: '', matcher: null, query: '' };

    panel.innerHTML =
      views.sectionHead('changelog.heading', 'changelog.body') +
      '<div class="changelog-controls">' +
        '<div class="changelog-search"></div>' +
        '<div class="daterange">' +
          '<div class="datefield">' +
            '<label class="rowlbl" data-copy="changelog.from"></label>' +
            '<div class="datefield-row">' +
              '<input class="input from mono" type="text" inputmode="numeric" autocomplete="off" ' +
                'data-copy-attr="placeholder:changelog.dateformat;aria-label:changelog.from">' +
              '<button type="button" class="iconbtn calendar-open" data-for="from" aria-expanded="false" ' +
                'data-copy-attr="aria-label:changelog.calendar;title:changelog.calendar">' +
                '<span data-icon aria-hidden="true">calendar_month</span></button>' +
            '</div>' +
          '</div>' +
          '<div class="datefield">' +
            '<label class="rowlbl" data-copy="changelog.to"></label>' +
            '<div class="datefield-row">' +
              '<input class="input to mono" type="text" inputmode="numeric" autocomplete="off" ' +
                'data-copy-attr="placeholder:changelog.dateformat;aria-label:changelog.to">' +
              '<button type="button" class="iconbtn calendar-open" data-for="to" aria-expanded="false" ' +
                'data-copy-attr="aria-label:changelog.calendar;title:changelog.calendar">' +
                '<span data-icon aria-hidden="true">calendar_month</span></button>' +
            '</div>' +
          '</div>' +
        '</div>' +
        '<div class="presets"></div>' +
        '<p class="date-error" role="alert" hidden></p>' +
        '<div class="calendar-host"></div>' +
        '<div class="changelog-actions">' +
          '<button type="button" class="btn btn-outline copy-view">' +
            '<span data-icon aria-hidden="true">content_copy</span>' +
            '<span data-copy="changelog.copy"></span></button>' +
          '<button type="button" class="btn btn-outline export-view">' +
            '<span data-icon aria-hidden="true">download</span>' +
            '<span data-copy="changelog.export"></span></button>' +
          '<p class="count mono" role="status"></p>' +
        '</div>' +
      '</div>' +
      '<div class="changelog-list"></div>' +
      '<p class="hint" data-copy="changelog.derivation"></p>';

    var list = panel.querySelector('.changelog-list');
    var countLine = panel.querySelector('.count');
    var errorLine = panel.querySelector('.date-error');
    var fromInput = panel.querySelector('.input.from');
    var toInput = panel.querySelector('.input.to');
    var calendarHost = panel.querySelector('.calendar-host');

    var searchField = global.BambuRegex.createSearchField({
      labelKey: 'changelog.search',
      sampleProvider: function () { return data.releases.map(searchTextFor).join('\n'); },
      onChange: function (matcher, query) {
        stateValue.matcher = matcher;
        stateValue.query = query;
        paint();
      }
    });
    panel.querySelector('.changelog-search').appendChild(searchField.element);

    var presetHost = panel.querySelector('.presets');
    PRESETS.forEach(function (preset) {
      var button = doc.createElement('button');
      button.type = 'button';
      button.className = 'chip-btn';
      button.setAttribute('data-copy', preset.key);
      button.addEventListener('click', function () {
        if (preset.days === null) {
          stateValue.from = '';
          stateValue.to = '';
        } else {
          var now = new Date();
          var start = new Date(now.getTime() - preset.days * 86400000);
          stateValue.from = isoOf(start);
          stateValue.to = isoOf(now);
        }
        fromInput.value = stateValue.from;
        toInput.value = stateValue.to;
        errorLine.hidden = true;
        paint();
      });
      presetHost.appendChild(button);
    });

    [[fromInput, 'from'], [toInput, 'to']].forEach(function (entry) {
      entry[0].addEventListener('input', function () {
        var parsed = parseTypedDate(entry[0].value);
        if (parsed === null) {
          // Keep what the user typed; report inline and leave the filter alone.
          errorLine.textContent = site.text('changelog.dateinvalid');
          errorLine.hidden = false;
          return;
        }
        errorLine.hidden = true;
        stateValue[entry[1]] = parsed;
        paint();
      });
    });

    panel.querySelectorAll('.calendar-open').forEach(function (button) {
      button.addEventListener('click', function () {
        var open = calendarHost.dataset.open !== button.dataset.for;
        calendarHost.innerHTML = '';
        panel.querySelectorAll('.calendar-open').forEach(function (other) {
          other.setAttribute('aria-expanded', 'false');
        });
        if (!open) {
          calendarHost.dataset.open = '';
          return;
        }
        calendarHost.dataset.open = button.dataset.for;
        button.setAttribute('aria-expanded', 'true');
        buildCalendar(calendarHost, button.dataset.for);
      });
    });

    function buildCalendar(host, field) {
      var anchor = stateValue[field] ? new Date(stateValue[field]) : new Date();
      var cursor = { year: anchor.getFullYear(), month: anchor.getMonth() };
      var calendar = doc.createElement('div');
      calendar.className = 'calendar';
      calendar.setAttribute('role', 'group');
      host.appendChild(calendar);

      function paintCalendar() {
        var first = new Date(cursor.year, cursor.month, 1);
        var startWeekday = first.getDay();
        var days = new Date(cursor.year, cursor.month + 1, 0).getDate();
        var years = [];
        for (var year = 2024; year <= new Date().getFullYear() + 1; year++) years.push(year);
        calendar.innerHTML =
          '<div class="calendar-head">' +
            '<button type="button" class="iconbtn cal-prev" aria-label="Previous month">' +
              '<span data-icon aria-hidden="true">chevron_left</span></button>' +
            '<select class="select cal-month" aria-label="Month">' +
              Array.from({ length: 12 }, function (unused, index) {
                return '<option value="' + index + '"' + (index === cursor.month ? ' selected' : '') + '>' +
                  new Date(2000, index, 1).toLocaleDateString(undefined, { month: 'long' }) + '</option>';
              }).join('') +
            '</select>' +
            '<select class="select cal-year" aria-label="Year">' +
              years.map(function (year) {
                return '<option value="' + year + '"' + (year === cursor.year ? ' selected' : '') + '>' + year + '</option>';
              }).join('') +
            '</select>' +
            '<button type="button" class="iconbtn cal-next" aria-label="Next month">' +
              '<span data-icon aria-hidden="true">chevron_right</span></button>' +
          '</div>' +
          '<div class="calendar-grid">' +
            Array.from({ length: startWeekday }, function () { return '<span class="cal-blank"></span>'; }).join('') +
            Array.from({ length: days }, function (unused, index) {
              var day = index + 1;
              var iso = isoOf(new Date(cursor.year, cursor.month, day));
              var inRange = stateValue.from && stateValue.to && iso >= stateValue.from && iso <= stateValue.to;
              var isEdge = iso === stateValue.from || iso === stateValue.to;
              return '<button type="button" class="cal-day' + (inRange ? ' inrange' : '') +
                (isEdge ? ' edge' : '') + '" data-iso="' + iso + '">' + day + '</button>';
            }).join('') +
          '</div>';
        calendar.querySelector('.cal-prev').addEventListener('click', function () {
          cursor.month--;
          if (cursor.month < 0) { cursor.month = 11; cursor.year--; }
          paintCalendar();
        });
        calendar.querySelector('.cal-next').addEventListener('click', function () {
          cursor.month++;
          if (cursor.month > 11) { cursor.month = 0; cursor.year++; }
          paintCalendar();
        });
        calendar.querySelector('.cal-month').addEventListener('change', function (event) {
          cursor.month = Number(event.target.value);
          paintCalendar();
        });
        calendar.querySelector('.cal-year').addEventListener('change', function (event) {
          cursor.year = Number(event.target.value);
          paintCalendar();
        });
        calendar.querySelectorAll('.cal-day').forEach(function (day) {
          day.addEventListener('click', function () {
            var iso = day.dataset.iso;
            // Clicking always sets the field the calendar was opened for, and a
            // start later than the end swaps them rather than emptying the view.
            stateValue[field] = iso;
            if (stateValue.from && stateValue.to && stateValue.from > stateValue.to) {
              var swap = stateValue.from;
              stateValue.from = stateValue.to;
              stateValue.to = swap;
            }
            fromInput.value = stateValue.from;
            toInput.value = stateValue.to;
            errorLine.hidden = true;
            paintCalendar();
            paint();
          });
        });
      }
      paintCalendar();
    }

    panel.querySelector('.copy-view').addEventListener('click', function () {
      site.copyText(markdown(visible()), list);
    });
    panel.querySelector('.export-view').addEventListener('click', function () {
      var shown = visible();
      site.downloadText(exportName(), markdown(shown));
      site.notify('success', 'notify.exported', { count: shown.length });
    });

    function exportName() {
      var from = stateValue.from || 'start';
      var to = stateValue.to || 'latest';
      return 'bambu-studio-changelog-' + from + '_to_' + to + '.md';
    }

    function searchTextFor(release) {
      return [
        release.tag, release.name, release.version, release.dish.en, release.dish.yue,
        release.qualifier, release.commit, formatDate(release.published),
        (release.changes || []).map(function (change) { return change.subject + ' ' + change.short; }).join(' ')
      ].join(' ');
    }

    function visible() {
      var matcher = stateValue.matcher;
      return data.releases.filter(function (release) {
        var day = formatDate(release.published);
        if (stateValue.from && day < stateValue.from) return false;
        if (stateValue.to && day > stateValue.to) return false;
        if (!matcher || matcher.empty) return true;
        return matcher.test(searchTextFor(release));
      });
    }

    function paint() {
      var shown = visible();
      countLine.textContent = site.text('changelog.count', { shown: shown.length, total: data.releases.length });
      if (!shown.length) {
        list.innerHTML = '<p class="empty" data-copy="changelog.empty"></p>';
        site.applyCopy(list);
        return;
      }
      list.innerHTML = shown.map(entryHtml).join('');
      site.applyCopy(list);
    }

    function entryHtml(release) {
      var dish = release.dish.en
        ? '<span class="release-dish">' + views.escapeHtml(release.dish.en) +
          ' · <span lang="yue-Hant-HK">' + views.escapeHtml(release.dish.yue) + '</span></span>'
        : '';
      var changes = (release.changes || []).length
        ? '<ul class="change-list">' + release.changes.map(function (change) {
            return '<li class="change">' +
              '<span class="cat cat-' + change.category + '" data-copy="changelog.cat.' + change.category + '"></span>' +
              '<span class="change-text">' + views.escapeHtml(change.subject) + '</span>' +
              '<a class="change-sha mono" href="' + views.REPO + '/commit/' + change.sha + '" ' +
                'target="_blank" rel="noopener">' + change.short + '</a>' +
              '</li>';
          }).join('') + '</ul>'
        : '<p class="empty small" data-copy="' + (release.baseline ? 'changelog.baseline' : 'changelog.nochanges') + '"></p>';
      return '<article class="release-entry">' +
        '<header class="release-entry-head">' +
          '<h3 class="release-version mono">' + views.escapeHtml(release.version) + '</h3>' +
          '<time class="release-date mono" datetime="' + release.published + '">' +
            formatDate(release.published) + '</time>' +
          dish +
          (release.qualifier ? '<span class="release-qualifier">' + views.escapeHtml(release.qualifier) + '</span>' : '') +
        '</header>' +
        changes +
        '<footer class="release-entry-foot">' +
          '<a class="mono" href="' + release.url + '" target="_blank" rel="noopener">' + views.escapeHtml(release.tag) + '</a>' +
          (release.commit
            ? ' · <a class="mono" href="' + views.REPO + '/commit/' + release.commit + '" target="_blank" rel="noopener">' +
              release.commit.slice(0, 9) + '</a>'
            : '') +
          (release.assets && release.assets.length
            ? ' · <span class="mono">' + release.assets.length + ' file(s)</span>'
            : '') +
        '</footer>' +
      '</article>';
    }

    function markdown(releases) {
      var lines = [
        '# Bambu Studio MD3 — changelog',
        '',
        '- Source: ' + data.repository + ' GitHub Releases',
        '- Range exported: ' + (stateValue.from || 'first release') + ' to ' + (stateValue.to || 'latest release'),
        '- Search: ' + (stateValue.query ? '`' + stateValue.query + '`' : 'none'),
        '- Releases in this export: ' + releases.length + ' of ' + data.releases.length,
        '- Change categories are derived from each commit subject’s leading verb.',
        ''
      ];
      releases.forEach(function (release) {
        lines.push('## ' + release.name);
        lines.push('');
        lines.push('- Tag: `' + release.tag + '`');
        lines.push('- Published: ' + formatDate(release.published));
        if (release.commit) lines.push('- Commit: `' + release.commit + '`');
        if (release.workflow) lines.push('- CI run: ' + release.workflow);
        lines.push('');
        if ((release.changes || []).length) {
          release.changes.forEach(function (change) {
            lines.push('- **' + site.pair('changelog.cat.' + change.category).en + '** ' +
              change.subject + ' (`' + change.short + '`)');
          });
        } else {
          lines.push('- ' + site.pair(release.baseline ? 'changelog.baseline' : 'changelog.nochanges').en);
        }
        lines.push('');
      });
      return lines.join('\n');
    }

    function formatDate(iso) {
      return String(iso || '').slice(0, 10);
    }

    site.applyCopy(panel);
    stateValue.matcher = searchField.matcher();
    paint();
  }

  global.BambuChangelog = {
    render: render,
    parseTypedDate: parseTypedDate,
    localeOrder: localeOrder
  };
})(typeof window !== 'undefined' ? window : globalThis);
