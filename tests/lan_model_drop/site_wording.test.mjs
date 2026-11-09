// The sender page's personal wording file: the same version 1 file Bambu
// Studio accepts, checked in full before use and applied only in the browser.
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath, pathToFileURL } from 'node:url';

const repo = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..', '..');
const siteDir = path.join(repo, 'lan-model-drop', 'site');
const { createWording, validateWording, MAX_FILE_BYTES } = await import(pathToFileURL(path.join(siteDir, 'wording.js')).href);

const bytes = (text) => new TextEncoder().encode(text);
const file = (entries) => bytes(JSON.stringify({ schemaVersion: 1, entries }));

test('a valid file is accepted with its entries in key order', () => {
  const result = validateWording(file({ model: 'thing', 'drop box': 'mailbox' }));
  assert.equal(result.ok, true);
  assert.deepEqual(result.entries, [['drop box', 'mailbox'], ['model', 'thing']]);
});

test('every broken file is refused with a reason and no entries', () => {
  const cases = [
    [bytes(''), 'malformed'],
    [bytes('not json'), 'malformed'],
    [new Uint8Array([0x7b, 0xff, 0x7d]), 'malformed'],
    [new Uint8Array(MAX_FILE_BYTES + 1), 'too-large'],
    [bytes('{"schemaVersion":2,"entries":{}}'), 'unsupported-version'],
    [bytes('{"schemaVersion":1,"entries":{},"extra":1}'), 'unexpected-field'],
    [bytes('{"schemaVersion":1,"entries":[]}'), 'unexpected-field'],
    [bytes('{"schemaVersion":1,"entries":{"a":"b","a":"c"}}'), 'duplicate-key'],
    [bytes('{"schemaVersion":1,"entries":{"a":1}}'), 'non-string-value'],
    [bytes('{"schemaVersion":1,"entries":{"__proto__":"x"}}'), 'unsafe-text'],
    [file({ 'a\u202Eb': 'x' }), 'unsafe-text'],
    [file({ '   ': 'x' }), 'unsafe-text'],
    [file({ ['k'.repeat(161)]: 'x' }), 'key-too-long'],
    [file({ k: 'v'.repeat(1001) }), 'value-too-long'],
    [bytes('{"schemaVersion":1,"entries":{"a":{"b":{"c":{"d":{"e":{"f":{"g":{"h":{}}}}}}}}}}'), 'too-deep'],
  ];
  for (const [input, reason] of cases) {
    const result = validateWording(input);
    assert.equal(result.ok, false, reason);
    assert.equal(result.reason, reason);
    assert.equal(result.entries, undefined);
  }
});

test('whole words are replaced, exact spelling first, and addresses stay as they are', () => {
  const { entries } = validateWording(file({ 'drop box': 'mailbox', model: 'thing', Model: 'Thing' }));
  const wording = createWording(entries);
  assert.equal(wording.active, true);
  assert.equal(wording.apply('The drop box took a model; models wait.'), 'The mailbox took a thing; models wait.');
  assert.equal(wording.apply('Model sent'), 'Thing sent');
  assert.equal(wording.apply('DROP BOX'), 'mailbox');
  assert.equal(wording.apply('http://host/drop box'), 'http://host/drop box');
  assert.equal(wording.apply('C:\\drop box'), 'C:\\drop box');
});

test('two different answers for the same words keep the original words', () => {
  const wording = createWording([['Ink', 'paint'], ['INK', 'colour']]);
  assert.equal(wording.apply('ink'), 'ink');
  assert.equal(wording.apply('Ink'), 'paint');
});

test('no file means the original wording', () => {
  const wording = createWording([]);
  assert.equal(wording.active, false);
  assert.equal(wording.apply('drop box'), 'drop box');
});

test('the page never sends the wording file anywhere', () => {
  const app = fs.readFileSync(path.join(siteDir, 'app.js'), 'utf8');
  const module = fs.readFileSync(path.join(siteDir, 'wording.js'), 'utf8');
  assert.doesNotMatch(module, /XMLHttpRequest|fetch\(|sendBeacon|WebSocket|localStorage|document\./);
  // Only the model upload uses the network, and it is given the model file.
  const opens = [...app.matchAll(/request\.open\(/g)];
  assert.equal(opens.length, 1);
  assert.doesNotMatch(app, /send\([^)]*wording/i);
});
