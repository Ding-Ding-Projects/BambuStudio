import assert from 'node:assert/strict';
import test from 'node:test';
import vm from 'node:vm';
import { readFile } from 'node:fs/promises';
const source = await readFile(new URL('../site/confirmation.js', import.meta.url), 'utf8');
function authorization(modified = source) {
  let runs = 0;
  const context = vm.createContext({ BambuSite: {}, document: {} });
  vm.runInContext(modified, context);
  return { state: context.BambuConfirmation.createAuthorization(() => { runs++; }), runs: () => runs };
}
test('untouched and one-key states never authorize even a full slider', () => {
  const { state, runs } = authorization();
  assert.equal(state.move(100), false);
  state.key(0, true);
  assert.equal(state.ready(), false);
  assert.equal(state.move(100), false);
  assert.equal(runs(), 0);
});
test('both independent keys and full slider authorize exactly once', () => {
  const { state, runs } = authorization();
  state.key(0, true); state.key(1, true);
  assert.equal(state.ready(), true);
  assert.equal(state.move(99), false);
  assert.equal(state.move(100), true);
  assert.equal(state.move(100), false);
  assert.equal(state.consumed(), true);
  assert.equal(runs(), 1);
});
test('withdrawing either key invalidates a previous partial slider attempt', () => {
  const { state, runs } = authorization();
  state.key(0, true); state.key(1, true); state.move(75);
  state.key(0, false);
  assert.equal(state.move(100), false); assert.equal(runs(), 0);
  state.key(0, true); state.key(1, false);
  assert.equal(state.move(100), false); assert.equal(runs(), 0);
});
test('emergency cancellation permanently closes that authorization', () => {
  const { state, runs } = authorization();
  state.key(0, true); state.key(1, true);
  assert.equal(state.cancel(), true);
  assert.equal(state.key(0, true), false);
  assert.equal(state.move(100), false);
  assert.equal(runs(), 0);
});
test('invalid indices, types and out-of-range slider values fail closed', () => {
  const { state, runs } = authorization();
  assert.equal(state.key(2, true), false); assert.equal(state.key(0, 'true'), false);
  state.key(0, true); state.key(1, true);
  for (const value of [-1, 0, 101, NaN, Infinity, '100', null]) assert.equal(state.move(value), false);
  assert.equal(runs(), 0);
});
test('two prompts cannot inherit each other’s keys or cancellation', () => {
  const first = authorization(); const second = authorization();
  first.state.key(0, true); second.state.key(1, true);
  assert.equal(first.state.move(100), false); assert.equal(second.state.move(100), false);
  first.state.cancel(); second.state.key(0, true);
  assert.equal(second.state.move(100), true); assert.equal(first.runs(), 0);
});
test('negative regression catches removal of either independent-key check', () => {
  for (const key of ['!keys[0]', '!keys[1]']) {
    const broken = authorization(source.replace(key + ' || ', ''));
    broken.state.key(key === '!keys[0]' ? 1 : 0, true);
    assert.equal(broken.state.move(100), true);
    assert.throws(() => assert.equal(broken.runs(), 0));
  }
  const restored = authorization(); restored.state.key(0, true);
  assert.equal(restored.state.move(100), false);
});
