import assert from 'node:assert/strict';
import test from 'node:test';
import vm from 'node:vm';
import { readFile } from 'node:fs/promises';
const source = await readFile(new URL('../site/attention.js', import.meta.url), 'utf8');
const coreSource = await readFile(new URL('../site/core.js', import.meta.url), 'utf8');
const REQUIRED_MODES = ['attentionFocus', 'attentionLow', 'attentionTime', 'attentionOne', 'attentionMomentum'];
function environment(modified = source) {
  const state = Object.fromEntries(REQUIRED_MODES.map(key => [key, false]));
  Object.assign(state, { attentionNextAction: '', attentionSnoozeUntil: 0 });
  const notifications = [];
  const context = vm.createContext({ Date: { now: () => 0 }, document: {}, BambuSite: { get: key => state[key], set: (key, value) => { state[key] = value; }, notify: (...args) => notifications.push(args) } });
  vm.runInContext(modified, context);
  return { api: context.BambuAttention, state, notifications };
}
test('all five accommodations are independent and off by default', () => {
  const { api, state } = environment();
  assert.deepEqual(Array.from(api.MODES), REQUIRED_MODES);
  for (const key of REQUIRED_MODES) assert.equal(state[key], false);
  state.attentionFocus = true; state.attentionTime = true;
  assert.equal(state.attentionLow, false); assert.equal(state.attentionOne, false); assert.equal(state.attentionMomentum, false);
});
test('session and unchanged minutes are elapsed facts, never negative', () => {
  const { api } = environment();
  assert.equal(api.elapsed(125000).session, 2);
  assert.equal(api.elapsed(125000).unchanged, 2);
  assert.equal(api.elapsed(-500).session, 0);
});
test('next action is chosen, bounded and not partially overwritten on invalid input', () => {
  const { api, state } = environment();
  assert.equal(api.setNextAction('Review the current article'), true);
  assert.equal(api.setNextAction('a'.repeat(513)), false);
  assert.equal(api.setNextAction('line\nfeed'), false);
  assert.equal(api.setNextAction(null), false);
  assert.equal(state.attentionNextAction, 'Review the current article');
  assert.equal(api.setNextAction(''), true);
});
test('momentum is opt-in, delayed, dismissible and low-stimulation aware', () => {
  const { api, state, notifications } = environment();
  assert.equal(api.shouldPrompt(30 * 60000), false);
  state.attentionMomentum = true;
  assert.equal(api.shouldPrompt(19 * 60000), false);
  assert.equal(api.shouldPrompt(20 * 60000), true);
  api.tick(20 * 60000);
  assert.equal(notifications.length, 1);
  assert.equal(notifications[0][2].minutes, 20);
  assert.equal(notifications[0][3].action.key, 'attention.snooze');
  assert.equal(api.shouldPrompt(21 * 60000), false);
  state.attentionLow = true;
  assert.equal(api.shouldPrompt(90 * 60000), false);
});
test('not-now honors the full stated thirty-minute period', () => {
  const { api, state } = environment();
  state.attentionMomentum = true;
  const until = api.snooze(25 * 60000);
  assert.equal(until, 55 * 60000);
  assert.equal(state.attentionSnoozeUntil, until);
  assert.equal(api.shouldPrompt(54 * 60000), false);
  assert.equal(api.shouldPrompt(55 * 60000), true);
});
test('hand-written mode inventory turns red for every removed mode', () => {
  for (const key of REQUIRED_MODES) {
    const broken = source.replace("'" + key + "'", "'removedMode'");
    const { api } = environment(broken);
    assert.throws(() => assert.deepEqual(Array.from(api.MODES), REQUIRED_MODES));
  }
  assert.deepEqual(Array.from(environment().api.MODES), REQUIRED_MODES);
});
test('message emoji is decorative, persisted and removable without changing factual copy', () => {
  const stored = new Map();
  const toast = { children: [], textContent: 'A factual message', classList: { contains: value => value === 'toast-warning' }, getAttribute: () => 'alert', querySelector: function () { return this.children.find(child => child.className === 'message-emoji') || null; }, insertBefore: function (child) { child.parentNode = this; this.children.unshift(child); } };
  const document = { body: {}, querySelectorAll: () => [], createElement: () => ({ attributes: {}, setAttribute(name, value) { this.attributes[name] = value; }, remove() { this.parentNode.children = this.parentNode.children.filter(child => child !== this); } }) };
  const context = vm.createContext({ document, location: { search: '' }, localStorage: { getItem: key => stored.get(key) || null, setItem: (key, value) => stored.set(key, value), removeItem: key => stored.delete(key) } });
  vm.runInContext(coreSource, context);
  const scope = { querySelectorAll: selector => selector.startsWith('.toast,') ? [toast] : [] };
  context.BambuSite.applyCopy(scope);
  assert.equal(toast.children.length, 1);
  assert.equal(toast.children[0].attributes['aria-hidden'], 'true');
  assert.equal(toast.textContent, 'A factual message');
  context.BambuSite.set('messageEmojis', false);
  context.BambuSite.applyCopy(scope);
  assert.equal(toast.children.length, 0);
  assert.equal(JSON.parse(stored.get('bambuStudio.site.v1')).messageEmojis, false);
});
