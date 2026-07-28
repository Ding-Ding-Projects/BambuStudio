/*
 * Browser-style tab strip.
 *
 * The site is a set of pages reached from a persistent strip, not one long
 * scroll. The strip behaves like browser chrome: tabs can be pinned, dragged
 * into a new order, grouped, and searched; tabs that do not fit move into an
 * overflow menu rather than being clipped or hidden.
 *
 * Layout is measured, never assumed. `fits()` asks whether every visible tab
 * still shares the first tab's row; the strip also wraps in CSS, so even with
 * scripting broken nothing can be clipped off the right edge.
 *
 * Accessibility: a real tablist/tab/tabpanel structure with roving tabindex,
 * arrow-key navigation, Home/End, Shift+Arrow to reorder, and a context menu on
 * each tab. Order, pinning and the active tab persist across restarts.
 */
(function (global) {
  'use strict';

  var site = global.BambuSite;
  var doc = global.document;

  function byId(list, id) {
    return list.filter(function (item) { return item.id === id; })[0];
  }

  function create(config) {
    var definitions = config.tabs;
    var strip = config.strip;
    var panelHost = config.panelHost;
    var ids = definitions.map(function (tab) { return tab.id; });

    var order = (site.get('tabOrder') || []).filter(function (id) { return ids.indexOf(id) !== -1; });
    ids.forEach(function (id) { if (order.indexOf(id) === -1) order.push(id); });
    var pinned = (site.get('tabPinned') || []).filter(function (id) { return ids.indexOf(id) !== -1; });
    // A tab's group is the definition's default until the user reassigns it.
    var storedGroups = site.get('tabGroups') || {};
    var groups = {};
    definitions.forEach(function (definition) {
      groups[definition.id] = storedGroups[definition.id] || definition.group;
    });
    var GROUP_KEYS = ['group.product', 'group.tools', 'group.project'];
    var active = resolveInitialTab();

    var elements = {};
    var panels = {};
    var rendered = {};

    function resolveInitialTab() {
      var params = new URLSearchParams(global.location.search);
      var requested = params.get('tab') || (global.location.hash || '').replace(/^#/, '');
      if (requested && ids.indexOf(requested) !== -1) return requested;
      var stored = site.get('activeTab');
      if (stored && ids.indexOf(stored) !== -1) return stored;
      return order[0];
    }

    /* ------------------------------------------------------------ build */

    var stripInner = doc.createElement('div');
    stripInner.className = 'tabstrip-tabs';
    stripInner.setAttribute('role', 'tablist');
    stripInner.setAttribute('aria-label', 'Site sections');

    var searchButton = doc.createElement('button');
    searchButton.type = 'button';
    searchButton.className = 'iconbtn tabstrip-search';
    searchButton.setAttribute('aria-expanded', 'false');
    searchButton.setAttribute('data-copy-attr', 'aria-label:shell.tabsearch;title:shell.tabsearch');
    searchButton.innerHTML = '<span data-icon aria-hidden="true">search</span>';

    var overflowButton = doc.createElement('button');
    overflowButton.type = 'button';
    overflowButton.className = 'iconbtn tabstrip-overflow';
    overflowButton.setAttribute('aria-expanded', 'false');
    overflowButton.setAttribute('aria-haspopup', 'menu');
    overflowButton.setAttribute('data-copy-attr', 'aria-label:shell.overflow;title:shell.overflow');
    overflowButton.innerHTML = '<span data-icon aria-hidden="true">more_horiz</span>' +
      '<span class="tabstrip-overflow-count" aria-hidden="true"></span>';

    var overflowMenu = doc.createElement('div');
    overflowMenu.className = 'tabstrip-menu';
    overflowMenu.setAttribute('role', 'menu');
    overflowMenu.hidden = true;

    var searchPanel = doc.createElement('div');
    searchPanel.className = 'tabstrip-searchpanel';
    searchPanel.hidden = true;

    strip.appendChild(stripInner);
    var trailing = doc.createElement('div');
    trailing.className = 'tabstrip-trailing';
    trailing.appendChild(searchButton);
    trailing.appendChild(overflowButton);
    strip.appendChild(trailing);
    strip.appendChild(overflowMenu);
    strip.appendChild(searchPanel);

    definitions.forEach(function (definition) {
      var tab = doc.createElement('button');
      tab.type = 'button';
      tab.className = 'tab';
      tab.id = 'tab-' + definition.id;
      tab.setAttribute('role', 'tab');
      tab.setAttribute('aria-controls', 'panel-' + definition.id);
      tab.setAttribute('draggable', 'true');
      tab.dataset.tab = definition.id;
      /*
       * The name lives on the button, not only in the label: when the strip
       * collapses to icons-only the label is display:none and the icon is
       * aria-hidden, which left every tab with an empty accessible name at
       * exactly the widths where a screen-reader user needs it most.
       */
      tab.setAttribute('data-copy-attr', 'aria-label:' + definition.copy +
        ';title:' + definition.copy);
      tab.innerHTML =
        '<span class="tab-icon" data-icon aria-hidden="true">' + definition.icon + '</span>' +
        '<span class="tab-label" data-copy="' + definition.copy + '"></span>';
      tab.addEventListener('click', function () { activate(definition.id); });
      tab.addEventListener('keydown', onTabKeydown);
      tab.addEventListener('contextmenu', function (event) {
        event.preventDefault();
        menuOpener = tab;
        openMenu(definition.id);
      });
      tab.addEventListener('dragstart', function (event) {
        event.dataTransfer.setData('text/plain', definition.id);
        event.dataTransfer.effectAllowed = 'move';
        tab.classList.add('dragging');
      });
      tab.addEventListener('dragend', function () { tab.classList.remove('dragging'); });
      tab.addEventListener('dragover', function (event) {
        event.preventDefault();
        event.dataTransfer.dropEffect = 'move';
        tab.classList.add('dragover');
      });
      tab.addEventListener('dragleave', function () { tab.classList.remove('dragover'); });
      tab.addEventListener('drop', function (event) {
        event.preventDefault();
        tab.classList.remove('dragover');
        var moved = event.dataTransfer.getData('text/plain');
        if (moved && moved !== definition.id) moveBefore(moved, definition.id);
      });
      elements[definition.id] = tab;

      var panel = doc.createElement('section');
      panel.className = 'panel';
      panel.id = 'panel-' + definition.id;
      panel.setAttribute('role', 'tabpanel');
      panel.setAttribute('aria-labelledby', tab.id);
      panel.tabIndex = 0;
      panel.hidden = true;
      panels[definition.id] = panel;
      panelHost.appendChild(panel);
    });

    /* ---------------------------------------------------------- ordering */

    function sortedIds() {
      var pinnedFirst = order.filter(function (id) { return pinned.indexOf(id) !== -1; });
      var rest = order.filter(function (id) { return pinned.indexOf(id) === -1; });
      return pinnedFirst.concat(rest);
    }

    function paintOrder() {
      var previousGroup = null;
      sortedIds().forEach(function (id) {
        var tab = elements[id];
        tab.classList.toggle('pinned', pinned.indexOf(id) !== -1);
        tab.dataset.group = groups[id] || '';
        // A visible seam between groups, so grouping is not a hidden preference.
        tab.classList.toggle('group-start', previousGroup !== null && groups[id] !== previousGroup);
        previousGroup = groups[id];
        stripInner.appendChild(tab);
      });
    }

    function persist() {
      site.set('tabOrder', order.slice());
      site.set('tabPinned', pinned.slice());
      site.set('tabGroups', JSON.parse(JSON.stringify(groups)));
    }

    function setGroup(id, group) {
      groups[id] = group;
      persist();
      paintOrder();
      layout();
    }

    function moveBefore(movedId, targetId) {
      var from = order.indexOf(movedId);
      if (from === -1) return;
      order.splice(from, 1);
      var to = order.indexOf(targetId);
      order.splice(to === -1 ? order.length : to, 0, movedId);
      persist();
      paintOrder();
      layout();
    }

    function shift(id, delta) {
      var list = sortedIds();
      var index = list.indexOf(id);
      var target = list[index + delta];
      if (!target) return;
      if ((pinned.indexOf(id) !== -1) !== (pinned.indexOf(target) !== -1)) return;
      var a = order.indexOf(id);
      var b = order.indexOf(target);
      order[a] = target;
      order[b] = id;
      persist();
      paintOrder();
      layout();
      elements[id].focus();
    }

    function togglePin(id) {
      var index = pinned.indexOf(id);
      if (index === -1) pinned.push(id);
      else pinned.splice(index, 1);
      persist();
      paintOrder();
      layout();
      site.notify('info', index === -1 ? 'notify.tab.pinned' : 'notify.tab.unpinned',
        { name: site.text(byId(definitions, id).copy) });
    }

    function resetOrder() {
      order = ids.slice();
      pinned = [];
      definitions.forEach(function (definition) { groups[definition.id] = definition.group; });
      persist();
      paintOrder();
      layout();
      site.notify('info', 'notify.tab.reset');
    }

    /* --------------------------------------------------------- activation */

    function activate(id, options) {
      if (ids.indexOf(id) === -1) return;
      active = id;
      site.set('activeTab', id);
      sortedIds().forEach(function (candidate) {
        var isActive = candidate === id;
        var tab = elements[candidate];
        tab.setAttribute('aria-selected', String(isActive));
        tab.tabIndex = isActive ? 0 : -1;
        tab.classList.toggle('active', isActive);
        panels[candidate].hidden = !isActive;
      });
      if (!rendered[id]) {
        var definition = byId(definitions, id);
        definition.render(panels[id]);
        rendered[id] = true;
      } else {
        var refresh = byId(definitions, id).refresh;
        if (refresh) refresh(panels[id]);
      }
      site.applyCopy(panels[id]);
      closeMenus({ restoreFocus: false });
      // The newly active tab may have been sitting in the overflow menu, where
      // it is display:none. Un-hide it and re-run layout synchronously, because
      // focusing an element that is not rendered throws focus back to <body>.
      elements[id].classList.remove('overflowed');
      runLayout();
      try {
        var url = new URL(global.location.href);
        url.hash = id;
        global.history.replaceState(null, '', url.toString());
      } catch (error) { /* file:// history writes are not essential */ }
      if (options && options.focus) elements[id].focus();
    }

    function onTabKeydown(event) {
      var id = event.currentTarget.dataset.tab;
      var list = sortedIds();
      var index = list.indexOf(id);
      var next = null;
      if (event.key === 'ArrowRight') next = list[(index + 1) % list.length];
      else if (event.key === 'ArrowLeft') next = list[(index - 1 + list.length) % list.length];
      else if (event.key === 'Home') next = list[0];
      else if (event.key === 'End') next = list[list.length - 1];
      if (next && event.shiftKey && (event.key === 'ArrowRight' || event.key === 'ArrowLeft')) {
        event.preventDefault();
        shift(id, event.key === 'ArrowRight' ? 1 : -1);
        return;
      }
      if (next) {
        event.preventDefault();
        activate(next, { focus: true });
        return;
      }
      if (event.key === 'p' && event.altKey) {
        event.preventDefault();
        togglePin(id);
      }
    }

    /* ------------------------------------------------------------ menus */

    var menuOpener = null;

    /*
     * Closing a popover while focus is inside it would drop focus on the body
     * and strand a keyboard user at the top of the document, so focus goes back
     * to whatever opened it.
     */
    function closeMenus(options) {
      var restore = !options || options.restoreFocus !== false;
      var hadFocus = overflowMenu.contains(doc.activeElement) || searchPanel.contains(doc.activeElement);
      overflowMenu.hidden = true;
      overflowButton.setAttribute('aria-expanded', 'false');
      searchPanel.hidden = true;
      searchButton.setAttribute('aria-expanded', 'false');
      if (restore && hadFocus && menuOpener) menuOpener.focus();
      menuOpener = null;
    }

    /** Roving focus inside the overflow menu, as a menu is expected to have. */
    function onMenuKeydown(event) {
      var focusable = [].slice.call(overflowMenu.querySelectorAll('.menuitem'));
      if (!focusable.length) return;
      var index = focusable.indexOf(doc.activeElement);
      if (event.key === 'ArrowDown' || event.key === 'ArrowUp') {
        event.preventDefault();
        var step = event.key === 'ArrowDown' ? 1 : -1;
        var next = (index + step + focusable.length) % focusable.length;
        focusable[next < 0 ? focusable.length - 1 : next].focus();
      } else if (event.key === 'Home') {
        event.preventDefault();
        focusable[0].focus();
      } else if (event.key === 'End') {
        event.preventDefault();
        focusable[focusable.length - 1].focus();
      }
    }

    function menuItem(labelKey, icon, onClick, params) {
      var item = doc.createElement('button');
      item.type = 'button';
      item.className = 'menuitem';
      item.setAttribute('role', 'menuitem');
      item.innerHTML = '<span data-icon aria-hidden="true">' + icon + '</span>' +
        '<span class="menuitem-label" data-copy="' + labelKey + '"' +
        (params ? ' data-copy-params=\'' + JSON.stringify(params) + '\'' : '') + '></span>';
      item.addEventListener('click', onClick);
      return item;
    }

    function openMenu(forTab) {
      overflowMenu.innerHTML = '';
      var hidden = sortedIds().filter(function (id) { return elements[id].classList.contains('overflowed'); });
      hidden.forEach(function (id) {
        overflowMenu.appendChild(menuItem(byId(definitions, id).copy, byId(definitions, id).icon, function () {
          activate(id, { focus: true });
        }));
      });
      if (hidden.length) overflowMenu.appendChild(divider());
      var target = forTab || active;
      var definition = byId(definitions, target);
      var heading = doc.createElement('p');
      heading.className = 'menu-heading';
      heading.setAttribute('data-copy', definition.copy);
      overflowMenu.appendChild(heading);
      overflowMenu.appendChild(menuItem(
        pinned.indexOf(target) === -1 ? 'shell.pin' : 'shell.unpin',
        pinned.indexOf(target) === -1 ? 'keep' : 'keep_off',
        function () { togglePin(target); openMenu(target); }
      ));
      overflowMenu.appendChild(menuItem('shell.moveleft', 'chevron_left', function () {
        shift(target, -1); openMenu(target);
      }));
      overflowMenu.appendChild(menuItem('shell.moveright', 'chevron_right', function () {
        shift(target, 1); openMenu(target);
      }));
      overflowMenu.appendChild(divider());
      var groupHeading = doc.createElement('p');
      groupHeading.className = 'menu-heading';
      groupHeading.setAttribute('data-copy', 'shell.group');
      overflowMenu.appendChild(groupHeading);
      GROUP_KEYS.forEach(function (groupKey) {
        var item = menuItem(groupKey, groups[target] === groupKey ? 'radio_button_checked' : 'radio_button_unchecked',
          function () { setGroup(target, groupKey); openMenu(target); });
        item.setAttribute('role', 'menuitemradio');
        item.setAttribute('aria-checked', String(groups[target] === groupKey));
        overflowMenu.appendChild(item);
      });
      overflowMenu.appendChild(divider());
      overflowMenu.appendChild(menuItem('shell.tabsearch', 'search', function () {
        closeMenus();
        openSearch();
      }));
      overflowMenu.appendChild(menuItem('shell.resettabs', 'restart_alt', function () {
        resetOrder(); closeMenus();
      }));
      site.applyCopy(overflowMenu);
      searchPanel.hidden = true;
      overflowMenu.hidden = false;
      overflowButton.setAttribute('aria-expanded', 'true');
      if (!menuOpener) menuOpener = forTab && elements[forTab] ? elements[forTab] : overflowButton;
      overflowMenu.onkeydown = onMenuKeydown;
      var first = overflowMenu.querySelector('.menuitem');
      if (first) first.focus();
    }

    function divider() {
      var line = doc.createElement('div');
      line.className = 'menu-divider';
      return line;
    }

    var searchField = null;
    var searchResults = null;

    function tabSearchCorpus() {
      return definitions.map(function (definition) {
        return site.pair(definition.copy).en + ' ' + site.pair(definition.copy).yue;
      }).join('\n');
    }

    function searchItems() {
      return sortedIds().map(function (id) {
        var both = site.pair(byId(definitions, id).copy);
        return { id: id, text: both.en + ' ' + both.yue + ' ' + id };
      });
    }

    function openSearch() {
      if (!searchField) {
        searchField = global.BambuRegex.createSearchField({
          labelKey: 'shell.tabsearch',
          sampleProvider: tabSearchCorpus,
          items: searchItems,
          onResults: function (ids) { renderSearchResults(ids); }
        });
        searchPanel.appendChild(searchField.element);
        searchResults = doc.createElement('div');
        searchResults.className = 'tabsearch-results';
        searchPanel.appendChild(searchResults);
      }
      overflowMenu.hidden = true;
      searchPanel.hidden = false;
      searchButton.setAttribute('aria-expanded', 'true');
      searchField.apply();
      searchField.element.querySelector('.sf-input').focus();
    }

    function renderSearchResults(ids) {
      searchResults.innerHTML = '';
      var matched = ids === null
        ? sortedIds()
        : sortedIds().filter(function (id) { return ids.indexOf(id) !== -1; });
      if (!matched.length) {
        var empty = doc.createElement('p');
        empty.className = 'empty';
        empty.setAttribute('data-copy', 'settings.search.empty');
        searchResults.appendChild(empty);
        site.applyCopy(searchResults);
        return;
      }
      matched.forEach(function (id) {
        var definition = byId(definitions, id);
        var item = doc.createElement('button');
        item.type = 'button';
        item.className = 'tabsearch-item';
        item.innerHTML = '<span data-icon aria-hidden="true">' + definition.icon + '</span>' +
          '<span data-copy="' + definition.copy + '"></span>' +
          '<span class="tabsearch-group" data-copy="' + (groups[id] || 'group.product') + '"></span>' +
          (pinned.indexOf(id) !== -1 ? '<span class="pin-flag" data-icon aria-hidden="true">keep</span>' : '');
        item.addEventListener('click', function () {
          activate(id, { focus: true });
          closeMenus();
        });
        searchResults.appendChild(item);
      });
      site.applyCopy(searchResults);
    }

    overflowButton.addEventListener('click', function () {
      if (!overflowMenu.hidden) closeMenus();
      else {
        menuOpener = overflowButton;
        openMenu(active);
      }
    });
    searchButton.addEventListener('click', function () {
      if (!searchPanel.hidden) closeMenus();
      else {
        menuOpener = searchButton;
        openSearch();
      }
    });
    doc.addEventListener('click', function (event) {
      if (!strip.contains(event.target)) closeMenus();
    });
    doc.addEventListener('keydown', function (event) {
      if (event.key === 'Escape') closeMenus();
    });

    /* ----------------------------------------------------------- layout */

    /*
     * The strip fits when every visible tab AND the trailing controls sit on
     * one row. Measuring only the tabs would call a strip "fitting" while the
     * search and overflow buttons had already been pushed onto a second line.
     */
    function fits() {
      var visible = sortedIds()
        .map(function (id) { return elements[id]; })
        .filter(function (tab) { return !tab.classList.contains('overflowed'); });
      [searchButton, overflowButton].forEach(function (button) {
        if (!button.hidden) visible.push(button);
      });
      if (visible.length < 2) return true;
      var top = visible[0].offsetTop;
      return visible.every(function (element) { return Math.abs(element.offsetTop - top) < 2; });
    }

    var layoutPending = null;
    function layout() {
      // Deliberately a timer rather than requestAnimationFrame: a page in a
      // background or non-compositing tab never runs rAF callbacks, and the
      // strip must still be measured and settled there — headless capture and
      // the deploy gate both load the page without painting it.
      if (layoutPending) clearTimeout(layoutPending);
      layoutPending = setTimeout(function () {
        layoutPending = null;
        runLayout();
      }, 0);
    }

    function runLayout() {
      // Stage 0 — everything visible with labels.
      stripInner.classList.remove('icons-only');
      searchButton.hidden = false;
      overflowButton.hidden = true;
      sortedIds().forEach(function (id) { elements[id].classList.remove('overflowed'); });
      if (fits()) return finishLayout();

      // Stage 1 — the strip keeps its labels only while they all fit.
      overflowButton.hidden = false;
      if (fits()) return finishLayout();
      stripInner.classList.add('icons-only');
      if (fits()) return finishLayout();

      // Stage 2 — push tabs into the overflow menu, last first, never the active one.
      var list = sortedIds().slice().reverse();
      for (var index = 0; index < list.length && !fits(); index++) {
        if (list[index] === active) continue;
        elements[list[index]].classList.add('overflowed');
      }
      if (fits()) return finishLayout();

      // Stage 3 — the tab search lives inside the overflow menu at this width.
      searchButton.hidden = true;
      finishLayout();
    }

    function finishLayout() {
      var hidden = sortedIds().filter(function (id) {
        return elements[id].classList.contains('overflowed');
      });
      overflowButton.querySelector('.tabstrip-overflow-count').textContent = hidden.length ? String(hidden.length) : '';
      strip.classList.toggle('has-overflow', hidden.length > 0);
    }

    if (global.ResizeObserver) {
      new global.ResizeObserver(function () { layout(); }).observe(strip);
    }
    global.addEventListener('resize', layout);
    // Before the icon font arrives each icon renders as its own ligature name,
    // which is far wider than the glyph, so the strip would keep an overflow
    // menu it no longer needs. `fonts.ready` alone is not enough — it can settle
    // before a face this page has not painted yet begins loading — so the two
    // faces the strip measures are requested explicitly and re-measured.
    if (doc.fonts) {
      if (doc.fonts.load) {
        doc.fonts.load("400 20px 'Material Symbols Outlined'").then(layout, function () {});
        doc.fonts.load("500 14px 'Roboto'").then(layout, function () {});
      }
      if (doc.fonts.ready) doc.fonts.ready.then(layout, function () {});
    }
    global.addEventListener('load', layout);

    site.subscribe(function (keys) {
      if (keys.indexOf('languageMode') !== -1 || keys.indexOf('funnyEn') !== -1 || keys.indexOf('funnyYue') !== -1) {
        site.applyCopy(strip);
        layout();
      }
    });

    paintOrder();
    site.applyCopy(strip);
    activate(active);
    layout();

    return {
      activate: activate,
      layout: layout,
      ids: function () { return sortedIds(); },
      activeId: function () { return active; },
      panel: function (id) { return panels[id]; },
      isOverflowed: function (id) { return elements[id].classList.contains('overflowed'); }
    };
  }

  global.BambuTabs = { create: create };
})(typeof window !== 'undefined' ? window : globalThis);
