/*
 * Motion for the landing site: scroll reveals, tab transitions, the hero
 * toolpaths and the reduced-motion path.
 *
 * Reduced motion is a different design, not a slower one. With it on, nothing
 * reveals (every section is simply there), tabs switch without a transition,
 * the hero toolpaths are drawn complete and settle on one hue, and cards do
 * not lift. It follows the system setting until the user chooses otherwise in
 * the accessibility tools, and the choice is saved in this browser.
 *
 * Nothing here is needed for the site to work: with scripting off the CSS
 * shows every section in place, and with IntersectionObserver missing the
 * reveal classes are added at once.
 */
(function (global) {
  'use strict';

  var site = global.BambuSite;
  var doc = global.document;
  var root = doc.documentElement;
  var observer = null;
  var previewReduced = false;

  function systemReduced() {
    try {
      return !!(global.matchMedia && global.matchMedia('(prefers-reduced-motion: reduce)').matches);
    } catch (error) {
      return false;
    }
  }

  function reduced() {
    if (previewReduced) return true;
    var choice = site.get('reducedMotion');
    if (choice === 'on') return true;
    if (choice === 'off') return false;
    return systemReduced();
  }

  function apply() {
    var off = reduced();
    root.setAttribute('data-reduced-motion', off ? 'on' : 'off');
    root.setAttribute('data-motion-tab', off ? 'none' : String(site.get('motionTab') || 'fade'));
    root.setAttribute('data-motion-hero', off || site.get('motionHero') === false ? 'off' : 'on');
    root.setAttribute('data-motion-hover', off || site.get('motionHover') === false ? 'off' : 'on');
    root.setAttribute('data-high-contrast', site.get('highContrast') ? 'on' : 'off');
    root.setAttribute('data-focus-always', site.get('focusAlways') ? 'on' : 'off');
    root.setAttribute('data-larger-targets', site.get('largerTargets') ? 'on' : 'off');
    var reveal = Number(site.get('motionReveal'));
    if (!isFinite(reveal) || reveal < 0) reveal = 320;
    root.style.setProperty('--motion-reveal', (off ? 0 : Math.min(800, reveal)) + 'ms');
    if (off) revealEverything();
  }

  /* ------------------------------------------------------------ reveals */

  function revealEverything() {
    doc.querySelectorAll('.reveal:not(.revealed)').forEach(function (element) {
      element.classList.add('revealed');
    });
  }

  function watch(scope) {
    var targets = (scope || doc).querySelectorAll('.reveal:not(.revealed)');
    if (!targets.length) return;
    // An automated browser (the layout gate, the documentation capture) never
    // scrolls, so it sees every section at once; a person gets the reveal.
    var automated = !!(global.navigator && global.navigator.webdriver);
    if (reduced() || automated || typeof global.IntersectionObserver !== 'function') {
      targets.forEach(function (element) { element.classList.add('revealed'); });
      return;
    }
    if (!observer) {
      observer = new global.IntersectionObserver(function (entries) {
        entries.forEach(function (entry) {
          if (!entry.isIntersecting) return;
          entry.target.classList.add('revealed');
          observer.unobserve(entry.target);
        });
      }, { rootMargin: '0px 0px -8% 0px', threshold: 0.08 });
    }
    targets.forEach(function (element, index) {
      // A short stagger per sibling, capped so a long list never waits.
      element.style.setProperty('--reveal-delay', Math.min(index, 8) * 45 + 'ms');
      observer.observe(element);
    });
    // Anything already on screen when a panel opens must not sit invisible
    // waiting for a scroll that never comes.
    global.setTimeout(function () {
      targets.forEach(function (element) {
        if (element.classList.contains('revealed')) return;
        var rect = element.getBoundingClientRect();
        if (rect.top < (global.innerHeight || 900) && rect.bottom > 0) element.classList.add('revealed');
      });
    }, 60);
  }

  /* ---------------------------------------------------- tab transitions */

  // The switch runs synchronously, because the layout gate and the capture
  // activate a tab and measure it at once; the entering panel then plays a
  // short CSS keyframe (cross fade or slide) chosen in the Tools tab.
  function transition(update) {
    update();
    if (reduced() || site.get('motionTab') === 'none') return;
    var panel = doc.querySelector('main .panel:not([hidden])');
    if (!panel) return;
    panel.classList.remove('tab-enter');
    void panel.offsetWidth; // restart the animation when the same panel re-enters
    panel.classList.add('tab-enter');
    global.setTimeout(function () { panel.classList.remove('tab-enter'); }, 400);
  }

  /* ---------------------------------------------------------- preview */

  function setPreview(on) {
    previewReduced = !!on;
    apply();
  }

  site.subscribe(function (keys) {
    var mine = ['reducedMotion', 'motionReveal', 'motionTab', 'motionHero', 'motionHover',
      'highContrast', 'focusAlways', 'largerTargets'];
    if (keys.some(function (key) { return mine.indexOf(key) !== -1; })) apply();
  });
  try {
    var query = global.matchMedia && global.matchMedia('(prefers-reduced-motion: reduce)');
    if (query && query.addEventListener) query.addEventListener('change', apply);
  } catch (error) { /* no media queries here */ }

  apply();

  global.BambuMotion = {
    reduced: reduced,
    apply: apply,
    watch: watch,
    transition: transition,
    setPreview: setPreview,
    previewing: function () { return previewReduced; }
  };
})(typeof window !== 'undefined' ? window : globalThis);
