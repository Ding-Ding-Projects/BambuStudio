import assert from 'node:assert/strict';
import test from 'node:test';
import vm from 'node:vm';
import { readFile } from 'node:fs/promises';
const core = await readFile(new URL('../site/core.js', import.meta.url), 'utf8');
const wording = await readFile(new URL('../site/wording.js', import.meta.url), 'utf8');
const schedule = await readFile(new URL('../site/schedule.js', import.meta.url), 'utf8');
function environment() {
  const values = new Map(); const style = new Map();
  const root = { style: { setProperty: (key, value) => style.set(key, value), removeProperty: key => style.delete(key) }, setAttribute() {} };
  const storage = { getItem: key => values.get(key) ?? null, setItem: (key, value) => values.set(key, value), removeItem: key => values.delete(key) };
  const context = vm.createContext({ Date, Intl, TextEncoder, Set, localStorage: storage, location: { search: '' }, document: { body: {}, documentElement: root, querySelectorAll: () => [] }, setInterval: () => 1, clearInterval() {}, addEventListener() {}, BambuControls: { appearanceTargets: () => [{ id: 'cards', properties: ['radius', 'spacing', 'size', 'color', 'font', 'weight'] }] } });
  vm.runInContext(core, context); vm.runInContext(wording, context); vm.runInContext(schedule, context);
  return { api: context.BambuSchedule, site: context.BambuSite, context, values, style, storage };
}
function rule(extra = {}) {
  return { id: 'rule-1', label: 'Evening appearance', enabled: true, priority: 0, startDate: '', endDate: '', startTime: '18:00', endTime: '22:00', allDay: false, everyDay: true, weekdays: [], source: 'local', values: { theme: 'light' }, ...extra };
}
const state = (rules, timeZone = 'UTC') => ({ version: 1, timeZone, rules });
function match(api, value, iso) { return api.evaluate(value, new Date(iso)); }

test('empty defaults are local, valid and do not create a rule', () => {
  const { api } = environment();
  assert.equal(api.defaults().version, 1); assert.equal(api.defaults().rules.length, 0);
  assert.deepEqual(Object.keys(match(api, state([]), '2026-10-05T20:00:00Z').values), []);
});
test('daily window includes start and excludes end', () => {
  const { api } = environment(); const value = state([rule()]);
  assert.equal(match(api, value, '2026-10-05T17:59:00Z').values.theme, undefined);
  assert.equal(match(api, value, '2026-10-05T18:00:00Z').values.theme, 'light');
  assert.equal(match(api, value, '2026-10-05T21:59:00Z').values.theme, 'light');
  assert.equal(match(api, value, '2026-10-05T22:00:00Z').values.theme, undefined);
});
test('selected weekdays compose with date range and disable state', () => {
  const { api } = environment(); const selected = rule({ everyDay: false, weekdays: [1], startDate: '2026-10-05', endDate: '2026-10-12' });
  assert.equal(match(api, state([selected]), '2026-10-05T20:00:00Z').values.theme, 'light');
  assert.equal(match(api, state([selected]), '2026-10-06T20:00:00Z').values.theme, undefined);
  assert.equal(match(api, state([selected]), '2026-10-19T20:00:00Z').values.theme, undefined);
  selected.enabled = false;
  assert.equal(match(api, state([selected]), '2026-10-05T20:00:00Z').values.theme, undefined);
});
test('cross-midnight belongs to its starting weekday and date', () => {
  const { api } = environment(); const value = state([rule({ startTime: '22:00', endTime: '02:00', everyDay: false, weekdays: [1], startDate: '2026-10-05', endDate: '2026-10-05' })]);
  assert.equal(match(api, value, '2026-10-05T23:30:00Z').values.theme, 'light');
  assert.equal(match(api, value, '2026-10-06T01:59:00Z').values.theme, 'light');
  assert.equal(match(api, value, '2026-10-06T02:00:00Z').values.theme, undefined);
  assert.equal(match(api, value, '2026-10-05T01:00:00Z').values.theme, undefined);
});
test('all-day is explicit and equal or incomplete time bounds reject', () => {
  const { api } = environment();
  assert.equal(match(api, state([rule({ allDay: true, startTime: '', endTime: '' })]), '2026-10-05T03:00:00Z').values.theme, 'light');
  for (const extra of [{ startTime: '18:00', endTime: '18:00' }, { startTime: '' }, { endTime: '' }, { allDay: true }]) assert.throws(() => api.validate(state([rule(extra)])));
});
test('higher priority wins and later rule breaks equal priority, per setting', () => {
  const { api } = environment();
  const value = state([rule({ priority: 4, values: { theme: 'dark', fontScale: 130 } }), rule({ id: 'rule-2', priority: 4, values: { theme: 'light' } }), rule({ id: 'rule-3', priority: 5, values: { accent: '#abcdef' } })]);
  const result = match(api, value, '2026-10-05T20:00:00Z');
  assert.equal(result.values.theme, 'light'); assert.equal(result.values.fontScale, 130); assert.equal(result.values.accent, '#abcdef');
  value.rules[0].priority = 6;
  assert.equal(match(api, value, '2026-10-05T20:00:00Z').values.theme, 'dark');
});
test('named timezone and both occurrences of a repeated daylight-saving hour are honored', () => {
  const { api } = environment();
  const value = state([rule({ startTime: '01:00', endTime: '02:00' })], 'America/Toronto');
  assert.equal(match(api, value, '2026-11-01T05:30:00Z').values.theme, 'light');
  assert.equal(match(api, value, '2026-11-01T06:30:00Z').values.theme, 'light');
  assert.equal(match(api, value, '2026-11-01T07:00:00Z').values.theme, undefined);
  assert.throws(() => api.validate(state([], 'Imaginary/Zone')));
});
test('strict import rejects duplicate keys, unsupported version, unknown fields and unpaired sources', () => {
  const { api } = environment();
  assert.throws(() => api.parse('{"version":1,"version":1,"timeZone":"UTC","rules":[]}'));
  assert.throws(() => api.validate({ ...state([]), version: 2 }));
  assert.throws(() => api.validate({ ...state([]), credential: 'unapproved' }));
  assert.throws(() => api.validate(state([rule({ source: 'httpsApi' })])));
  assert.throws(() => api.validate(state([rule({ source: 'homeAssistant' })])));
});
test('date, rule, identifier, weekday, label and value resource bounds reject', () => {
  const { api } = environment();
  for (const extra of [{ startDate: '2026-02-30' }, { startDate: '2026-10-07', endDate: '2026-10-06' }, { priority: 1000 }, { priority: 1.5 }, { label: '' }, { label: 'a'.repeat(129) }, { id: '../escape' }, { everyDay: false, weekdays: [] }, { everyDay: false, weekdays: [1, 1] }, { values: {} }, { values: { imaginarySetting: true } }, { values: { fontScale: 161 } }, { values: { accent: 'url(https://example.invalid/)' } }]) assert.throws(() => api.validate(state([rule(extra)])));
  assert.throws(() => api.validate(state([rule(), rule()])));
  assert.throws(() => api.validate(state(Array.from({ length: 129 }, (_, i) => rule({ id: 'rule-' + i })))));
  assert.throws(() => api.parse(' '.repeat(262145)));
});
test('known element appearance snapshots apply only registered targets and real properties', () => {
  const { api } = environment();
  assert.doesNotThrow(() => api.validate(state([rule({ values: { elementStyles: { cards: { radius: '20px', color: '#abcdef', size: '1.2' } } } })])));
  for (const styles of [{ unknown: { radius: '20px' } }, { cards: { unknown: 'yes' } }, { cards: { radius: '100px' } }, { cards: { color: 'url(secret)' } }]) assert.throws(() => api.validate(state([rule({ values: { elementStyles: styles } })])));
});
test('scheduled override expires back to base without changing the saved base preference', () => {
  const { api, site, values } = environment();
  site.set('theme', 'dark');
  api.save(state([rule({ allDay: true, startTime: '', endTime: '', values: { theme: 'light' } })])); api.start();
  assert.equal(site.get('theme'), 'light'); assert.equal(site.getBase('theme'), 'dark');
  api.save(state([]));
  assert.equal(site.get('theme'), 'dark'); assert.equal(JSON.parse(values.get('bambuStudio.site.v1')).theme, 'dark');
});
test('expired appearance snapshot removes stale CSS properties and restores base styles', () => {
  const { api, site, style } = environment();
  site.set('elementStyles', {});
  api.save(state([rule({ allDay: true, startTime: '', endTime: '', values: { elementStyles: { cards: { radius: '20px' } } } })])); api.start();
  assert.equal(style.get('--el-cards-radius'), '20px');
  api.save(state([]));
  assert.equal(style.has('--el-cards-radius'), false);
});
test('storage failure rolls back memory before emitting or applying a schedule', () => {
  const { api, site, storage } = environment();
  const before = site.getBase('scheduledSettings'); let changes = 0;
  site.subscribe(keys => { if (keys.includes('scheduledSettings')) changes++; });
  storage.setItem = () => { throw new Error('quota'); };
  assert.throws(() => api.save(state([rule()])));
  assert.equal(site.getBase('scheduledSettings'), before); assert.equal(changes, 0);
});
