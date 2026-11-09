/*
 * The Docs tab: every feature article and every wiki page, inside the site.
 *
 * The bundle (site/docs.data.js, generated at compose time from docs/features
 * and the wiki checkout) is loaded the first time anything needs it: the tab,
 * a feature card, the command palette or a ?article= link. The markdown is
 * rendered here with a small, escaping converter: headings, paragraphs, lists,
 * tables, code, emphasis and links. Links between articles stay inside the
 * reader; anything else opens in a new tab.
 *
 * The search is the shared search field with the regex builder beside it, over
 * title, summary and body in both languages. No panel truncates or scrolls
 * sideways: wide tables wrap their cells.
 */
(function (global) {
  'use strict';

  var site = global.BambuSite;
  var doc = global.document;

  var TASKS = [
    { key: 'docs.task.printer', article: 'windows-practical-setup' },
    { key: 'docs.task.lock', article: 'windows-element-locks' },
    { key: 'docs.task.chart', article: 'device-live-charts' },
    { key: 'docs.task.extension', article: 'windows-extensions' },
    { key: 'docs.task.cantonese', article: 'windows-language-modes' },
    { key: 'docs.task.update', article: 'windows-self-update' }
  ];

  var bundle = null;
  var loading = null;
  var panelRef = null;
  var current = null;
  var language = 'en';
  var pendingArticle = null;
  var speaking = false;

  /* ------------------------------------------------------------- loading */

  function load() {
    if (bundle) return Promise.resolve(bundle);
    if (global.BAMBU_DOCS) { bundle = global.BAMBU_DOCS; return Promise.resolve(bundle); }
    if (loading) return loading;
    loading = new Promise(function (resolve) {
      var script = doc.createElement('script');
      script.src = './site/docs.data.js';
      script.async = true;
      script.onload = function () { bundle = global.BAMBU_DOCS || null; resolve(bundle); };
      script.onerror = function () { bundle = null; resolve(null); };
      doc.head.appendChild(script);
    });
    return loading;
  }

  function find(id) {
    if (!bundle) return null;
    for (var i = 0; i < bundle.articles.length; i++) if (bundle.articles[i].id === id) return bundle.articles[i];
    return null;
  }

  /* ------------------------------------------------------------ markdown */

  function escapeHtml(text) {
    return String(text).replace(/[&<>"']/g, function (c) {
      return { '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c];
    });
  }

  function inline(text) {
    var out = escapeHtml(text);
    out = out.replace(/`([^`]+)`/g, function (m, code) { return '<code class="mono">' + code + '</code>'; });
    out = out.replace(/\*\*([^*]+)\*\*/g, '<strong>$1</strong>');
    out = out.replace(/(^|[^*\w])\*([^*\n]+)\*(?!\w)/g, '$1<em>$2</em>');
    out = out.replace(/\[([^\]]+)\]\(([^)\s]+)\)/g, function (m, label, href) {
      var target = resolveLink(href);
      if (target.article) return '<a href="#docs" data-article="' + target.article + '">' + label + '</a>';
      return '<a href="' + escapeHtml(target.href) + '" target="_blank" rel="noopener">' + label + '</a>';
    });
    return out;
  }

  // A relative link to another article becomes a reader link; any other
  // relative path points at the repository, absolute links stay as they are.
  function resolveLink(href) {
    if (/^(https?:|mailto:)/i.test(href)) return { href: href };
    if (/^#/.test(href)) return { href: href };
    var clean = href.replace(/^(\.\.\/|\.\/)+/, '').replace(/#.*$/, '');
    if (/\.md$/i.test(clean) && bundle) {
      var wanted = clean.replace(/\.yue_HK\.md$/, '.md');
      for (var i = 0; i < bundle.articles.length; i++) {
        var p = bundle.articles[i].path;
        if (p.endsWith('/' + wanted) || p === wanted || p === 'docs/features/' + wanted) return { article: bundle.articles[i].id };
      }
    }
    var base = current && current.path ? current.path.replace(/\/[^/]*$/, '/') : 'docs/features/';
    return { href: 'https://github.com/Ding-Ding-Projects/BambuStudio/blob/master/' + base + clean.replace(/^\/+/, '') };
  }

  function render(markdown) {
    var lines = markdown.split('\n');
    var html = [];
    var i = 0;
    function flushParagraph(buffer) {
      if (buffer.length) html.push('<p>' + inline(buffer.join(' ')) + '</p>');
      buffer.length = 0;
    }
    var paragraph = [];
    while (i < lines.length) {
      var line = lines[i];
      var fence = /^```/.exec(line);
      if (fence) {
        flushParagraph(paragraph);
        var code = [];
        i++;
        while (i < lines.length && !/^```/.test(lines[i])) code.push(lines[i++]);
        i++;
        html.push('<pre class="cmd mono"><code>' + escapeHtml(code.join('\n')) + '</code></pre>');
        continue;
      }
      var heading = /^(#{1,6})\s+(.*)$/.exec(line);
      if (heading) {
        flushParagraph(paragraph);
        var level = Math.min(4, heading[1].length + 1);
        html.push('<h' + level + '>' + inline(heading[2]) + '</h' + level + '>');
        i++;
        continue;
      }
      if (/^\s*[-*]\s+/.test(line) || /^\s*\d+\.\s+/.test(line)) {
        flushParagraph(paragraph);
        var ordered = /^\s*\d+\./.test(line);
        var items = [];
        while (i < lines.length && (/^\s*[-*]\s+/.test(lines[i]) || /^\s*\d+\.\s+/.test(lines[i]) || /^\s{2,}\S/.test(lines[i]))) {
          if (/^\s*([-*]|\d+\.)\s+/.test(lines[i])) items.push(lines[i].replace(/^\s*([-*]|\d+\.)\s+/, ''));
          else items[items.length - 1] += ' ' + lines[i].trim();
          i++;
        }
        html.push('<' + (ordered ? 'ol' : 'ul') + '>' + items.map(function (item) { return '<li>' + inline(item) + '</li>'; }).join('') + '</' + (ordered ? 'ol' : 'ul') + '>');
        continue;
      }
      if (/^\s*\|/.test(line)) {
        flushParagraph(paragraph);
        var rows = [];
        while (i < lines.length && /^\s*\|/.test(lines[i])) rows.push(lines[i++]);
        var cells = function (row) {
          return row.trim().replace(/^\||\|$/g, '').split('|').map(function (cell) { return cell.trim(); });
        };
        var head = cells(rows[0]);
        var body = rows.slice(2).map(cells);
        html.push('<table class="doc-table"><thead><tr>' + head.map(function (c) { return '<th>' + inline(c) + '</th>'; }).join('') +
          '</tr></thead><tbody>' + body.map(function (row) {
            return '<tr>' + row.map(function (c) { return '<td>' + inline(c) + '</td>'; }).join('') + '</tr>';
          }).join('') + '</tbody></table>');
        continue;
      }
      if (/^\s*>/.test(line)) {
        flushParagraph(paragraph);
        var quote = [];
        while (i < lines.length && /^\s*>/.test(lines[i])) quote.push(lines[i++].replace(/^\s*>\s?/, ''));
        html.push('<blockquote>' + inline(quote.join(' ')) + '</blockquote>');
        continue;
      }
      if (!line.trim()) { flushParagraph(paragraph); i++; continue; }
      if (/^\s*<!--/.test(line)) { i++; continue; }
      paragraph.push(line.trim());
      i++;
    }
    flushParagraph(paragraph);
    return html.join('');
  }

  /* --------------------------------------------------------------- panel */

  function renderDocs(panel) {
    panelRef = panel;
    panel.innerHTML =
      '<div class="section-head"><h2 data-copy="docs.heading"></h2><p data-copy="docs.body"></p></div>' +
      '<div class="docs-search"></div>' +
      '<div class="docs-layout">' +
        '<aside class="docs-side">' +
          '<p class="rowlbl" data-copy="docs.tasks"></p>' +
          '<div class="docs-tasks">' + TASKS.map(function (task) {
            return '<button type="button" class="docs-task" data-article="' + task.article + '" data-copy="' + task.key + '"></button>';
          }).join('') + '</div>' +
          '<p class="rowlbl" data-copy="docs.areas"></p>' +
          '<div class="docs-areas"></div>' +
        '</aside>' +
        '<div class="docs-main">' +
          '<p class="docs-count" role="status"></p>' +
          '<div class="docs-list"></div>' +
          '<article class="docs-article" hidden></article>' +
        '</div>' +
      '</div>';

    var listHost = panel.querySelector('.docs-list');
    var countLine = panel.querySelector('.docs-count');
    var areasHost = panel.querySelector('.docs-areas');
    var article = panel.querySelector('.docs-article');
    var area = '';
    var matched = null;

    var field = global.BambuRegex.createSearchField({
      labelKey: 'docs.search',
      sampleProvider: function () {
        return (bundle ? bundle.articles : []).slice(0, 40).map(function (a) { return a.title + ' ' + a.summary; }).join('\n');
      },
      items: function () {
        return (bundle ? bundle.articles : []).map(function (a) {
          return { id: a.id, text: a.title + '\n' + a.titleYue + '\n' + a.summary + '\n' + a.summaryYue + '\n' + a.en + '\n' + a.yue };
        });
      },
      onResults: function (ids) { matched = ids; paintList(); }
    });
    panel.querySelector('.docs-search').appendChild(field.element);

    function visibleArticles() {
      if (!bundle) return [];
      return bundle.articles.filter(function (a) {
        if (area && a.area !== area) return false;
        return matched === null || matched.indexOf(a.id) !== -1;
      });
    }

    function paintAreas() {
      if (!bundle) return;
      var names = Object.keys(bundle.areas).sort(function (a, b) { return a === 'wiki' ? 1 : b === 'wiki' ? -1 : a.localeCompare(b); });
      areasHost.innerHTML = '<button type="button" class="docs-area' + (area ? '' : ' active') + '" data-area="" aria-pressed="' + (!area) + '">' +
        '<span data-copy="tab.docs"></span> <span class="mono">' + bundle.articles.length + '</span></button>' +
        names.map(function (name) {
          return '<button type="button" class="docs-area' + (area === name ? ' active' : '') + '" data-area="' + name + '" aria-pressed="' + (area === name) + '">' +
            (name === 'wiki' ? '<span data-copy="docs.area.wiki"></span>' : '<span>' + escapeHtml(name.replace(/-/g, ' ')) + '</span>') +
            ' <span class="mono">' + bundle.areas[name] + '</span></button>';
        }).join('');
      site.applyCopy(areasHost);
    }

    function paintList() {
      if (!bundle) {
        listHost.innerHTML = '<p class="empty" data-copy="docs.unavailable"></p>';
        countLine.textContent = '';
        site.applyCopy(listHost);
        return;
      }
      var items = visibleArticles();
      site.setCopy(countLine, 'docs.results', { n: items.length });
      listHost.innerHTML = items.length
        ? items.map(function (a) {
          var yue = language === 'yue' && a.titleYue;
          return '<button type="button" class="docs-item reveal" data-article="' + a.id + '" aria-pressed="' + (current && current.id === a.id) + '">' +
            '<span class="docs-item-area mono">' + escapeHtml(a.area) + '</span>' +
            '<span class="docs-item-title">' + escapeHtml(yue ? a.titleYue : a.title) + '</span>' +
            '<span class="docs-item-summary">' + escapeHtml(yue && a.summaryYue ? a.summaryYue : a.summary) + '</span>' +
          '</button>';
        }).join('')
        : '<p class="empty" data-copy="docs.empty"></p>';
      site.applyCopy(listHost);
      if (global.BambuMotion) global.BambuMotion.watch(listHost);
    }

    function paintArticle() {
      if (!current) {
        article.hidden = true;
        listHost.hidden = false;
        countLine.hidden = false;
        return;
      }
      var hasYue = !!current.yue;
      var showYue = language === 'yue' && hasYue;
      var markdown = showYue ? current.yue : current.en;
      article.hidden = false;
      article.lang = showYue ? 'yue-Hant-HK' : 'en';
      article.innerHTML =
        '<div class="docs-toolbar">' +
          '<button type="button" class="btn btn-outline docs-back"><span data-icon aria-hidden="true">arrow_back</span><span data-copy="tab.docs"></span></button>' +
          '<span class="docs-item-area mono">' + escapeHtml(current.area) + '</span>' +
          '<span class="spacer"></span>' +
          (hasYue
            ? '<button type="button" class="btn btn-tonal docs-lang" data-copy="' + (showYue ? 'docs.readEn' : 'docs.readYue') + '"></button>'
            : '<span class="hint docs-notwin" data-copy="docs.noTwin"></span>') +
          '<button type="button" class="btn btn-outline docs-aloud" aria-pressed="false" data-copy="docs.readAloud"></button>' +
          '<button type="button" class="btn btn-outline docs-copylink" data-copy="docs.copyLink"></button>' +
          '<a class="btn btn-outline" href="' + escapeHtml(current.source) + '" target="_blank" rel="noopener" data-copy="docs.source"></a>' +
        '</div>' +
        '<div class="docs-body prose-card">' + render(markdown) + '</div>';
      site.applyCopy(article);
      listHost.hidden = true;
      countLine.hidden = true;
      article.scrollIntoView({ block: 'start', behavior: global.BambuMotion && global.BambuMotion.reduced() ? 'auto' : 'smooth' });
    }

    function show(id) {
      var found = find(id);
      if (!found) return false;
      current = found;
      try {
        var url = new URL(global.location.href);
        url.searchParams.set('article', id);
        global.history.replaceState(null, '', url.toString());
      } catch (error) { /* file:// history writes are not essential */ }
      paintArticle();
      return true;
    }

    panel.addEventListener('click', function (event) {
      var areaButton = event.target.closest('.docs-area');
      if (areaButton) { area = areaButton.dataset.area || ''; current = null; stopAloud(); paintAreas(); paintList(); paintArticle(); return; }
      var open = event.target.closest('[data-article]');
      if (open) { event.preventDefault(); stopAloud(); show(open.dataset.article); return; }
      if (event.target.closest('.docs-back')) { current = null; stopAloud(); paintArticle(); paintList(); return; }
      if (event.target.closest('.docs-lang')) { language = language === 'yue' ? 'en' : 'yue'; stopAloud(); paintArticle(); return; }
      if (event.target.closest('.docs-copylink')) {
        var url;
        try { url = new URL(global.location.href); url.hash = 'docs'; url.searchParams.set('article', current.id); url = url.toString(); } catch (error) { url = global.location.href; }
        site.copyText(url).then(function () { site.notify('info', 'notify.docs.linkcopied'); });
        return;
      }
      var aloud = event.target.closest('.docs-aloud');
      if (aloud) { if (speaking) stopAloud(); else startAloud(aloud); }
    });

    function startAloud(button) {
      if (!global.speechSynthesis || !current) return;
      var text = article.querySelector('.docs-body').textContent;
      var utterance = new global.SpeechSynthesisUtterance(text.slice(0, 12000));
      utterance.lang = article.lang === 'en' ? 'en-US' : 'zh-HK';
      utterance.rate = Number(site.get('narratorRate')) || 1;
      utterance.pitch = Number(site.get('narratorPitch')) || 1;
      utterance.onend = stopAloud;
      speaking = true;
      button.setAttribute('aria-pressed', 'true');
      site.setCopy(button, 'docs.stopAloud');
      global.speechSynthesis.cancel();
      global.speechSynthesis.speak(utterance);
    }
    function stopAloud() {
      if (global.speechSynthesis && speaking) global.speechSynthesis.cancel();
      speaking = false;
      var button = article.querySelector('.docs-aloud');
      if (button) { button.setAttribute('aria-pressed', 'false'); site.setCopy(button, 'docs.readAloud'); }
    }

    site.subscribe(function (keys) {
      if (keys.indexOf('languageMode') !== -1 || keys.indexOf('funnyEn') !== -1) {
        language = site.languageMode() === 'yue_HK' ? 'yue' : language;
      }
    });
    language = site.languageMode() === 'yue_HK' ? 'yue' : 'en';

    listHost.innerHTML = '<p class="empty" data-copy="docs.loading"></p>';
    site.applyCopy(listHost);
    load().then(function () {
      paintAreas();
      paintList();
      var wanted = pendingArticle;
      pendingArticle = null;
      if (!wanted) {
        try { wanted = new URLSearchParams(global.location.search).get('article'); } catch (error) { wanted = null; }
      }
      if (wanted && show(wanted)) return;
      if (!bundle) return;
      article.hidden = true;
    });
  }

  // Opens the Docs tab on an article; the panel renders first if it has not.
  function open(id) {
    pendingArticle = id;
    if (global.BambuSiteTabs) global.BambuSiteTabs.activate('docs', { focus: false });
    if (panelRef && bundle && pendingArticle) {
      var button = panelRef.querySelector('[data-article="' + id + '"]');
      pendingArticle = null;
      if (button) button.click();
      else {
        var found = find(id);
        if (found) { current = found; panelRef.querySelector('.docs-list').hidden = true; }
        panelRef.dispatchEvent(new global.CustomEvent('docs-open', { detail: id }));
      }
    }
  }

  global.BambuDocs = {
    TASKS: TASKS,
    load: load,
    find: find,
    render: render,
    renderDocs: renderDocs,
    open: open,
    articles: function () { return bundle ? bundle.articles : []; }
  };
})(typeof window !== 'undefined' ? window : globalThis);
