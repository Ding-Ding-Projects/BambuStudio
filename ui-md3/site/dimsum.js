/*
 * Startup dim-sum controller.
 *
 * The controller is deliberately separate from boot.js so its timing,
 * cancellation, focus and localization behavior can be executed under a
 * deterministic clock. Production supplies the browser objects; tests supply
 * small equivalents without weakening the runtime path.
 */
(function (global) {
  'use strict';

  var SEEN_KEY = 'bambuStudio.site.seen';
  var RELEASE_ROOT =
    'https://github.com/Ding-Ding-Projects/dim-sum-photos/releases/download/catalog-v1/';
  var LOAD_DEADLINE_MS = 4000;
  var DISMISS_DELAY_MS = 12000;

  function createController(options) {
    var settings = options || {};
    var doc = settings.document || global.document;
    var site = settings.site || global.BambuSite;
    var storage = settings.storage || global.localStorage;
    var random = settings.random || Math.random;
    var schedule = settings.setTimeout || global.setTimeout.bind(global);
    var unschedule = settings.clearTimeout || global.clearTimeout.bind(global);
    var navigationTarget = settings.navigationTarget || global;
    var knownDishes = settings.dishes ||
      (global.BAMBU_DIM_SUM && global.BAMBU_DIM_SUM.dishes) || [];
    var current = null;
    var startupAttempted = false;

    function validDish(dish) {
      if (!dish || !dish.photo || typeof dish.photo.file !== 'string') return false;
      var expectedUrl = RELEASE_ROOT + encodeURIComponent(dish.photo.file);
      var structurallyValid = dish.photo.url === expectedUrl &&
        /^hk-dish-[0-9]{4}(?:-[a-z0-9]+)+\.png$/.test(dish.photo.file) &&
        /^[0-9a-f]{64}$/.test(dish.photo.sha256 || '') &&
        Boolean(dish.en && dish.yue && dish.altEn && dish.altYue);
      return structurallyValid && knownDishes.some(function (known) {
        return known.id === dish.id && known.en === dish.en && known.yue === dish.yue &&
          known.altEn === dish.altEn && known.altYue === dish.altYue &&
          known.photo.file === dish.photo.file && known.photo.url === dish.photo.url &&
          known.photo.sha256 === dish.photo.sha256;
      });
    }

    function focusReturnTarget() {
      return doc.querySelector('[role="tab"][aria-selected="true"]') ||
        doc.getElementById('notifyButton') || doc.body;
    }

    function render(dish, renderOptions) {
      var renderSettings = renderOptions || {};
      if (!doc || !doc.body || !site || !validDish(dish) || current ||
          doc.querySelector('.dimsum')) {
        return Promise.resolve(false);
      }

      var card = doc.createElement('aside');
      card.className = 'dimsum';
      card.setAttribute('role', 'note');

      var image = doc.createElement('img');
      image.className = 'dimsum-art';
      image.width = 76;
      image.height = 76;
      image.decoding = 'async';
      image.referrerPolicy = 'no-referrer';

      var copy = doc.createElement('div');
      copy.className = 'dimsum-copy';
      var badge = doc.createElement('p');
      badge.className = 'dimsum-badge';
      badge.setAttribute('data-copy', 'dimsum.badge');
      var dishName = doc.createElement('p');
      dishName.className = 'dimsum-name';
      var line = doc.createElement('p');
      line.className = 'dimsum-line';
      line.setAttribute('data-copy', 'dimsum.line');
      copy.appendChild(badge);
      copy.appendChild(dishName);
      copy.appendChild(line);

      var close = doc.createElement('button');
      close.type = 'button';
      close.className = 'iconbtn dimsum-dismiss';
      close.setAttribute('data-copy-attr', 'aria-label:dimsum.dismiss');
      var closeIcon = doc.createElement('span');
      closeIcon.setAttribute('data-icon', '');
      closeIcon.setAttribute('aria-hidden', 'true');
      closeIcon.textContent = 'close';
      close.appendChild(closeIcon);

      card.appendChild(image);
      card.appendChild(copy);
      card.appendChild(close);

      var resolveResult;
      var result = new Promise(function (resolve) { resolveResult = resolve; });
      var session = {
        card: card,
        image: image,
        phase: 'loading',
        settled: false,
        closed: false,
        loadTimer: null,
        dismissTimer: null,
        focused: false,
        hovered: false,
        unsubscribe: null,
        removers: []
      };
      current = session;

      function settle(value) {
        if (session.settled) return;
        session.settled = true;
        resolveResult(value);
      }

      function clearActivityListeners() {
        var keep = [];
        session.removers.forEach(function (remove) {
          if (remove.activity) remove();
          else keep.push(remove);
        });
        session.removers = keep;
      }

      function listenForActivity(target, type, listener, listenerOptions) {
        if (!target || !target.addEventListener) return;
        target.addEventListener(type, listener, listenerOptions);
        var remove = function () {
          target.removeEventListener(type, listener, listenerOptions);
        };
        remove.activity = true;
        session.removers.push(remove);
      }

      function updateCopy() {
        var mode = site.languageMode();
        var cantoneseFirst = mode === 'yue_HK';
        var name = cantoneseFirst
          ? dish.yue + ' · ' + dish.en
          : dish.en + ' · ' + dish.yue;
        image.alt = cantoneseFirst
          ? dish.altYue + ' / ' + dish.altEn
          : dish.altEn + ' / ' + dish.altYue;
        dishName.textContent = name;
        line.setAttribute('data-copy-params', JSON.stringify({ dish: name }));
        site.applyCopy(card);
      }

      function clearDismissTimer() {
        if (!session.dismissTimer) return;
        unschedule(session.dismissTimer);
        session.dismissTimer = null;
      }

      function scheduleDismiss() {
        clearDismissTimer();
        if (session.closed || session.phase !== 'shown' || session.focused || session.hovered) return;
        session.dismissTimer = schedule(function () {
          session.dismissTimer = null;
          if (session.focused || session.hovered || card.contains(doc.activeElement)) {
            session.focused = card.contains(doc.activeElement);
            return;
          }
          dismiss();
        }, DISMISS_DELAY_MS);
      }

      function dismiss() {
        if (session.closed) return;
        session.closed = true;
        var ownedFocus = card.contains(doc.activeElement);
        if (session.loadTimer) unschedule(session.loadTimer);
        clearDismissTimer();
        session.removers.splice(0).forEach(function (remove) { remove(); });
        if (session.unsubscribe) session.unsubscribe();
        if (card.parentNode) card.parentNode.removeChild(card);
        current = null;
        settle(false);
        if (ownedFocus) {
          var target = focusReturnTarget();
          if (target && target.focus) target.focus();
        }
      }

      function cancelPending() {
        if (session.closed || session.phase !== 'loading') return;
        image.removeAttribute('src');
        dismiss();
      }

      function show() {
        if (session.closed || session.phase !== 'loading' || doc.hidden) {
          cancelPending();
          return;
        }
        session.phase = 'shown';
        if (session.loadTimer) {
          unschedule(session.loadTimer);
          session.loadTimer = null;
        }
        clearActivityListeners();
        updateCopy();
        var host = site.cornerSurfaceHost ? site.cornerSurfaceHost() : doc.body;
        host.appendChild(card);
        scheduleDismiss();
        settle(true);
      }

      function decodeAndShow() {
        if (session.closed || session.phase !== 'loading') return;
        if (!image.naturalWidth) {
          cancelPending();
          return;
        }
        var decoded;
        try {
          decoded = image.decode ? image.decode() : Promise.resolve();
        } catch (error) {
          cancelPending();
          return;
        }
        Promise.resolve(decoded).then(show, cancelPending);
      }

      close.addEventListener('click', dismiss);
      card.addEventListener('focusin', function () {
        session.focused = true;
        clearDismissTimer();
      });
      card.addEventListener('focusout', function (event) {
        session.focused = card.contains(event.relatedTarget);
        scheduleDismiss();
      });
      card.addEventListener('pointerenter', function () {
        session.hovered = true;
        clearDismissTimer();
      });
      card.addEventListener('pointerleave', function () {
        session.hovered = false;
        scheduleDismiss();
      });
      image.addEventListener('load', decodeAndShow, { once: true });
      image.addEventListener('error', cancelPending, { once: true });

      session.unsubscribe = site.subscribe(function (keys) {
        if (keys.indexOf('activeTab') !== -1 && session.phase === 'loading') {
          cancelPending();
          return;
        }
        if (keys.indexOf('languageMode') !== -1 || keys.indexOf('funnyEn') !== -1 ||
            keys.indexOf('funnyYue') !== -1) updateCopy();
      });

      if (renderSettings.cancelOnActivity) {
        ['pointerdown', 'pointermove', 'wheel', 'keydown', 'input', 'change'].forEach(function (type) {
          listenForActivity(doc, type, cancelPending, true);
        });
        ['hashchange', 'popstate', 'pagehide'].forEach(function (type) {
          listenForActivity(navigationTarget, type, cancelPending, true);
        });
        listenForActivity(doc, 'visibilitychange', function () {
          if (doc.hidden) cancelPending();
        }, true);
      }

      session.loadTimer = schedule(cancelPending, LOAD_DEADLINE_MS);
      image.src = dish.photo.url;
      return result;
    }

    function maybeStart(catalogue) {
      if (startupAttempted) return Promise.resolve(false);
      startupAttempted = true;
      if (!catalogue || !Array.isArray(catalogue.dishes) || !catalogue.dishes.length ||
          typeof catalogue.chance !== 'number' || catalogue.chance < 0 ||
          catalogue.chance > 1 || doc.hidden) {
        return Promise.resolve(false);
      }

      var seen = false;
      try {
        seen = storage.getItem(SEEN_KEY) === '1';
        storage.setItem(SEEN_KEY, '1');
      } catch (error) {
        return Promise.resolve(false);
      }
      if (!seen || random() >= catalogue.chance) return Promise.resolve(false);

      var index = Math.min(catalogue.dishes.length - 1,
        Math.floor(random() * catalogue.dishes.length));
      return render(catalogue.dishes[index], { cancelOnActivity: true });
    }

    return {
      maybeStart: maybeStart,
      render: render,
      active: function () { return Boolean(current); }
    };
  }

  global.BambuDimSum = {
    createController: createController,
    loadDeadlineMs: LOAD_DEADLINE_MS,
    dismissDelayMs: DISMISS_DELAY_MS,
    releaseRoot: RELEASE_ROOT
  };
})(typeof window !== 'undefined' ? window : globalThis);
