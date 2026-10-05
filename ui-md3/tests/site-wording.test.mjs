import assert from 'node:assert/strict';
import test from 'node:test';
import vm from 'node:vm';
import { readFile } from 'node:fs/promises';

const source = await readFile(new URL('../site/wording.js', import.meta.url), 'utf8');
function environment(initial = {}) {
  const values = new Map(Object.entries(initial));
  let network = 0;
  const context = vm.createContext({
    TextEncoder,
    localStorage: {
      getItem: key => values.get(key) ?? null,
      setItem: (key, value) => values.set(key, value),
      removeItem: key => values.delete(key)
    },
    fetch: () => { network++; throw new Error('No network permitted'); },
    document: { body: {} },
    BambuSite: { applyCopy() {}, emit() {} }
  });
  vm.runInContext(source, context);
  return { api: context.BambuWording, context, values, network: () => network };
}
const valid = JSON.stringify({ schemaVersion: 1, entries: { Workshop: 'Studio', Studio: 'Room' } });

test('empty profile leaves original wording exact', () => {
  const { api } = environment();
  assert.equal(api.replace('Workshop URL'), 'Workshop URL');
  assert.equal(api.status().loaded, false);
});
test('valid import is atomic, persistent and single-pass', () => {
  const { api, values, network } = environment();
  api.load(valid);
  assert.equal(api.replace('Workshop Studio Workshops'), 'Studio Room Workshops');
  assert.equal(api.status().persistent, true);
  assert.equal(values.size, 1);
  assert.equal(network(), 0);
  assert.equal(environment(Object.fromEntries(values)).api.replace('Workshop'), 'Studio');
  assert.throws(() => api.load('{"schemaVersion":1,"entries":{"Workshop":13}}'));
  assert.equal(api.replace('Workshop'), 'Studio');
});
test('duplicate keys, unsafe names, unknown fields and versions reject', () => {
  const { api } = environment();
  [
    '{"schemaVersion":1,"schemaVersion":1,"entries":{"x":"y"}}',
    '{"schemaVersion":1,"entries":{"x":"y","\\u0078":"z"}}',
    '{"schemaVersion":1,"entries":{"__proto__":"z"}}',
    '{"schemaVersion":1,"entries":{"constructor":"z"}}',
    '{"schemaVersion":2,"entries":{"x":"y"}}',
    '{"schemaVersion":1,"entries":{"x":"y"},"filename":"private.json"}',
    '{"schemaVersion":1,"entries":[]}',
    '{"schemaVersion":1,"entries":{}}',
    '{"schemaVersion":1,"entries":{"x":{"nested":{"too":{"deep":"y"}}}}}',
    '{"schemaVersion":1,"entries":{"x":"y"}} trailing',
    '{"schemaVersion":1,"entries":{"x":"y",}}'
    , '\u00a0{"schemaVersion":1,"entries":{"x":"y"}}'
  ].forEach(raw => assert.throws(() => api.parse(raw)));
});
test('byte, count, key, value and control-character bounds reject', () => {
  const { api } = environment();
  const encode = entries => JSON.stringify({ schemaVersion: 1, entries });
  assert.throws(() => api.parse(' '.repeat(api.LIMITS.bytes + 1)));
  assert.throws(() => api.parse(encode({ ['a'.repeat(api.LIMITS.key + 1)]: 'b' })));
  assert.throws(() => api.parse(encode({ a: 'b'.repeat(api.LIMITS.value + 1) })));
  assert.throws(() => api.parse(encode({ a: 'b\n' })));
  assert.throws(() => api.parse(encode(Object.fromEntries(Array.from({ length: api.LIMITS.entries + 1 }, (_, i) => ['key' + i, 'value'])))));
});
test('replace and clear purge previous cache and restore original wording', () => {
  const { api, values, network } = environment();
  api.load(valid);
  api.load('{"schemaVersion":1,"entries":{"Workshop":"Bench"}}');
  assert.equal(api.replace('Workshop Studio'), 'Bench Studio');
  api.clear();
  assert.equal(api.replace('Workshop'), 'Workshop');
  assert.equal(values.size, 0);
  assert.equal(network(), 0);
});
test('corrupt or stale cache is removed and never partially applied', () => {
  ['{broken', '{"schemaVersion":3,"entries":{"x":"y"}}'].forEach(raw => {
    const { api, values } = environment({ 'bambuStudio.site.wording.v1': raw });
    assert.equal(api.status().loaded, false);
    assert.equal(values.size, 0);
  });
});
test('literal regex metacharacters are safe and Unicode word boundaries stay exact', () => {
  const { api } = environment();
  api.load('{"schemaVersion":1,"entries":{"a+b":"sum","工作":"內容"}}');
  assert.equal(api.replace('a+b (a+b) 工作 工作室'), 'sum (sum) 內容 工作室');
});
test('parameters, exact URLs, code and paths remain unchanged', () => {
  const { api } = environment();
  api.load('{"schemaVersion":1,"entries":{"Workshop":"Bench","count":"total","https":"scheme"}}');
  assert.equal(api.replace('Workshop {count} https://example.invalid/Workshop `Workshop` ./Workshop'), 'Bench {count} https://example.invalid/Workshop `Workshop` ./Workshop');
});
test('storage failure preserves session behavior without claiming persistence', () => {
  const { api, context } = environment();
  context.localStorage.setItem = () => { throw new Error('blocked'); };
  api.load(valid);
  assert.equal(api.status().persistent, false);
  assert.equal(api.replace('Workshop'), 'Studio');
  context.localStorage.removeItem = () => { throw new Error('blocked'); };
  assert.throws(() => api.clear());
  assert.equal(api.status().loaded, true);
});
test('loader does not export, log, record file metadata or send data', () => {
  assert.doesNotMatch(source, /\b(?:fetch|XMLHttpRequest|console|downloadText|filename|file\.name|file\.path)\s*(?:\(|\.|=)/);
  const { api, values } = environment();
  api.load(valid);
  const stored = JSON.parse([...values.values()][0]);
  assert.deepEqual(Object.keys(stored).sort(), ['entries', 'schemaVersion']);
});
