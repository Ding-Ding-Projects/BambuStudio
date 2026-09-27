import assert from 'node:assert/strict';
import test from 'node:test';

globalThis.window = globalThis;
await import('../site/dimsum.data.js');
await import('../site/dimsum.js');

const catalogue = globalThis.BAMBU_DIM_SUM;
const dish = catalogue.dishes[0];

function emitter(target = {}) {
  const listeners = new Map();
  target.addEventListener = (type, listener, options) => {
    const entries = listeners.get(type) || [];
    entries.push({ listener, once: Boolean(options && options.once) });
    listeners.set(type, entries);
  };
  target.removeEventListener = (type, listener) => {
    listeners.set(type, (listeners.get(type) || []).filter((entry) => entry.listener !== listener));
  };
  target.dispatch = (type, properties = {}) => {
    const event = { type, target, currentTarget: target, relatedTarget: null, ...properties };
    for (const entry of [...(listeners.get(type) || [])]) {
      entry.listener(event);
      if (entry.once) target.removeEventListener(type, entry.listener);
    }
    return event;
  };
  return target;
}

function clock() {
  let now = 0;
  let nextId = 1;
  const tasks = new Map();
  return {
    setTimeout(callback, delay) {
      const id = nextId++;
      tasks.set(id, { at: now + delay, callback });
      return id;
    },
    clearTimeout(id) { tasks.delete(id); },
    tick(duration) {
      const target = now + duration;
      while (true) {
        const due = [...tasks.entries()]
          .filter(([, task]) => task.at <= target)
          .sort((left, right) => left[1].at - right[1].at || left[0] - right[0])[0];
        if (!due) break;
        tasks.delete(due[0]);
        now = due[1].at;
        due[1].callback();
      }
      now = target;
    },
  };
}

function storage(seen) {
  const values = new Map(seen ? [['bambuStudio.site.seen', '1']] : []);
  return {
    getItem: (key) => values.has(key) ? values.get(key) : null,
    setItem: (key, value) => values.set(key, String(value)),
    value: (key) => values.get(key),
  };
}

function descendants(node) {
  return [node, ...node.children.flatMap(descendants)];
}

function environment({ seen = true, randomValues = [0, 0], decode = () => Promise.resolve() } = {}) {
  const timer = clock();
  const navigation = emitter();
  const images = [];
  let document;

  function node(tag) {
    const attributes = new Map();
    const element = emitter({
      tagName: String(tag).toUpperCase(),
      className: '',
      children: [],
      parentNode: null,
      textContent: '',
      naturalWidth: 0,
      style: {},
      setAttribute(name, value) { attributes.set(name, String(value)); },
      getAttribute(name) { return attributes.has(name) ? attributes.get(name) : null; },
      hasAttribute(name) { return attributes.has(name); },
      removeAttribute(name) {
        attributes.delete(name);
        if (name === 'src') this._src = '';
      },
      appendChild(child) {
        this.children.push(child);
        child.parentNode = this;
        return child;
      },
      removeChild(child) {
        this.children = this.children.filter((candidate) => candidate !== child);
        child.parentNode = null;
        return child;
      },
      contains(candidate) {
        return Boolean(candidate) && descendants(this).includes(candidate);
      },
      querySelector(selector) {
        if (!selector.startsWith('.')) return null;
        const className = selector.slice(1);
        return descendants(this).find((candidate) =>
          String(candidate.className).split(/\s+/).includes(className)) || null;
      },
      querySelectorAll(selector) {
        if (selector === '[data-copy]')
          return descendants(this).filter((candidate) => candidate.hasAttribute('data-copy'));
        if (selector === '[data-copy-attr]')
          return descendants(this).filter((candidate) => candidate.hasAttribute('data-copy-attr'));
        return [];
      },
      focus() {
        const previous = document.activeElement;
        if (previous === this) return;
        document.activeElement = this;
        for (let current = previous; current; current = current.parentNode)
          current.dispatch('focusout', { target: previous, relatedTarget: this });
        for (let current = this; current; current = current.parentNode)
          current.dispatch('focusin', { target: this, relatedTarget: previous });
      },
    });
    element.classList = {
      contains: (name) => String(element.className).split(/\s+/).includes(name),
    };
    Object.defineProperty(element, 'src', {
      get() { return this._src || ''; },
      set(value) { this._src = String(value); attributes.set('src', this._src); },
    });
    if (String(tag).toLowerCase() === 'img') {
      element.decode = decode;
      images.push(element);
    }
    return element;
  }

  const body = node('body');
  const activeTab = node('button');
  activeTab.id = 'tab-overview';
  activeTab.setAttribute('role', 'tab');
  activeTab.setAttribute('aria-selected', 'true');
  body.appendChild(activeTab);
  const corner = node('div');
  corner.className = 'corner-surfaces';
  body.appendChild(corner);

  document = emitter({
    body,
    activeElement: body,
    hidden: false,
    createElement: node,
    querySelector(selector) {
      if (selector === '[role="tab"][aria-selected="true"]') return activeTab;
      if (selector === '.dimsum') return body.querySelector(selector);
      return null;
    },
    getElementById(id) {
      return descendants(body).find((candidate) => candidate.id === id) || null;
    },
  });

  let mode = 'en';
  let applyCount = 0;
  let subscribers = [];
  const site = {
    languageMode: () => mode,
    subscribe(listener) {
      subscribers.push(listener);
      return () => { subscribers = subscribers.filter((candidate) => candidate !== listener); };
    },
    applyCopy(root) {
      applyCount++;
      for (const candidate of root.querySelectorAll('[data-copy-attr]')) {
        if (candidate.getAttribute('data-copy-attr') === 'aria-label:dimsum.dismiss')
          candidate.setAttribute('aria-label', mode === 'yue_HK' ? '關閉' : 'Dismiss');
      }
    },
    cornerSurfaceHost: () => corner,
    setMode(nextMode) {
      mode = nextMode;
      for (const listener of [...subscribers]) listener(['languageMode']);
    },
    emitFunny() {
      for (const listener of [...subscribers]) listener(['funnyEn']);
    },
    activateTab() {
      for (const listener of [...subscribers]) listener(['activeTab']);
    },
    applyCount: () => applyCount,
  };

  let randomIndex = 0;
  let randomCalls = 0;
  const random = () => {
    randomCalls++;
    return randomValues[Math.min(randomIndex++, randomValues.length - 1)];
  };
  const localStorage = storage(seen);
  const controller = globalThis.BambuDimSum.createController({
    document,
    site,
    storage: localStorage,
    random,
    setTimeout: timer.setTimeout,
    clearTimeout: timer.clearTimeout,
    navigationTarget: navigation,
  });

  return {
    controller, document, site, localStorage, timer, navigation, images, activeTab,
    randomCalls: () => randomCalls,
  };
}

async function loaded(env, image = env.images.at(-1)) {
  image.naturalWidth = 1254;
  image.dispatch('load');
  await Promise.resolve();
  await Promise.resolve();
}

function byClass(root, className) {
  return descendants(root).find((candidate) =>
    String(candidate.className).split(/\s+/).includes(className));
}

test('first visit is recorded without a draw and cannot be retried in the same launch', async () => {
  const env = environment({ seen: false });
  assert.equal(await env.controller.maybeStart(catalogue), false);
  assert.equal(env.localStorage.value('bambuStudio.site.seen'), '1');
  assert.equal(env.randomCalls(), 0);
  assert.equal(env.images.length, 0);
  assert.equal(await env.controller.maybeStart(catalogue), false);
  assert.equal(env.randomCalls(), 0);

  const hidden = environment();
  hidden.document.hidden = true;
  assert.equal(await hidden.controller.maybeStart(catalogue), false);
  assert.equal(hidden.randomCalls(), 0);

  const blocked = environment();
  blocked.localStorage.getItem = () => { throw new Error('storage blocked'); };
  assert.equal(await blocked.controller.maybeStart(catalogue), false);
  assert.equal(blocked.randomCalls(), 0);
});

test('eligible startup performs one chance draw, one selection draw and never doubles', async () => {
  const env = environment({ randomValues: [0.099, 0.6] });
  const pending = env.controller.maybeStart(catalogue);
  assert.equal(env.randomCalls(), 2);
  assert.equal(env.controller.active(), true);
  assert.equal(await env.controller.maybeStart(catalogue), false);
  assert.equal(await env.controller.render(catalogue.dishes[1]), false);
  assert.equal(env.randomCalls(), 2);
  await loaded(env);
  assert.equal(await pending, true);
  assert.equal(descendants(env.document.body).filter((node) => node.className === 'dimsum').length, 1);

  const missed = environment({ randomValues: [0.10] });
  assert.equal(await missed.controller.maybeStart(catalogue), false);
  assert.equal(missed.randomCalls(), 1);
  assert.equal(missed.images.length, 0);
});

test('pending photo is permanently cancelled by user, navigation and visibility activity', async (t) => {
  const cases = [
    ['pointer input', (env) => env.document.dispatch('pointerdown')],
    ['pointer movement', (env) => env.document.dispatch('pointermove')],
    ['wheel movement', (env) => env.document.dispatch('wheel')],
    ['keyboard input', (env) => env.document.dispatch('keydown')],
    ['text input', (env) => env.document.dispatch('input')],
    ['form change', (env) => env.document.dispatch('change')],
    ['history navigation', (env) => env.navigation.dispatch('popstate')],
    ['hash navigation', (env) => env.navigation.dispatch('hashchange')],
    ['tab navigation', (env) => env.site.activateTab()],
    ['page exit', (env) => env.navigation.dispatch('pagehide')],
    ['hidden page', (env) => {
      env.document.hidden = true;
      env.document.dispatch('visibilitychange');
    }],
  ];
  for (const [name, act] of cases) {
    await t.test(name, async () => {
      const env = environment({ randomValues: [0, 0] });
      const pending = env.controller.maybeStart(catalogue);
      const image = env.images[0];
      act(env);
      assert.equal(await pending, false);
      assert.equal(image.src, '');
      await loaded(env, image);
      assert.equal(env.document.querySelector('.dimsum'), null);
    });
  }
});

test('load, error, decode rejection and deadline races settle once', async () => {
  const success = environment();
  const shown = success.controller.render(dish);
  await loaded(success);
  assert.equal(await shown, true);
  success.timer.tick(globalThis.BambuDimSum.loadDeadlineMs);
  assert.ok(success.document.querySelector('.dimsum'));

  const failed = environment();
  const errored = failed.controller.render(dish);
  failed.images[0].dispatch('error');
  assert.equal(await errored, false);
  await loaded(failed);
  assert.equal(failed.document.querySelector('.dimsum'), null);

  const timedOut = environment();
  const late = timedOut.controller.render(dish);
  timedOut.timer.tick(globalThis.BambuDimSum.loadDeadlineMs);
  assert.equal(await late, false);
  await loaded(timedOut);
  assert.equal(timedOut.document.querySelector('.dimsum'), null);

  const corrupt = environment({ decode: () => Promise.reject(new Error('decode failed')) });
  const undecodable = corrupt.controller.render(dish);
  await loaded(corrupt);
  assert.equal(await undecodable, false);
  assert.equal(corrupt.document.querySelector('.dimsum'), null);
});

test('dish name, alt text and sentence parameters follow live language changes', async () => {
  const env = environment();
  const pending = env.controller.render(dish);
  env.site.setMode('yue_HK');
  await loaded(env);
  assert.equal(await pending, true);
  const card = env.document.querySelector('.dimsum');
  const image = byClass(card, 'dimsum-art');
  const name = byClass(card, 'dimsum-name');
  const line = byClass(card, 'dimsum-line');
  assert.ok(name.textContent.startsWith(`${dish.yue} · ${dish.en}`));
  assert.ok(image.alt.startsWith(`${dish.altYue} / ${dish.altEn}`));
  assert.equal(JSON.parse(line.getAttribute('data-copy-params')).dish, `${dish.yue} · ${dish.en}`);

  env.site.setMode('en');
  assert.ok(name.textContent.startsWith(`${dish.en} · ${dish.yue}`));
  assert.ok(image.alt.startsWith(`${dish.altEn} / ${dish.altYue}`));
  const beforeFunny = env.site.applyCount();
  env.site.emitFunny();
  assert.ok(env.site.applyCount() > beforeFunny);

  byClass(card, 'dimsum-dismiss').dispatch('click');
  const afterDismiss = env.site.applyCount();
  env.site.setMode('yue_HK');
  assert.equal(env.site.applyCount(), afterDismiss, 'dismissal must remove the language subscription');
});

test('only the exact published catalog release URL shape is accepted', async () => {
  const invalidUrls = [
    'https://github.com/another-owner/dim-sum-photos/releases/download/catalog-v1/' + dish.photo.file,
    'https://github.com/Ding-Ding-Projects/dim-sum-photos/releases/download/catalog-v2/' + dish.photo.file,
    dish.photo.url + '?mutable=1',
    dish.photo.url + '#mutable',
    'http://github.com/Ding-Ding-Projects/dim-sum-photos/releases/download/catalog-v1/' + dish.photo.file,
  ];
  for (const url of invalidUrls) {
    const env = environment();
    assert.equal(await env.controller.render({ ...dish, photo: { ...dish.photo, url } }), false);
    assert.equal(env.images.length, 0);
  }

  const altered = [
    { ...dish, photo: { ...dish.photo, sha256: '0'.repeat(64) } },
    { ...dish, photo: {
      ...dish.photo,
      file: 'hk-dish-0001-not-the-catalog-file.png',
      url: globalThis.BambuDimSum.releaseRoot + 'hk-dish-0001-not-the-catalog-file.png',
    } },
  ];
  for (const candidate of altered) {
    const env = environment();
    assert.equal(await env.controller.render(candidate), false);
    assert.equal(env.images.length, 0);
  }
});

test('hover and focus pause expiry, and focused dismissal returns to the active tab', async () => {
  const focused = environment();
  await loaded(focused, focused.images[0] || (() => {
    focused.controller.render(dish);
    return focused.images[0];
  })());
  const focusedCard = focused.document.querySelector('.dimsum');
  const close = byClass(focusedCard, 'dimsum-dismiss');
  close.focus();
  focused.timer.tick(globalThis.BambuDimSum.dismissDelayMs * 2);
  assert.ok(focused.document.querySelector('.dimsum'), 'focused card must not expire');
  close.dispatch('click');
  assert.equal(focused.document.querySelector('.dimsum'), null);
  assert.equal(focused.document.activeElement, focused.activeTab);

  const hovered = environment();
  const pending = hovered.controller.render(dish);
  await loaded(hovered);
  assert.equal(await pending, true);
  const hoveredCard = hovered.document.querySelector('.dimsum');
  hoveredCard.dispatch('pointerenter');
  hovered.timer.tick(globalThis.BambuDimSum.dismissDelayMs * 2);
  assert.ok(hovered.document.querySelector('.dimsum'), 'hovered card must not expire');
  hoveredCard.dispatch('pointerleave');
  hovered.timer.tick(globalThis.BambuDimSum.dismissDelayMs - 1);
  assert.ok(hovered.document.querySelector('.dimsum'));
  hovered.timer.tick(1);
  assert.equal(hovered.document.querySelector('.dimsum'), null);
});
