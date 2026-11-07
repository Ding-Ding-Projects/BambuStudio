'use strict';
const { test } = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const { randomUUID } = require('node:crypto');
const { Controller } = require('./controller.cjs');
const source = fs.readFileSync(path.join(__dirname, 'ui/renderer.js'), 'utf8');
const image = 'ghcr.io/ding-ding-projects/bambustudio-lan-model-drop:v1.0.1@sha256:' + 'b'.repeat(64);
const flush = () => new Promise(resolve => setImmediate(resolve));

function fixture(code) {
  let releaseHealth, failInspection = false, now = Date.now(), timerId = 0;
  const timers = new Map(), elements = new Map();
  function element() { return { value: '', textContent: '', hidden: false, disabled: false, children: [],
    focus() {}, append(...items) { this.children.push(...items); }, replaceChildren() { this.children = []; } }; }
  function get(id) { if (!elements.has(id)) elements.set(id, element()); return elements.get(id); }
  get('name').value = 'Bambu Studio';
  const controller = new Controller('fixture', async (_script, request) => {
    if (request.method === 'inspect') {
      if (failInspection) throw new Error('inspection_failed');
      return { ownership: 'existing-owned', containerId: 'a'.repeat(64), port: 18833, storage: 'existing', ready: true, verifiedImage: image };
    }
    if (request.method === 'start') return { containerId: 'a'.repeat(64) };
    if (request.method === 'health') return new Promise((resolve, reject) => { releaseHealth = fail => fail ? reject(new Error('health_unavailable')) : resolve({ identityVerified: true,
      dataPreserved: true, serviceState: 'running', healthy: true, urls: [], verifiedImage: image }); });
    throw new Error('unexpected_fixture_operation');
  });
  vm.runInNewContext(code, {
    document: { getElementById: get, createElement: element, documentElement: {}, body: { classList: { toggle() {} } } },
    localStorage: { getItem() { return null; }, setItem() {} }, crypto: { randomUUID }, Date: { now: () => now },
    setTimeout(fn, delay) { const id = ++timerId; timers.set(id, { fn, delay }); return id; }, clearTimeout(id) { timers.delete(id); },
    window: { setup: { async request(request) {
      try { return { ok: true, result: await controller.dispatch(request) }; }
      catch (error) { return { ok: false, error: { code: error.message } }; }
    }, async open() { return false; } } }
  });
  return { get, controller, async begin() {
    await get('next').onclick(); await get('next').onclick(); await get('next').onclick(); await flush();
    assert.equal(typeof releaseHealth, 'function');
    assert.equal(controller.ledger.view().workerActive, false);
    assert.equal(controller.ledger.view().reconciliationActive, true);
  }, async tick() {
    const pending = [...timers].find(([, value]) => value.delay <= 1000);
    if (pending) { timers.delete(pending[0]); pending[1].fn(); }
    await flush();
  }, async complete(fail = false) { releaseHealth(fail); await flush(); },
  pendingPolls() { return [...timers.values()].filter(value => value.delay <= 1000).length; },
  expire() { now += 700000; get('language').value = 'en'; get('language').onchange(); },
  failInspection() { failInspection = true; } };
}

async function completionScenario(code) {
  const ui = fixture(code); await ui.begin();
  await ui.tick(); await ui.tick();
  assert.equal(ui.get('next').disabled, true, 'reconciliation must disable manual recheck while active');
  await ui.complete(); await ui.tick();
  assert.equal(ui.get('result').hidden, false, 'completed reconciliation must reach result without manual recheck');
  assert.match(ui.get('identity').textContent, /Actual service: verified published image/);
  return ui;
}

test('real renderer polls deferred reconciliation through automatic terminal display and identity expiry', async () => {
  const ui = await completionScenario(source);
  ui.expire(); assert.match(ui.get('identity').textContent, /requires inspection/);
});

async function cancelRaceScenario(code) {
  const ui = fixture(code); await ui.begin(); await ui.tick();
  await ui.complete(); // Backend terminal, renderer has not consumed its next poll yet.
  await ui.get('cancel').onclick();
  assert.equal(ui.get('result').hidden, false, 'terminal cancel response must render actual result');
  assert.match(ui.get('status').textContent, /already completed: succeeded/);
  assert.doesNotMatch(ui.get('status').textContent, /Cancellation requested/);
  assert.match(ui.get('identity').textContent, /verified published image/);
  ui.failInspection(); await ui.get('next').onclick();
  assert.match(ui.get('identity').textContent, /requires inspection/);
}

test('real renderer consumes already-terminal cancellation and invalidates failed inspection identity', async () => {
  await cancelRaceScenario(source);
});

test('failed bounded reconciliation stops polling and exposes manual recheck', async () => {
  const ui = fixture(source); await ui.begin(); await ui.tick();
  await ui.complete(true); await ui.tick();
  assert.equal(ui.controller.ledger.view().reconciliationActive, false);
  assert.equal(ui.controller.ledger.view().terminal, null);
  assert.equal(ui.get('next').disabled, false);
  assert.equal(ui.get('result').hidden, true);
  assert.equal(ui.pendingPolls(), 0);
  assert.match(ui.get('status').textContent, /reconciliation_required/);
  assert.match(ui.get('identity').textContent, /requires inspection/);
});

test('causal old polling mutation fails the same real renderer completion scenario', async () => {
  const mutant = source.replace('!operation.workerActive && !operation.reconciliationActive && !operation.terminal', '!operation.workerActive && !operation.terminal');
  assert.notEqual(mutant, source);
  await assert.rejects(completionScenario(mutant), /completed reconciliation must reach result/);
});

test('causal old cancel mutation fails the same real renderer race scenario', async () => {
  const start = source.indexOf("$('cancel').onclick =");
  const end = source.indexOf('async function poll()', start);
  assert.ok(start >= 0 && end > start);
  const mutant = source.slice(0, start) + "$('cancel').onclick = () => action(async () => { await request('cancel', { operationId: operation.operationId }); message = 'Cancellation requested.'; });\n" + source.slice(end);
  await assert.rejects(cancelRaceScenario(mutant), /terminal cancel response must render actual result/);
});
