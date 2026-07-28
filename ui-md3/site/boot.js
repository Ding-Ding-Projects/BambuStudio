/*
 * Site bootstrap: header chrome, tab definitions, notification centre and the
 * dim sum surprise.
 *
 * Load order matters and is fixed in landing.html: i18n resources, i18n, copy,
 * changelog data, dim sum data, core, regex, settings, views, changelog view,
 * tabs, then this file.
 */
(function (global) {
  'use strict';

  var site = global.BambuSite;
  var doc = global.document;

  var TABS = [
    { id: 'overview', copy: 'tab.overview', icon: 'home', group: 'group.product', render: global.BambuViews.renderOverview },
    { id: 'screens', copy: 'tab.screens', icon: 'grid_view', group: 'group.product', render: global.BambuViews.renderScreens },
    { id: 'materialyou', copy: 'tab.materialyou', icon: 'palette', group: 'group.product', render: global.BambuViews.renderMaterialYou },
    { id: 'download', copy: 'tab.download', icon: 'download', group: 'group.product', render: global.BambuViews.renderDownload },
    { id: 'changelog', copy: 'tab.changelog', icon: 'history', group: 'group.project', render: global.BambuChangelog.render },
    { id: 'regex', copy: 'tab.regex', icon: 'regular_expression', group: 'group.tools', render: renderRegexLab },
    { id: 'settings', copy: 'tab.settings', icon: 'settings', group: 'group.tools', render: global.BambuControls.renderSettings },
    { id: 'build', copy: 'tab.build', icon: 'construction', group: 'group.project', render: global.BambuViews.renderBuild }
  ];

  function renderRegexLab(panel) {
    panel.innerHTML = global.BambuViews.sectionHead('regex.heading', 'regex.body') +
      '<div class="regex-lab"></div>';
    var changelog = global.BAMBU_CHANGELOG || { releases: [] };
    var sample = changelog.releases.slice(0, 12).map(function (release) {
      return release.tag + '  ' + release.name + '  ' + String(release.published).slice(0, 10);
    }).join('\n');
    global.BambuRegex.mountBuilder(panel.querySelector('.regex-lab'), {
      pattern: 'md3-v(\\d+)',
      flags: 'gi',
      sample: sample
    });
  }

  /* ------------------------------------------------------------- header */

  function buildHeader() {
    var select = doc.getElementById('languageMode');
    select.value = site.languageMode();
    select.addEventListener('change', function () {
      site.setLanguageMode(select.value);
      refreshAll();
    });

    var themeToggle = doc.getElementById('themeToggle');
    function paintTheme() {
      var dark = site.get('theme') === 'dark';
      doc.getElementById('themeIcon').textContent = dark ? 'light_mode' : 'dark_mode';
    }
    themeToggle.addEventListener('click', function () {
      site.set('theme', site.get('theme') === 'dark' ? 'light' : 'dark');
      site.applyAppearance();
      paintTheme();
    });
    paintTheme();

    var bell = doc.getElementById('notifyButton');
    var centre = doc.getElementById('notifyCentre');
    bell.addEventListener('click', function () {
      var open = centre.hasAttribute('hidden');
      if (open) {
        centre.removeAttribute('hidden');
        paintCentre();
      } else {
        centre.setAttribute('hidden', '');
      }
      bell.setAttribute('aria-expanded', String(open));
    });
    doc.addEventListener('click', function (event) {
      if (!centre.contains(event.target) && event.target !== bell && !bell.contains(event.target)) {
        centre.setAttribute('hidden', '');
        bell.setAttribute('aria-expanded', 'false');
      }
    });

    function paintCentre() {
      var history = site.notificationHistory();
      var body = centre.querySelector('.notify-list');
      body.innerHTML = '';
      if (!history.length) {
        var empty = doc.createElement('p');
        empty.className = 'empty';
        empty.setAttribute('data-copy', 'shell.notifications.empty');
        body.appendChild(empty);
      } else {
        history.forEach(function (record) {
          var item = doc.createElement('div');
          item.className = 'notify-item kind-' + record.kind;
          item.innerHTML = '<span data-icon aria-hidden="true">' +
            ({ info: 'info', success: 'check_circle', warning: 'warning', error: 'error' }[record.kind] || 'info') +
            '</span><p data-copy="' + record.key + '"' +
            (record.params ? ' data-copy-params=\'' + JSON.stringify(record.params) + '\'' : '') + '></p>' +
            '<time class="mono">' + record.at.slice(11, 16) + '</time>';
          body.appendChild(item);
        });
      }
      site.applyCopy(centre);
    }

    centre.querySelector('.notify-clear').addEventListener('click', function () {
      site.clearNotifications();
      paintCentre();
    });
    site.subscribe(function (keys) {
      if (keys.indexOf('notifications') !== -1 && !centre.hasAttribute('hidden')) paintCentre();
      if (keys.indexOf('notifications') !== -1) paintBadge();
    });

    function paintBadge() {
      var badge = bell.querySelector('.badge-count');
      var count = site.notificationHistory().length;
      badge.textContent = count ? String(Math.min(count, 99)) : '';
      badge.hidden = !count;
    }
    paintBadge();

    var launch = doc.getElementById('launchTop');
    launch.setAttribute('href', global.BambuViews.APP_HREF);
  }

  /* ------------------------------------------------------------ dim sum */

  function maybeDimSum() {
    var catalogue = global.BAMBU_DIM_SUM;
    if (!catalogue || !site.get('dimSum')) return;
    // Never on a visitor's first run: a surprise is for people already settled in.
    var seenKey = 'bambuStudio.site.seen';
    var seen = false;
    try {
      seen = global.localStorage.getItem(seenKey) === '1';
      global.localStorage.setItem(seenKey, '1');
    } catch (error) {
      return;
    }
    if (!seen) return;
    if (Math.random() >= catalogue.chance) return;

    var dish = catalogue.dishes[Math.floor(Math.random() * catalogue.dishes.length)];
    var card = doc.createElement('aside');
    card.className = 'dimsum';
    card.setAttribute('role', 'note');
    var name = dish.en + ' · ' + dish.yue;
    card.innerHTML =
      '<svg class="dimsum-art" viewBox="0 0 120 120" role="img" aria-label="' + name + '">' + dish.art + '</svg>' +
      '<div class="dimsum-copy">' +
        '<p class="dimsum-badge" data-copy="dimsum.badge"></p>' +
        '<p class="dimsum-name">' + name + '</p>' +
        '<p class="dimsum-line" data-copy="dimsum.line" data-copy-params=\'' +
          JSON.stringify({ dish: name }) + '\'></p>' +
        '<button type="button" class="link-btn dimsum-off" data-copy="dimsum.turnoff"></button>' +
      '</div>' +
      '<button type="button" class="iconbtn dimsum-dismiss" data-copy-attr="aria-label:dimsum.dismiss">' +
        '<span data-icon aria-hidden="true">close</span></button>';
    doc.body.appendChild(card);
    site.applyCopy(card);

    function dismiss() {
      if (card.parentNode) card.parentNode.removeChild(card);
      if (timer) clearTimeout(timer);
    }
    card.querySelector('.dimsum-dismiss').addEventListener('click', dismiss);
    card.querySelector('.dimsum-off').addEventListener('click', function () {
      site.set('dimSum', false);
      site.notify('info', 'notify.dimsum.off');
      dismiss();
    });
    var timer = setTimeout(dismiss, 12000);
  }

  /* --------------------------------------------------------------- boot */

  var tabs = null;

  function refreshAll() {
    site.applyCopy(doc.body);
    if (tabs) {
      var active = tabs.activeId();
      var panel = tabs.panel(active);
      site.applyCopy(panel);
      tabs.layout();
    }
  }

  function start() {
    site.applyAppearance();
    buildHeader();

    tabs = global.BambuTabs.create({
      strip: doc.getElementById('tabstrip'),
      panelHost: doc.getElementById('panels'),
      tabs: TABS
    });
    global.BambuSiteTabs = tabs;

    site.subscribe(function (keys) {
      if (keys.indexOf('languageMode') !== -1 || keys.indexOf('funnyEn') !== -1 ||
          keys.indexOf('funnyYue') !== -1) {
        refreshAll();
      }
    });

    site.applyCopy(doc.body);

    if (site.storageBlocked()) site.notify('warning', 'settings.storage.blocked');
    maybeDimSum();
  }

  if (doc.readyState === 'loading') doc.addEventListener('DOMContentLoaded', start);
  else start();
})(typeof window !== 'undefined' ? window : globalThis);
