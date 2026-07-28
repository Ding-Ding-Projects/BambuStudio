/*
 * Content panels: Overview, Screens, Material You, Download and How it is built.
 *
 * Each renderer receives its panel element once and fills it. Copy is bound with
 * data-copy so a language-mode or funny-level change re-renders text in place
 * without rebuilding the panel or losing scroll position.
 *
 * No panel uses ellipsis truncation or a horizontally scrolling container: the
 * deploy gate fails a build over an element whose content is wider than itself,
 * so long strings wrap instead of being cut off.
 */
(function (global) {
  'use strict';

  var site = global.BambuSite;
  var doc = global.document;
  var changelog = global.BAMBU_CHANGELOG || { releases: [], releaseCount: 0 };

  var APP_HREF = (function () {
    /*
     * Where the prototype lives depends on how this page was published, not on
     * what host is serving it: opened straight from the checkout the app is the
     * sibling index.html, while the composed tree puts the landing page at the
     * root and the app under /app/. compose-site.mjs stamps the answer into
     * this meta tag, so localhost previews, Pages and a custom domain all
     * resolve correctly — a hostname sniff got the local preview wrong, which
     * is exactly the case the layout gate exercises.
     */
    var meta = doc.querySelector('meta[name="bambu-app-base"]');
    var base = meta && meta.getAttribute('content');
    return base || 'index.html';
  })();

  var REPO = 'https://github.com/Ding-Ding-Projects/BambuStudio';

  var SCREENS = [
    { key: 'screen.home', icon: 'home', art: 'home.webp', alt: 'Finished geometric prints and a project folder arranged on a creative build plate.' },
    { key: 'screen.prepare', icon: 'view_in_ar', art: 'prepare.webp', alt: 'Geometric models arranged on a virtual build plate with transform guides.' },
    { key: 'screen.preview', icon: 'layers', art: 'preview.webp', alt: 'A detailed model formed from colorful sliced toolpath layers.' },
    { key: 'screen.device', icon: 'cast', art: 'device.webp', alt: 'An enclosed printer monitored by abstract status indicators.' },
    { key: 'screen.multidevice', icon: 'devices', art: 'multi-device.webp', alt: 'Four enclosed printers producing different colorful geometric forms.' },
    { key: 'screen.project', icon: 'folder', art: 'project.webp', alt: 'A 3D model surrounded by organized project pictures, parts, and assembly cards.' },
    { key: 'screen.calibration', icon: 'tune', art: 'calibration.webp', alt: 'Calibration parts and a digital caliper arranged on a build plate.' },
    { key: 'screen.filament', icon: 'palette', art: 'filament.webp', alt: 'Colorful filament spools and material samples in a modular tray.' },
    { key: 'screen.settings', icon: 'settings', art: 'settings.webp', alt: 'Layered light and dark appearance surfaces with a generated tonal color palette.' },
    { key: 'screen.smarthome', icon: 'home_iot_device', art: '', alt: '' }
  ];

  function element(html) {
    var host = doc.createElement('div');
    host.innerHTML = html;
    return host.firstElementChild;
  }

  function sectionHead(headingKey, bodyKey) {
    return '<div class="section-head">' +
      '<h2 data-copy="' + headingKey + '"></h2>' +
      '<p data-copy="' + bodyKey + '"></p>' +
      '</div>';
  }

  function formatBytes(bytes) {
    if (!bytes && bytes !== 0) return '';
    if (bytes < 1024) return bytes + ' B';
    if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + ' KB';
    return (bytes / (1024 * 1024)).toFixed(1) + ' MB';
  }

  function formatDate(iso) {
    return String(iso || '').slice(0, 10);
  }

  /* ------------------------------------------------------------ overview */

  function renderOverview(panel) {
    panel.innerHTML =
      '<div class="hero">' +
        '<div class="hero-grid">' +
          '<div class="hero-copy">' +
            '<p class="eyebrow"><span data-icon aria-hidden="true">auto_awesome</span>' +
              '<span data-copy="overview.eyebrow"></span></p>' +
            '<h1 class="hero-title" data-copy="hero.headline"></h1>' +
            '<p class="lede" data-copy="hero.lede"></p>' +
            '<div class="cta-row">' +
              '<a class="btn btn-filled" href="' + APP_HREF + '">' +
                '<span data-icon aria-hidden="true">rocket_launch</span>' +
                '<span data-copy="hero.cta.launch"></span></a>' +
              '<a class="btn btn-tonal" href="' + REPO + '/releases/latest/download/BambuStudioMD3-Setup.exe">' +
                '<span data-icon aria-hidden="true">download</span>' +
                '<span data-copy="hero.cta.download"></span></a>' +
              '<a class="btn btn-outline" href="' + REPO + '/tree/master/ui-md3" target="_blank" rel="noopener">' +
                '<span data-icon aria-hidden="true">code</span>' +
                '<span data-copy="hero.cta.source"></span></a>' +
            '</div>' +
            '<p class="hint"><span data-icon aria-hidden="true">warning</span>' +
              '<span data-copy="hero.integrity"></span> ' +
              '<a href="' + REPO + '/releases/latest/download/BambuStudioMD3-Setup.exe.sha256" data-copy="hero.checksum"></a> · ' +
              '<a href="' + REPO + '/releases/latest" data-copy="hero.releaseDetails"></a></p>' +
          '</div>' +
          '<div class="stage">' +
            '<img class="hero-art" src="./assets/showcase/hero-studio.webp" width="1536" height="1024" ' +
              'fetchpriority="high" data-copy-attr="alt:hero.art.alt">' +
            '<span class="chip c1"><span data-icon aria-hidden="true">layers</span>' +
              '<span data-copy="hero.chip.layers"></span></span>' +
            '<span class="chip c2"><span data-icon aria-hidden="true">palette</span>' +
              '<span data-copy="hero.chip.accent"></span></span>' +
            '<span class="chip c3"><span data-icon aria-hidden="true">schedule</span>' +
              '<span data-copy="hero.chip.time"></span></span>' +
          '</div>' +
        '</div>' +
      '</div>' +
      '<div class="stats">' +
        stat('overview.stat.releases', String(changelog.releaseCount || 0)) +
        stat('overview.stat.screens', String(SCREENS.length)) +
        stat('overview.stat.cases', '444') +
        stat('overview.stat.requests', '0') +
      '</div>' +
      '<div class="prose-card">' +
        '<h2 data-copy="overview.facts.heading"></h2>' +
        '<p data-copy="overview.facts.body"></p>' +
      '</div>';
  }

  function stat(labelKey, value) {
    return '<div class="stat"><span class="stat-value mono">' + value + '</span>' +
      '<span class="stat-label" data-copy="' + labelKey + '"></span></div>';
  }

  /* ------------------------------------------------------------- screens */

  function renderScreens(panel) {
    panel.innerHTML = sectionHead('screens.heading', 'screens.body') +
      '<div class="grid">' + SCREENS.map(function (screen) {
        var copy = '<div class="card-copy">' +
          '<span class="ico"><span data-icon aria-hidden="true">' + screen.icon + '</span></span>' +
          '<h3 data-copy="' + screen.key + '"></h3>' +
          '<p data-copy="' + screen.key + '.body"></p>' +
          '</div>';
        return '<article class="card' + (screen.art ? ' has-art' : '') + '">' +
          (screen.art
            ? '<img class="card-art" src="./assets/showcase/' + screen.art + '" alt="' + screen.alt +
              '" loading="lazy" decoding="async" width="900" height="900">'
            : '') +
          copy + '</article>';
      }).join('') + '</div>';
  }

  /* -------------------------------------------------------- material you */

  function renderMaterialYou(panel) {
    panel.innerHTML =
      '<div class="you">' +
        '<div class="you-copy">' +
          '<h2 data-copy="you.heading"></h2>' +
          '<p data-copy="you.body"></p>' +
          '<a class="btn btn-outline" href="' + APP_HREF + '">' +
            '<span data-icon aria-hidden="true">tune</span><span data-copy="you.try"></span></a>' +
        '</div>' +
        '<div class="you-demo">' +
          // This panel is an appearance editor, so it carries its own search
          // bar wired to the shared regex builder, like every other adjustment
          // surface on the site.
          '<div class="you-search"></div>' +
          '<div class="you-row" data-control="theme">' +
            '<p class="rowlbl" data-copy="you.theme"></p>' +
            '<div class="seg" data-group="theme">' +
              '<button type="button" data-val="light"><span data-icon aria-hidden="true">light_mode</span>' +
                '<span data-copy="you.light"></span></button>' +
              '<button type="button" data-val="dark"><span data-icon aria-hidden="true">dark_mode</span>' +
                '<span data-copy="you.dark"></span></button>' +
            '</div>' +
          '</div>' +
          '<div class="you-row" data-control="density">' +
            '<p class="rowlbl" data-copy="you.density"></p>' +
            '<div class="seg" data-group="density">' +
              '<button type="button" data-val="comfortable"><span data-copy="you.comfortable"></span></button>' +
              '<button type="button" data-val="compact"><span data-copy="you.compact"></span></button>' +
            '</div>' +
          '</div>' +
          '<div class="you-row" data-control="accent">' +
            '<p class="rowlbl" data-copy="you.accent"></p>' +
            '<div class="swatches"></div>' +
          '</div>' +
          '<p class="empty you-empty" data-copy="settings.search.empty" hidden></p>' +
        '</div>' +
      '</div>';
    global.BambuControls.mountThemeControls(panel);
    global.BambuControls.mountAppearanceSearch(panel);
  }

  /* ------------------------------------------------------------ download */

  function renderDownload(panel) {
    var latest = changelog.releases[0];
    panel.innerHTML = sectionHead('download.heading', 'download.body') +
      (latest ? releaseCard(latest) : '') +
      '<div class="prose-card">' +
        '<h3 data-copy="download.verify.heading"></h3>' +
        '<p data-copy="download.verify.body"></p>' +
        '<pre class="cmd mono"><code>certutil -hashfile BambuStudioMD3-Setup.exe SHA256</code></pre>' +
        '<pre class="cmd mono"><code>gh attestation verify BambuStudioMD3-Setup.exe --repo Ding-Ding-Projects/BambuStudio</code></pre>' +
        '<p class="hint"><span data-icon aria-hidden="true">warning</span>' +
          '<span data-copy="hero.integrity"></span></p>' +
      '</div>' +
      '<p class="section-foot"><a href="' + REPO + '/releases" target="_blank" rel="noopener" ' +
        'data-copy="download.allreleases"></a></p>';
  }

  function releaseCard(release) {
    return '<article class="release-card">' +
      '<header class="release-head">' +
        '<span class="badge" data-copy="download.latest"></span>' +
        '<h3 class="release-title">' + escapeHtml(release.name) + '</h3>' +
      '</header>' +
      '<dl class="factlist">' +
        fact('download.published', formatDate(release.published)) +
        fact('download.commit', release.commit ? release.commit.slice(0, 9) : '—', release.commit ? REPO + '/commit/' + release.commit : '') +
        (release.workflow ? fact('download.workflow', 'Actions run', release.workflow) : '') +
      '</dl>' +
      '<p class="rowlbl" data-copy="download.assets"></p>' +
      '<ul class="assetlist">' + (release.assets || []).map(function (asset) {
        return '<li><a class="asset" href="' + asset.url + '">' +
          '<span data-icon aria-hidden="true">description</span>' +
          '<span class="asset-name mono">' + escapeHtml(asset.name) + '</span>' +
          '<span class="asset-size mono">' + formatBytes(asset.bytes) + '</span></a></li>';
      }).join('') + '</ul>' +
      '</article>';
  }

  function fact(labelKey, value, href) {
    return '<div class="fact"><dt data-copy="' + labelKey + '"></dt>' +
      '<dd class="mono">' + (href
        ? '<a href="' + href + '" target="_blank" rel="noopener">' + escapeHtml(value) + '</a>'
        : escapeHtml(value)) + '</dd></div>';
  }

  /* --------------------------------------------------------------- build */

  function renderBuild(panel) {
    var steps = [1, 2, 3, 4].map(function (index) {
      return '<article class="step">' +
        '<p class="n mono">0' + index + '</p>' +
        '<h3 data-copy="build.step' + index + '.title"></h3>' +
        '<p data-copy="build.step' + index + '.body"></p>' +
        '</article>';
    }).join('');
    panel.innerHTML = sectionHead('build.heading', 'build.body') + '<div class="steps">' + steps + '</div>';
  }

  function escapeHtml(value) {
    return String(value == null ? '' : value).replace(/[&<>"']/g, function (character) {
      return { '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[character];
    });
  }

  global.BambuViews = {
    SCREENS: SCREENS,
    APP_HREF: APP_HREF,
    REPO: REPO,
    escapeHtml: escapeHtml,
    formatBytes: formatBytes,
    formatDate: formatDate,
    element: element,
    sectionHead: sectionHead,
    renderOverview: renderOverview,
    renderScreens: renderScreens,
    renderMaterialYou: renderMaterialYou,
    renderDownload: renderDownload,
    renderBuild: renderBuild
  };
})(typeof window !== 'undefined' ? window : globalThis);
