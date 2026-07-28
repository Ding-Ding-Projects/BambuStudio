/*
 * Behavioural contracts for the layers the static tests do not reach:
 * preference persistence, the per-element appearance editor's storage
 * round-trip, and the notification centre.
 *
 * These run without a browser against a deliberately small DOM stub — enough
 * for the code paths under test to run, and no more. The audit that prompted
 * this file found the appearance and notification layers had no coverage at
 * all, which is how a size slider that wrote an invalid CSS value shipped.
 */
import assert from 'node:assert/strict';
import test from 'node:test';

/* --------------------------------------------------------- environment */

function memoryStorage() {
  const values = new Map();
  return {
    getItem: (key) => (values.has(key) ? values.get(key) : null),
    setItem: (key, value) => values.set(key, String(value)),
    removeItem: (key) => values.delete(key),
    size: () => values.size,
    keys: () => [...values.keys()],
  };
}

function stubNode(tag) {
  const node = {
    tagName: String(tag || 'div').toUpperCase(),
    children: [],
    attributes: new Map(),
    style: {
      setProperty(name, value) { this[name] = value; },
      removeProperty(name) { delete this[name]; },
    },
    classList: { add() {}, remove() {}, toggle() {}, contains: () => false },
    dataset: {},
    textContent: '',
    set innerHTML(value) { this._html = value; },
    get innerHTML() { return this._html || ''; },
    setAttribute(name, value) { this.attributes.set(name, String(value)); },
    getAttribute(name) { return this.attributes.has(name) ? this.attributes.get(name) : null; },
    removeAttribute(name) { this.attributes.delete(name); },
    hasAttribute(name) { return this.attributes.has(name); },
    appendChild(child) { this.children.push(child); child.parentNode = this; return child; },
    removeChild(child) { this.children = this.children.filter((c) => c !== child); },
    addEventListener() {},
    querySelectorAll: () => [],
    querySelector: () => null,
    contains: () => false,
    focus() {},
  };
  return node;
}

const storage = memoryStorage();
globalThis.window = globalThis;
globalThis.localStorage = storage;
globalThis.location = { search: '', href: 'https://example.invalid/', hostname: 'example.invalid' };
const documentElement = stubNode('html');
globalThis.document = {
  documentElement,
  body: stubNode('body'),
  createElement: (tag) => stubNode(tag),
  querySelectorAll: () => [],
  querySelector: () => null,
  addEventListener() {},
  readyState: 'complete',
};

await import('../site/copy.js');
await import('../site/dimsum.data.js');
await import('../site/core.js');

const site = globalThis.BambuSite;
const STORAGE_KEY = 'bambuStudio.site.v1';
const HISTORY_KEY = 'bambuStudio.site.notifications.v1';

/* ------------------------------------------------------------ settings */

test('a preference round-trips through storage and notifies subscribers', () => {
  const seen = [];
  const stop = site.subscribe((keys) => seen.push(...keys));
  site.set('theme', 'light');
  assert.equal(site.get('theme'), 'light');
  assert.equal(JSON.parse(storage.getItem(STORAGE_KEY)).theme, 'light');
  assert.ok(seen.includes('theme'));
  // Setting the same value again is a no-op, so subscribers are not spammed.
  const before = seen.length;
  assert.equal(site.set('theme', 'light'), false);
  assert.equal(seen.length, before);
  stop();
  site.set('theme', 'dark');
});

test('the funny sliders are independent and both persist', () => {
  site.set('funnyEn', 1);
  site.set('funnyYue', 5);
  const stored = JSON.parse(storage.getItem(STORAGE_KEY));
  assert.equal(stored.funnyEn, 1);
  assert.equal(stored.funnyYue, 5);
  // A key with a five-variant ladder must render differently at each end.
  assert.notEqual(site.preview('hero.headline', 'en', 1), site.preview('hero.headline', 'en', 5));
  assert.notEqual(site.preview('hero.headline', 'yue', 1), site.preview('hero.headline', 'yue', 5));
  site.set('funnyEn', 3);
  site.set('funnyYue', 4);
});

/* ------------------------------------------- per-element appearance */

test('an element style round-trips, applies, and resets cleanly', () => {
  site.elementStyle('cards', 'radius', '4px');
  site.elementStyle('cards', 'size', '1.2');
  assert.deepEqual(site.get('elementStyles').cards, { radius: '4px', size: '1.2' });
  assert.equal(JSON.parse(storage.getItem(STORAGE_KEY)).elementStyles.cards.radius, '4px');
  // Written out as the custom properties the stylesheet consumes.
  assert.equal(documentElement.style['--el-cards-radius'], '4px');
  assert.equal(documentElement.style['--el-cards-size'], '1.2');

  site.resetElement('cards');
  assert.equal(site.get('elementStyles').cards, undefined);
  assert.equal(documentElement.style['--el-cards-radius'], undefined);
  assert.equal(JSON.parse(storage.getItem(STORAGE_KEY)).elementStyles.cards, undefined);
});

test('the size property is stored unitless, because it is a calc multiplier', () => {
  // `calc(44px * 100%)` is invalid at computed-value time and drops the whole
  // declaration — which silently collapsed the hero headline to body size.
  site.elementStyle('hero', 'size', '1.25');
  assert.equal(documentElement.style['--el-hero-size'], '1.25');
  assert.doesNotMatch(String(documentElement.style['--el-hero-size']), /%/);
  site.resetElement('hero');
});

/* ------------------------------------------------------ notifications */

test('a notification is recorded, persisted, and re-rendered from its key', () => {
  site.clearNotifications();
  site.notify('info', 'notify.saved', { name: 'Theme' });
  const history = site.notificationHistory();
  assert.equal(history.length, 1);
  assert.equal(history[0].key, 'notify.saved');
  assert.deepEqual(history[0].params, { name: 'Theme' });
  // The record stores the key, never rendered text, so a restored history
  // re-renders in whatever language and funny level is active later.
  assert.equal(JSON.stringify(history[0]).includes('Theme'), true);
  assert.equal(JSON.parse(storage.getItem(HISTORY_KEY)).length, 1);
});

test('the history survives a reload and is capped', () => {
  site.clearNotifications();
  for (let index = 0; index < 60; index++) site.notify('info', 'notify.copied');
  assert.equal(site.notificationHistory().length, 50);
  assert.equal(JSON.parse(storage.getItem(HISTORY_KEY)).length, 50);
});

test('turning notifications off still records errors and warnings', () => {
  site.clearNotifications();
  site.set('notifications', false);
  site.notify('info', 'notify.copied');
  site.notify('error', 'regex.invalid', { message: 'boom' });
  site.notify('warning', 'settings.storage.blocked');
  const kinds = site.notificationHistory().map((record) => record.kind);
  assert.deepEqual(kinds, ['warning', 'error', 'info'],
    'every kind is recorded; the switch only silences the toast');
  site.set('notifications', true);
});

test('clearing empties both the centre and its storage', () => {
  site.notify('info', 'notify.copied');
  site.clearNotifications();
  assert.deepEqual(site.notificationHistory(), []);
  assert.deepEqual(JSON.parse(storage.getItem(HISTORY_KEY)), []);
});

/* --------------------------------------------------------------- reset */

test('resetting deletes every stored key it says it deletes', () => {
  site.set('theme', 'light');
  site.set('funnyYue', 1);
  site.elementStyle('toasts', 'radius', '2px');
  site.set('dimSum', false);
  site.set('notifications', false);
  site.set('tabGroups', { overview: 'group.tools' });
  site.notify('info', 'notify.copied');

  site.resetAll();

  assert.equal(storage.getItem(STORAGE_KEY), null);
  assert.equal(storage.getItem(HISTORY_KEY), null);
  assert.deepEqual(site.notificationHistory(), []);
  for (const [key, value] of Object.entries(site.DEFAULTS)) {
    assert.deepEqual(site.get(key), value, `${key} must be back to its default`);
  }
});

/* -------------------------------------------------------------- dimsum */

test('the dim sum odds the setting promises are the odds implemented', () => {
  assert.equal(globalThis.BAMBU_DIM_SUM.chance, 0.01);
  const description = globalThis.BAMBU_SITE_COPY.entries['settings.dimsum.desc'];
  for (const language of ['en', 'yue']) {
    for (const variant of description[language]) {
      assert.match(variant, /(one-in-a-hundred|one visit in a hundred|百分之一|一百次)/i,
        `the dim sum description (${language}) must state the real odds at every level`);
    }
  }
});
