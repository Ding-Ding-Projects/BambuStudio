import assert from 'node:assert/strict';
import test from 'node:test';

import { navigate, startChrome, stopChrome } from './devtools.mjs';

const PAGE_URL = process.env.BAMBU_PAGES_TEST_URL;
const PHYSICAL_WIDTHS = [320, 360, 420, 421, 560, 561, 640, 641, 860, 861, 1120, 1121, 1280];
const ZOOM_FACTORS = [1, 1.25, 1.5, 2];
const LANGUAGE_MODES = ['en', 'yue_HK', 'bilingual_en_yue_HK'];


async function measure(session) {
  const expression = `JSON.stringify((() => {
    const visible = element => {
      const style = getComputedStyle(element);
      const rect = element.getBoundingClientRect();
      return style.display !== 'none' && style.visibility !== 'hidden' &&
        rect.width > 0 && rect.height > 0;
    };
    const rectOf = element => {
      const rect = element.getBoundingClientRect();
      return {
        name: element.getAttribute('aria-label') ||
          element.textContent.trim().replace(/\\s+/g, ' ').slice(0, 80) ||
          element.id || element.tagName,
        left: rect.left,
        right: rect.right,
        top: rect.top,
        bottom: rect.bottom,
        width: rect.width,
        height: rect.height,
      };
    };
    const headerElements = [...document.querySelectorAll(
      'header .brand, header a, header button, header select'
    )].filter(visible).map(rectOf);
    const headerOverlaps = [];
    for (let left = 0; left < headerElements.length; left++) {
      for (let right = left + 1; right < headerElements.length; right++) {
        const a = headerElements[left];
        const b = headerElements[right];
        const overlapWidth = Math.min(a.right, b.right) - Math.max(a.left, b.left);
        const overlapHeight = Math.min(a.bottom, b.bottom) - Math.max(a.top, b.top);
        if (overlapWidth > 1 && overlapHeight > 1)
          headerOverlaps.push([a.name, b.name]);
      }
    }
    const controls = [...document.querySelectorAll(
      'header button, header select, header a'
    )].filter(visible).map(rectOf);
    const cards = [...document.querySelectorAll('.card')].map(card => {
      const outer = card.getBoundingClientRect();
      const content = card.querySelector('.card-copy') || card;
      const inner = content.getBoundingClientRect();
      return {
        outerBottom: outer.bottom,
        innerBottom: inner.bottom,
        outerRight: outer.right,
        innerRight: inner.right,
      };
    });
    const overflowOffenders = [...document.querySelectorAll('body *')]
      .filter(visible)
      .map(element => {
        const rect = element.getBoundingClientRect();
        return {
          selector: [
            element.tagName.toLowerCase(),
            element.id ? '#' + element.id : '',
            [...element.classList].map(name => '.' + name).join(''),
          ].join(''),
          left: Math.round(rect.left * 100) / 100,
          right: Math.round(rect.right * 100) / 100,
          width: Math.round(rect.width * 100) / 100,
          scrollWidth: element.scrollWidth,
          intentionallyHidden:
            element.matches('.brand-name') && rect.width <= 1.5 && rect.height <= 1.5,
        };
      })
      .filter(element =>
        !element.intentionallyHidden && (
          element.left < -1 ||
          element.right > document.documentElement.clientWidth + 1 ||
          element.scrollWidth > Math.ceil(element.width) + 1
        )
      )
      .sort((a, b) =>
        Math.max(b.right - document.documentElement.clientWidth, b.scrollWidth - b.width) -
        Math.max(a.right - document.documentElement.clientWidth, a.scrollWidth - a.width)
      )
      .slice(0, 8);
    return {
      innerWidth,
      clientWidth: document.documentElement.clientWidth,
      scrollWidth: document.documentElement.scrollWidth,
      selectedLanguage: document.getElementById('languageMode').value,
      headerElements,
      headerOverlaps,
      controls,
      overflowOffenders,
      cardOverflow: cards.filter(card =>
        card.innerBottom > card.outerBottom + 1 ||
        card.innerRight > card.outerRight + 1
      ).length,
    };
  })())`;
  const evaluated = await session.send('Runtime.evaluate', {
    expression,
    returnByValue: true,
  });
  return JSON.parse(evaluated.result.value);
}

/*
 * Every tab panel, measured in one page load per viewport.
 *
 * Panels render lazily, so a tab that is never activated is never measured;
 * activating them in-page is both faster and stricter than one navigation per
 * tab. Target-size checks cover controls (buttons, selects, inputs and the
 * anchors styled as buttons) rather than inline links inside prose, which are
 * text, not targets.
 */
async function measureEveryTab(session) {
  const expression = `JSON.stringify((() => {
    const visible = element => {
      const style = getComputedStyle(element);
      const rect = element.getBoundingClientRect();
      return style.display !== 'none' && style.visibility !== 'hidden' &&
        rect.width > 0 && rect.height > 0;
    };
    const describe = element => [
      element.tagName.toLowerCase(),
      element.id ? '#' + element.id : '',
      [...element.classList].map(name => '.' + name).join(''),
    ].join('');
    const results = [];
    const ids = window.BambuSiteTabs.ids();
    for (const id of ids) {
      window.BambuSiteTabs.activate(id);
      const panel = document.getElementById('panel-' + id);
      const clientWidth = document.documentElement.clientWidth;
      const offenders = [...panel.querySelectorAll('*')]
        .filter(visible)
        .filter(element => {
          const rect = element.getBoundingClientRect();
          return rect.left < -1 || rect.right > clientWidth + 1 ||
            element.scrollWidth > Math.ceil(rect.width) + 1;
        })
        .map(describe)
        .slice(0, 6);
      const undersized = [...panel.querySelectorAll(
        'button, select, textarea, input, a.btn, a.asset'
      )]
        .filter(visible)
        .filter(element => {
          const rect = element.getBoundingClientRect();
          if (rect.height >= 43.5 && rect.width >= 43.5) return false;
          // A checkbox inside a 44px label is reachable through the label.
          const label = element.closest('label');
          if (label) {
            const labelRect = label.getBoundingClientRect();
            if (labelRect.height >= 43.5 && labelRect.width >= 43.5) return false;
          }
          return true;
        })
        .map(element => describe(element) + ' ' +
          Math.round(element.getBoundingClientRect().width) + 'x' +
          Math.round(element.getBoundingClientRect().height))
        .slice(0, 6);
      const tabs = [...document.querySelectorAll('#tabstrip .tab')].filter(visible);
      const stripRows = [...new Set(tabs.map(tab => tab.offsetTop))].length;
      // A tab whose label is hidden by icons-only mode still has to be named:
      // its icon is aria-hidden, so without an aria-label it has no accessible
      // name at exactly the widths where one is most needed.
      const unnamedTabs = tabs
        .filter(tab => !(tab.getAttribute('aria-label') || '').trim() &&
          !tab.textContent.trim())
        .map(tab => tab.dataset.tab);
      // Every rendered string appears once: applyCopy renders the Cantonese
      // companion itself, so a primary that already contains it is a duplicate.
      const duplicated = [...panel.querySelectorAll('[data-copy]')]
        .filter(element => {
          const main = element.querySelector('.copy-main');
          const companion = element.querySelector('.copy-secondary');
          return main && companion && companion.textContent &&
            main.textContent.includes(companion.textContent);
        })
        .map(element => element.getAttribute('data-copy'))
        .slice(0, 5);
      results.push({
        id,
        empty: panel.textContent.trim().length < 40,
        offenders,
        undersized,
        stripRows,
        unnamedTabs,
        duplicated,
        documentOverflow: document.documentElement.scrollWidth > clientWidth,
      });
    }
    return results;
  })())`;
  const evaluated = await session.send('Runtime.evaluate', { expression, returnByValue: true });
  return JSON.parse(evaluated.result.value);
}

test('every tab renders inside the viewport at each supported width, zoom and language', {
  timeout: 240_000,
  skip: !PAGE_URL && 'Set BAMBU_PAGES_TEST_URL to the locally served landing page',
}, async () => {
  const chrome = await startChrome();
  const failures = [];
  let cases = 0;
  try {
    for (const physicalWidth of [320, 360, 640, 1280]) {
      for (const zoom of [1, 1.5, 2]) {
        const cssWidth = Math.max(120, Math.floor(physicalWidth / zoom));
        await chrome.session.send('Emulation.setDeviceMetricsOverride', {
          width: cssWidth,
          height: 900,
          deviceScaleFactor: zoom,
          mobile: false,
          screenWidth: physicalWidth,
          screenHeight: Math.round(900 * zoom),
        });
        for (const language of LANGUAGE_MODES) {
          const url = new URL(PAGE_URL);
          url.searchParams.set('lang', language);
          await navigate(chrome.session, url.href);
          const context = `${physicalWidth}px @ ${zoom * 100}% (${cssWidth} CSS px), ${language}`;
          for (const result of await measureEveryTab(chrome.session)) {
            cases++;
            if (result.empty)
              failures.push(`${context}: tab ${result.id} rendered no content`);
            if (result.documentOverflow)
              failures.push(`${context}: tab ${result.id} overflows the document horizontally`);
            if (result.offenders.length)
              failures.push(`${context}: tab ${result.id} clips ${JSON.stringify(result.offenders)}`);
            if (result.undersized.length)
              failures.push(`${context}: tab ${result.id} undersized ${JSON.stringify(result.undersized)}`);
            if (result.stripRows > 1)
              failures.push(`${context}: tab strip wrapped onto ${result.stripRows} rows instead of overflowing`);
            if (result.unnamedTabs.length)
              failures.push(`${context}: tabs with no accessible name ${JSON.stringify(result.unnamedTabs)}`);
            if (result.duplicated.length)
              failures.push(`${context}: tab ${result.id} renders these strings twice ${JSON.stringify(result.duplicated)}`);
          }
        }
      }
    }
  } finally {
    await stopChrome(chrome);
  }

  assert.equal(cases, 4 * 3 * 3 * 8);
  assert.deepEqual(failures, []);
});

/*
 * The prototype published at /app/ is part of the deploy, so it is measured
 * too. It was not, which is exactly how a title bar that pushed its own close
 * button 217px past a 640px viewport reached production: the gate only ever
 * loaded the landing page.
 */
test('the published prototype keeps its window controls reachable', {
  timeout: 120_000,
  skip: !PAGE_URL && 'Set BAMBU_PAGES_TEST_URL to the locally served landing page',
}, async () => {
  const appUrl = new URL('app/index.html', new URL('./', PAGE_URL)).href;
  const chrome = await startChrome();
  const failures = [];
  let cases = 0;
  try {
    for (const physicalWidth of [640, 900, 1280]) {
      for (const zoom of [1, 2]) {
        const cssWidth = Math.max(320, Math.floor(physicalWidth / zoom));
        await chrome.session.send('Emulation.setDeviceMetricsOverride', {
          width: cssWidth,
          height: 800,
          deviceScaleFactor: zoom,
          mobile: false,
          screenWidth: physicalWidth,
          screenHeight: Math.round(800 * zoom),
        });
        await navigate(chrome.session, appUrl);
        const evaluated = await chrome.session.send('Runtime.evaluate', {
          expression: `JSON.stringify((() => {
            const bar = document.querySelector('.titlebar');
            if (!bar) return { missing: true };
            const controls = [...document.querySelectorAll('.tb-controls button')];
            const clientWidth = document.documentElement.clientWidth;
            const outside = controls
              .filter(button => {
                const rect = button.getBoundingClientRect();
                return rect.width > 0 && (rect.right > clientWidth + 1 || rect.left < -1);
              })
              .map(button => button.getAttribute('aria-label') || button.textContent.trim());
            return {
              clientWidth,
              controlCount: controls.length,
              outside,
              barOverflow: bar.scrollWidth > Math.ceil(bar.getBoundingClientRect().width) + 1,
              documentOverflow: document.documentElement.scrollWidth > clientWidth,
              unnamedButtons: [...document.querySelectorAll('button')].filter(button => {
                const rect = button.getBoundingClientRect();
                if (rect.width === 0 || rect.height === 0) return false;
                return !button.textContent.trim() && !button.getAttribute('aria-label') &&
                  !button.getAttribute('title');
              }).length,
            };
          })())`,
          returnByValue: true,
        });
        const result = JSON.parse(evaluated.result.value);
        cases++;
        const context = `${physicalWidth}px @ ${zoom * 100}% (${cssWidth} CSS px)`;
        if (result.missing) {
          failures.push(`${context}: the prototype has no .titlebar`);
          continue;
        }
        if (result.controlCount < 4)
          failures.push(`${context}: expected the window controls, found ${result.controlCount}`);
        if (result.outside.length)
          failures.push(`${context}: unreachable controls ${JSON.stringify(result.outside)}`);
        if (result.barOverflow)
          failures.push(`${context}: the title bar's content is wider than the title bar`);
        if (result.documentOverflow)
          failures.push(`${context}: the prototype overflows the document horizontally`);
        if (result.unnamedButtons)
          failures.push(`${context}: ${result.unnamedButtons} button(s) with no accessible name`);
      }
    }
  } finally {
    await stopChrome(chrome);
  }
  assert.equal(cases, 6);
  assert.deepEqual(failures, []);
});

test('compact notification and dim-sum corner surfaces stack without collision', {
  timeout: 60_000,
  skip: !PAGE_URL && 'Set BAMBU_PAGES_TEST_URL to the locally served landing page',
}, async () => {
  const chrome = await startChrome();
  const failures = [];
  const widths = [420, 210, 160];
  try {
    for (const width of widths) {
      await chrome.session.send('Emulation.setDeviceMetricsOverride', {
        width,
        height: 900,
        deviceScaleFactor: 1,
        mobile: false,
        screenWidth: width,
        screenHeight: 900,
      });
      const url = new URL(PAGE_URL);
      url.searchParams.set('lang', 'bilingual_en_yue_HK');
      await navigate(chrome.session, url.href);
      const evaluated = await chrome.session.send('Runtime.evaluate', {
        expression: `JSON.stringify((() => {
          window.BambuSite.clearNotifications();
          const host = window.BambuSite.cornerSurfaceHost();
          const card = document.createElement('aside');
          card.className = 'dimsum';
          card.setAttribute('role', 'note');
          card.innerHTML =
            '<div class="dimsum-art" aria-hidden="true"></div>' +
            '<div class="dimsum-copy"><p class="dimsum-badge">Dim sum surprise</p>' +
            '<p class="dimsum-name">Classic Har Gow · 蝦餃</p>' +
            '<p class="dimsum-line">One visit in ten gets a dish. Yours is Classic Har Gow · 蝦餃.</p></div>' +
            '<button type="button" class="iconbtn dimsum-dismiss" aria-label="Dismiss">' +
            '<span data-icon aria-hidden="true">close</span></button>';
          host.appendChild(card);
          window.BambuSite.notify('warning', 'settings.storage.blocked');
          const toast = document.querySelector('.toast-host .toast');
          const rect = element => {
            const value = element.getBoundingClientRect();
            return { left: value.left, right: value.right, top: value.top, bottom: value.bottom,
              width: value.width, scrollWidth: element.scrollWidth };
          };
          const cardRect = rect(card);
          const toastRect = rect(toast);
          const overlapWidth = Math.min(cardRect.right, toastRect.right) -
            Math.max(cardRect.left, toastRect.left);
          const overlapHeight = Math.min(cardRect.bottom, toastRect.bottom) -
            Math.max(cardRect.top, toastRect.top);
          return {
            clientWidth: document.documentElement.clientWidth,
            card: cardRect,
            toast: toastRect,
            overlaps: overlapWidth > 1 && overlapHeight > 1,
          };
        })())`,
        returnByValue: true,
      });
      const result = JSON.parse(evaluated.result.value);
      for (const [name, rect] of [['card', result.card], ['toast', result.toast]]) {
        if (rect.left < -1 || rect.right > result.clientWidth + 1)
          failures.push(`${width}px: ${name} outside viewport ${JSON.stringify(rect)}`);
        if (rect.scrollWidth > Math.ceil(rect.width) + 1)
          failures.push(`${width}px: ${name} clips horizontally ${JSON.stringify(rect)}`);
      }
      if (result.overlaps) failures.push(`${width}px: toast overlaps dim-sum card`);
    }
  } finally {
    await stopChrome(chrome);
  }
  assert.deepEqual(failures, []);
});

test('landing page stays inside every supported width, zoom and language viewport', {
  timeout: 300_000,
  skip: !PAGE_URL && 'Set BAMBU_PAGES_TEST_URL to the locally served landing page',
}, async () => {
  const chrome = await startChrome();
  const failures = [];
  let cases = 0;
  try {
    for (const physicalWidth of PHYSICAL_WIDTHS) {
      for (const zoom of ZOOM_FACTORS) {
        const cssWidth = Math.max(120, Math.floor(physicalWidth / zoom));
        await chrome.session.send('Emulation.setDeviceMetricsOverride', {
          width: cssWidth,
          height: 900,
          deviceScaleFactor: zoom,
          mobile: false,
          screenWidth: physicalWidth,
          screenHeight: Math.round(900 * zoom),
        });
        for (const language of LANGUAGE_MODES) {
          const url = new URL(PAGE_URL);
          url.searchParams.set('lang', language);
          await navigate(chrome.session, url.href);
          const result = await measure(chrome.session);
          cases++;

          const context = `${physicalWidth}px @ ${zoom * 100}% (${cssWidth} CSS px), ${language}`;
          if (result.scrollWidth > result.clientWidth) {
            failures.push(
              `${context}: horizontal overflow ${result.scrollWidth}/${result.clientWidth}; ` +
              `offenders ${JSON.stringify(result.overflowOffenders)}`
            );
          }
          for (const element of result.headerElements) {
            if (element.left < -1 || element.right > result.clientWidth + 1)
              failures.push(`${context}: header element outside viewport: ${element.name}`);
          }
          if (result.headerOverlaps.length)
            failures.push(`${context}: header overlaps ${JSON.stringify(result.headerOverlaps)}`);
          for (const control of result.controls) {
            if (control.height < 43.5 || control.width < 43.5)
              failures.push(`${context}: undersized target ${control.name} ${control.width}x${control.height}`);
          }
          if (result.cardOverflow)
            failures.push(`${context}: ${result.cardOverflow} feature card(s) clip content`);
          if (result.selectedLanguage !== language)
            failures.push(`${context}: selected language is ${result.selectedLanguage}`);
        }
      }
    }
  } finally {
    await stopChrome(chrome);
  }

  assert.equal(cases, 156);
  assert.deepEqual(failures, []);
});
