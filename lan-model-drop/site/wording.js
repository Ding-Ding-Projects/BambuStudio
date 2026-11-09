// A personal wording file, the same version 1 file Bambu Studio accepts:
// {"schemaVersion": 1, "entries": {"ordinary words": "your words"}}. It is
// read and applied only in this browser; nothing is sent to the drop box.
// No DOM: the tests import this module directly.

export const MAX_FILE_BYTES = 256 * 1024;
const MAX_KEY_CODE_POINTS = 160;
const MAX_VALUE_CODE_POINTS = 1000;
const MAX_DEPTH = 8;
const RESERVED = new Set(['__proto__', 'constructor', 'prototype']);

// Control, text-direction and other invisible formatting characters.
const UNSAFE = /[\u0000-\u001F\u007F-\u009F؜​-‏‪-‮⁠-⁩﻿￹-￻]/u;

function safeText(text) {
  return !RESERVED.has(text) && !UNSAFE.test(text) && /\S/u.test(text);
}

function codePoints(text) {
  return Array.from(text).length;
}

function depthOf(value, depth = 1) {
  if (depth > MAX_DEPTH) return depth;
  if (Array.isArray(value)) return Math.max(depth, ...value.map((item) => depthOf(item, depth + 1)));
  if (value && typeof value === 'object') return Math.max(depth, ...Object.values(value).map((item) => depthOf(item, depth + 1)));
  return depth - 1;
}

// JSON.parse keeps the last of two equal keys, so duplicates are found by
// scanning each object's keys in the source text.
function hasDuplicateKey(source) {
  const stack = [];
  const keyPattern = /"((?:[^"\\]|\\.)*)"\s*:/y;
  let inString = false;
  for (let i = 0; i < source.length; i += 1) {
    const c = source[i];
    if (inString) {
      if (c === '\\') i += 1;
      else if (c === '"') inString = false;
      continue;
    }
    if (c === '{') { stack.push(new Set()); continue; }
    if (c === '[') { stack.push(null); continue; }
    if (c === '}' || c === ']') { stack.pop(); continue; }
    if (c !== '"') continue;
    const keys = stack[stack.length - 1];
    keyPattern.lastIndex = i;
    const match = keys ? keyPattern.exec(source) : null;
    if (match) {
      const key = JSON.parse(`"${match[1]}"`);
      if (keys.has(key)) return true;
      keys.add(key);
      i = keyPattern.lastIndex - 1;
    } else {
      inString = true;
    }
  }
  return false;
}

// Checks the whole file before any entry is used. Returns { ok: true, entries }
// or { ok: false, reason } with a reason code that never contains the input.
export function validateWording(bytes) {
  if (!(bytes instanceof Uint8Array)) return { ok: false, reason: 'unreadable' };
  if (bytes.length > MAX_FILE_BYTES) return { ok: false, reason: 'too-large' };
  if (bytes.length === 0) return { ok: false, reason: 'malformed' };
  let source;
  try {
    source = new TextDecoder('utf-8', { fatal: true }).decode(bytes);
  } catch {
    return { ok: false, reason: 'malformed' };
  }
  let root;
  try {
    root = JSON.parse(source);
  } catch {
    return { ok: false, reason: 'malformed' };
  }
  if (depthOf(root) > MAX_DEPTH) return { ok: false, reason: 'too-deep' };
  if (hasDuplicateKey(source)) return { ok: false, reason: 'duplicate-key' };
  if (!root || typeof root !== 'object' || Array.isArray(root)) return { ok: false, reason: 'unexpected-field' };
  if (root.schemaVersion !== 1) return { ok: false, reason: 'unsupported-version' };
  const fields = Object.keys(root);
  if (fields.length !== 2 || !fields.includes('entries')) return { ok: false, reason: 'unexpected-field' };
  const { entries } = root;
  if (!entries || typeof entries !== 'object' || Array.isArray(entries)) return { ok: false, reason: 'unexpected-field' };
  const result = [];
  for (const [key, value] of Object.entries(entries)) {
    if (codePoints(key) > MAX_KEY_CODE_POINTS) return { ok: false, reason: 'key-too-long' };
    if (!safeText(key)) return { ok: false, reason: 'unsafe-text' };
    if (typeof value !== 'string') return { ok: false, reason: 'non-string-value' };
    if (codePoints(value) > MAX_VALUE_CODE_POINTS) return { ok: false, reason: 'value-too-long' };
    if (!safeText(value)) return { ok: false, reason: 'unsafe-text' };
    result.push([key, value]);
  }
  result.sort((a, b) => (a[0] < b[0] ? -1 : a[0] > b[0] ? 1 : 0));
  return { ok: true, entries: result };
}

const foldAscii = (c) => (c >= 'A' && c <= 'Z' ? c.toLowerCase() : c);
const isWord = (c) => c !== undefined && /[\p{L}\p{N}_]/u.test(c);

function indexEntries(entries) {
  const index = new Map();
  entries.forEach(([key], position) => {
    if (!key) return;
    const first = foldAscii(key[0]);
    if (!index.has(first)) index.set(first, []);
    index.get(first).push(position);
  });
  return index;
}

// Replaces whole words the same way Bambu Studio does: an exact spelling wins,
// then a case-insensitive match; two different answers for one match keep the
// original words. Addresses and paths are left exactly as they are.
export function createWording(entries) {
  const list = Array.isArray(entries) ? entries : [];
  const index = indexEntries(list);
  function apply(source) {
    if (typeof source !== 'string' || !list.length || source.length > 32768) return source;
    if (source.includes('://') || /[\\/%]/u.test(source)) return source;
    let output = '';
    let i = 0;
    while (i < source.length) {
      let chosen = null;
      let ambiguous = false;
      const candidates = index.get(foldAscii(source[i]));
      if (candidates) {
        for (let pass = 0; pass < 2 && !chosen; pass += 1) {
          for (const position of candidates) {
            const [key, value] = list[position];
            const end = i + key.length;
            if (end > source.length) continue;
            if (i !== 0 && isWord(source[i - 1]) && isWord(key[0])) continue;
            if (end !== source.length && isWord(source[end]) && isWord(key[key.length - 1])) continue;
            const piece = source.slice(i, end);
            const equal = pass === 0 ? piece === key : [...piece].every((c, k) => foldAscii(c) === foldAscii(key[k]));
            if (!equal) continue;
            if (!chosen) chosen = [key, value];
            else if (chosen[0].length !== key.length) break;
            else if (chosen[1] !== value) ambiguous = true;
            if (pass === 0) break;
          }
          if (ambiguous) break;
        }
      }
      if (ambiguous) {
        output += source.slice(i, i + chosen[0].length);
        i += chosen[0].length;
      } else if (chosen) {
        output += chosen[1];
        i += chosen[0].length;
      } else {
        output += source[i];
        i += 1;
      }
      if (output.length > 65536) return source;
    }
    return output;
  }
  return { active: list.length > 0, apply };
}
