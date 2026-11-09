/*
 * The Every feature tab and the feature teaser on the Overview.
 *
 * Every feature of the Windows app gets a card: an illustration drawn inline
 * (so it follows the accent colour and the theme), the name, a short body, an
 * info icon, the place it lives in the app and a link that opens the matching
 * article in the Docs tab. The list is the same one the app's Every feature
 * page carries, so what the site promises is what the app has.
 */
(function (global) {
  'use strict';

  var site = global.BambuSite;
  var doc = global.document;

  // id, icon, docs article id (see scripts/build-docs-data.mjs), where it lives in the app
  var FEATURES = [
    { id: 'localtools', icon: 'handyman', article: 'windows-local-tools', inApp: 'Local tools tab' },
    { id: 'livecharts', icon: 'monitoring', article: 'device-live-charts', inApp: 'Device page, Live charts' },
    { id: 'elementlocks', icon: 'lock', article: 'windows-element-locks', inApp: 'Right-click any row or object' },
    { id: 'attention', icon: 'self_improvement', article: 'windows-attention-modes', inApp: 'Preferences, Attention' },
    { id: 'drawingboard', icon: 'draw', article: 'windows-drawing-board', inApp: 'Local tools, Drawing board' },
    { id: 'virtualdisks', icon: 'hard_drive', article: 'windows-virtual-disks', inApp: 'Local tools, Virtual disks' },
    { id: 'minecraft', icon: 'dns', article: 'windows-minecraft-servers', inApp: 'Local tools, Servers' },
    { id: 'extensions', icon: 'extension', article: 'windows-extensions', inApp: 'File, Extensions' },
    { id: 'base64', icon: 'code', article: 'windows-base64-toolkit', inApp: 'Local tools, Base64' },
    { id: 'wizard', icon: 'auto_fix_high', article: 'windows-practical-setup', inApp: 'First run, Help, Setup wizard' },
    { id: 'preferences', icon: 'tune', article: 'windows-preferences-provenance', inApp: 'Edit, Preferences' },
    { id: 'everyfeature', icon: 'search', article: 'windows-every-feature', inApp: 'Help, Every feature' },
    { id: 'languages', icon: 'translate', article: 'windows-language-modes', inApp: 'Preferences, Language' },
    { id: 'dimsum', icon: 'ramen_dining', article: 'windows-dim-sum-surprise', inApp: 'Help, About' },
    { id: 'multislice', icon: 'stacks', article: 'prepare-multi-slice', inApp: 'Prepare, Slice all' },
    { id: 'landrop', icon: 'wifi', article: 'application-integration-lan-model-drop', inApp: 'Local tools, LAN model drop' }
  ];

  /* ------------------------------------------------------ illustrations */

  // Each drawing is 160 x 100, stroke-only, in currentColor and the accent, so
  // it reads in both themes and under any seed colour. The `fx-*` classes carry
  // the animation; with reduced motion the CSS leaves every drawing complete.
  function art(id) {
    var open = '<svg class="feature-art" viewBox="0 0 160 100" aria-hidden="true" focusable="false">';
    var close = '</svg>';
    var a = 'var(--md-primary)';
    var m = 'var(--md-outline)';
    switch (id) {
      case 'localtools':
        return open +
          '<rect x="14" y="18" width="132" height="66" rx="10" fill="none" stroke="' + m + '" stroke-width="2"/>' +
          '<rect x="22" y="26" width="32" height="50" rx="6" fill="none" stroke="' + a + '" stroke-width="2"/>' +
          '<rect class="fx-pulse" x="62" y="26" width="76" height="14" rx="4" fill="' + a + '" opacity=".35"/>' +
          '<rect class="fx-pulse" style="animation-delay:.3s" x="62" y="46" width="76" height="14" rx="4" fill="' + a + '" opacity=".25"/>' +
          '<rect class="fx-pulse" style="animation-delay:.6s" x="62" y="66" width="48" height="10" rx="4" fill="' + a + '" opacity=".2"/>' + close;
      case 'livecharts':
        return open +
          '<path d="M14 80 H146" stroke="' + m + '" stroke-width="1.5"/>' +
          '<path d="M14 40 H146" stroke="' + m + '" stroke-width="1" stroke-dasharray="4 4"/>' +
          '<path class="fx-draw" d="M14 70 C 34 68, 40 52, 56 48 S 84 44, 98 36 S 126 30, 146 32" fill="none" stroke="' + a + '" stroke-width="3" stroke-linecap="round"/>' +
          '<circle class="fx-blink" cx="146" cy="32" r="4" fill="' + a + '"/>' +
          '<path d="M128 44 L 146 24" stroke="' + a + '" stroke-width="2" stroke-linecap="round"/>' +
          '<path d="M138 24 L 146 24 L 146 32" fill="none" stroke="' + a + '" stroke-width="2" stroke-linecap="round"/>' + close;
      case 'elementlocks':
        return open +
          '<rect x="20" y="26" width="120" height="14" rx="4" fill="none" stroke="' + m + '" stroke-width="2"/>' +
          '<rect x="20" y="48" width="120" height="14" rx="4" fill="' + a + '" opacity=".25" stroke="' + a + '" stroke-width="2"/>' +
          '<rect x="20" y="70" width="120" height="14" rx="4" fill="none" stroke="' + m + '" stroke-width="2"/>' +
          '<g class="fx-lock"><rect x="112" y="51" width="16" height="11" rx="2" fill="' + a + '"/>' +
          '<path d="M115 51 V 47 a5 5 0 0 1 10 0 V 51" fill="none" stroke="' + a + '" stroke-width="2"/></g>' + close;
      case 'attention':
        return open +
          '<circle cx="80" cy="50" r="30" fill="none" stroke="' + m + '" stroke-width="2"/>' +
          '<circle class="fx-breathe" cx="80" cy="50" r="18" fill="' + a + '" opacity=".3"/>' +
          '<circle cx="80" cy="50" r="6" fill="' + a + '"/>' +
          '<path d="M26 50 H 40 M120 50 H 134 M80 8 V 20 M80 80 V 92" stroke="' + m + '" stroke-width="2" stroke-linecap="round"/>' + close;
      case 'drawingboard':
        return open +
          '<rect x="18" y="16" width="124" height="68" rx="8" fill="none" stroke="' + m + '" stroke-width="2"/>' +
          '<path class="fx-draw" d="M34 68 C 50 30, 70 30, 84 56 S 112 40, 126 30" fill="none" stroke="' + a + '" stroke-width="3" stroke-linecap="round"/>' +
          '<path class="fx-draw" style="animation-delay:.5s" d="M40 40 L 60 24" fill="none" stroke="' + a + '" stroke-width="2" stroke-linecap="round"/>' +
          '<circle cx="126" cy="30" r="5" fill="' + a + '"/>' + close;
      case 'virtualdisks':
        return open +
          '<rect x="30" y="24" width="100" height="52" rx="8" fill="none" stroke="' + m + '" stroke-width="2"/>' +
          '<circle class="fx-spin" cx="60" cy="50" r="14" fill="none" stroke="' + a + '" stroke-width="3" stroke-dasharray="30 14"/>' +
          '<circle cx="60" cy="50" r="4" fill="' + a + '"/>' +
          '<rect x="88" y="42" width="32" height="16" rx="4" fill="' + a + '"/>' +
          '<path d="M96 46 h10 l-10 8 h10" fill="none" stroke="var(--md-on-primary)" stroke-width="2" stroke-linejoin="round"/>' + close;
      case 'minecraft':
        return open +
          '<rect x="52" y="22" width="56" height="56" fill="none" stroke="' + m + '" stroke-width="2"/>' +
          '<rect class="fx-pulse" x="60" y="30" width="18" height="18" fill="' + a + '" opacity=".5"/>' +
          '<rect class="fx-pulse" style="animation-delay:.4s" x="82" y="30" width="18" height="18" fill="' + a + '" opacity=".3"/>' +
          '<rect class="fx-pulse" style="animation-delay:.8s" x="60" y="52" width="18" height="18" fill="' + a + '" opacity=".3"/>' +
          '<rect x="82" y="52" width="18" height="18" fill="' + a + '" opacity=".5"/>' + close;
      case 'extensions':
        return open +
          '<path d="M40 30 h22 a8 8 0 0 1 16 0 h22 v22 a8 8 0 0 1 0 16 v22 h-22 a8 8 0 0 1 -16 0 h-22 v-22 a8 8 0 0 1 0 -16 z" fill="none" stroke="' + m + '" stroke-width="2"/>' +
          '<g class="fx-float"><path d="M104 30 h22 v22 a8 8 0 0 0 0 16 v22 h-22 v-22 a8 8 0 0 1 0 -16 z" fill="' + a + '" opacity=".35" stroke="' + a + '" stroke-width="2"/></g>' + close;
      case 'base64':
        return open +
          '<g class="fx-blink">' + [22, 36, 50, 64, 78, 92, 106, 120].map(function (x, i) {
            return '<rect x="' + x + '" y="' + (24 + (i % 3) * 4) + '" width="10" height="' + (18 - (i % 3) * 4) + '" rx="2" fill="' + m + '"/>';
          }).join('') + '</g>' +
          '<path d="M80 50 v 10 M74 56 l6 6 l6 -6" fill="none" stroke="' + a + '" stroke-width="2" stroke-linecap="round"/>' +
          '<rect x="36" y="70" width="88" height="14" rx="4" fill="' + a + '" opacity=".35"/>' +
          '<rect x="44" y="74" width="40" height="6" rx="3" fill="' + a + '"/>' + close;
      case 'wizard':
        return open +
          '<path d="M24 50 H 136" stroke="' + m + '" stroke-width="2"/>' +
          '<circle cx="40" cy="50" r="9" fill="' + a + '"/>' +
          '<circle class="fx-blink" cx="80" cy="50" r="9" fill="' + a + '" opacity=".6"/>' +
          '<circle cx="120" cy="50" r="9" fill="none" stroke="' + m + '" stroke-width="2"/>' +
          '<path d="M36 50 l3 3 l6 -6" fill="none" stroke="var(--md-on-primary)" stroke-width="2" stroke-linecap="round"/>' + close;
      case 'preferences':
        return open +
          '<path d="M30 32 H 130 M30 50 H 130 M30 68 H 130" stroke="' + m + '" stroke-width="2" stroke-linecap="round"/>' +
          '<circle class="fx-slide" cx="60" cy="32" r="7" fill="' + a + '"/>' +
          '<circle class="fx-slide" style="animation-delay:.4s" cx="100" cy="50" r="7" fill="' + a + '"/>' +
          '<circle class="fx-slide" style="animation-delay:.8s" cx="48" cy="68" r="7" fill="' + a + '"/>' + close;
      case 'everyfeature':
        return open +
          '<circle cx="66" cy="44" r="22" fill="none" stroke="' + a + '" stroke-width="3"/>' +
          '<path class="fx-draw" d="M82 60 L 110 86" stroke="' + a + '" stroke-width="4" stroke-linecap="round"/>' +
          '<path d="M118 24 H 146 M118 36 H 146 M118 48 H 140" stroke="' + m + '" stroke-width="2" stroke-linecap="round"/>' + close;
      case 'languages':
        return open +
          '<path d="M22 28 h58 a8 8 0 0 1 8 8 v22 a8 8 0 0 1 -8 8 h-30 l-12 10 v-10 h-16 a8 8 0 0 1 -8 -8 v-22 a8 8 0 0 1 8 -8 z" fill="none" stroke="' + m + '" stroke-width="2"/>' +
          '<path d="M34 42 h34 M34 52 h22" stroke="' + m + '" stroke-width="3" stroke-linecap="round"/>' +
          '<g class="fx-float"><path d="M80 44 h58 a8 8 0 0 1 8 8 v22 a8 8 0 0 1 -8 8 h-16 v10 l-12 -10 h-30 a8 8 0 0 1 -8 -8 v-22 a8 8 0 0 1 8 -8 z" fill="' + a + '" opacity=".25" stroke="' + a + '" stroke-width="2"/>' +
          '<path d="M92 58 h34 M92 68 h22" stroke="' + a + '" stroke-width="3" stroke-linecap="round"/></g>' + close;
      case 'dimsum':
        return open +
          '<ellipse cx="80" cy="72" rx="46" ry="8" fill="none" stroke="' + m + '" stroke-width="2"/>' +
          '<path d="M38 72 V 56 a42 10 0 0 1 84 0 V 72" fill="none" stroke="' + m + '" stroke-width="2"/>' +
          '<g class="fx-float"><path d="M62 54 c0 -14 36 -14 36 0 c0 8 -36 8 -36 0" fill="' + a + '" opacity=".6"/>' +
          '<path d="M66 44 q14 -16 28 0" fill="none" stroke="' + a + '" stroke-width="2"/></g>' +
          '<path class="fx-steam" d="M70 34 q4 -6 0 -12 M80 32 q4 -6 0 -12 M90 34 q4 -6 0 -12" fill="none" stroke="' + m + '" stroke-width="1.5" stroke-linecap="round"/>' + close;
      case 'multislice':
        return open +
          '<rect class="fx-pulse" x="28" y="54" width="104" height="10" rx="3" fill="' + a + '" opacity=".25"/>' +
          '<rect class="fx-pulse" style="animation-delay:.25s" x="36" y="42" width="88" height="10" rx="3" fill="' + a + '" opacity=".4"/>' +
          '<rect class="fx-pulse" style="animation-delay:.5s" x="44" y="30" width="72" height="10" rx="3" fill="' + a + '" opacity=".6"/>' +
          '<path d="M20 74 H 140" stroke="' + m + '" stroke-width="2"/>' + close;
      case 'landrop':
      default:
        return open +
          '<path class="fx-wave" d="M50 44 a42 42 0 0 1 60 0" fill="none" stroke="' + a + '" stroke-width="3" stroke-linecap="round"/>' +
          '<path class="fx-wave" style="animation-delay:.3s" d="M62 56 a25 25 0 0 1 36 0" fill="none" stroke="' + a + '" stroke-width="3" stroke-linecap="round"/>' +
          '<circle cx="80" cy="68" r="5" fill="' + a + '"/>' +
          '<path d="M30 84 H 130" stroke="' + m + '" stroke-width="2"/>' + close;
    }
  }

  /* ---------------------------------------------------------------- cards */

  function card(feature, options) {
    var compact = options && options.compact;
    return '<article class="feature-card reveal' + (compact ? ' compact' : '') + '" data-feature="' + feature.id + '">' +
      '<div class="feature-stage">' + art(feature.id) + '</div>' +
      '<div class="feature-copy">' +
        '<h3><span class="feature-icon" data-icon aria-hidden="true">' + feature.icon + '</span>' +
          '<span data-copy="feature.' + feature.id + '"></span>' +
          '<button type="button" class="infobtn" data-info="feature.' + feature.id + '.body" ' +
            'data-copy-attr="aria-label:shell.info;title:shell.info">' +
            '<span data-icon aria-hidden="true">info</span></button></h3>' +
        '<p data-copy="feature.' + feature.id + '.body"></p>' +
        '<p class="feature-where"><span data-icon aria-hidden="true">place</span>' +
          '<span class="feature-where-text"><span data-copy="features.inapp"></span>' +
          '<span class="mono">' + feature.inApp + '</span></span></p>' +
        '<a class="btn btn-tonal feature-link" href="#docs" data-article="' + feature.article + '">' +
          '<span data-icon aria-hidden="true">menu_book</span><span data-copy="features.read"></span></a>' +
      '</div>' +
    '</article>';
  }

  function wire(panel) {
    panel.addEventListener('click', function (event) {
      var link = event.target.closest('a.feature-link');
      if (link) {
        event.preventDefault();
        if (global.BambuDocs) global.BambuDocs.open(link.dataset.article);
        else if (global.BambuSiteTabs) global.BambuSiteTabs.activate('docs');
        return;
      }
      var info = event.target.closest('.infobtn');
      if (info) {
        event.preventDefault();
        site.notify('info', info.dataset.info);
      }
    });
    if (global.BambuMotion) global.BambuMotion.watch(panel);
  }

  function renderFeatures(panel) {
    panel.innerHTML =
      '<div class="section-head"><h2 data-copy="features.heading"></h2><p data-copy="features.body"></p></div>' +
      '<div class="feature-grid">' + FEATURES.map(function (feature) { return card(feature); }).join('') + '</div>';
    wire(panel);
  }

  function teaser() {
    return '<section class="feature-teaser reveal">' +
      '<div class="section-head"><h2 data-copy="overview.features.heading"></h2><p data-copy="overview.features.body"></p></div>' +
      '<div class="feature-grid compact">' +
        FEATURES.slice(0, 6).map(function (feature) { return card(feature, { compact: true }); }).join('') +
      '</div>' +
      '<p class="section-foot"><a class="btn btn-outline" href="#features" data-tab-link="features">' +
        '<span data-icon aria-hidden="true">apps</span><span data-copy="overview.features.all"></span></a></p>' +
    '</section>';
  }

  global.BambuFeatures = {
    FEATURES: FEATURES,
    art: art,
    renderFeatures: renderFeatures,
    teaser: teaser,
    wire: wire
  };
})(typeof window !== 'undefined' ? window : globalThis);
