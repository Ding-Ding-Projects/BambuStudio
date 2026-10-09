// The station key and the drop code. Both live in the data volume with owner
// read and write only (mode 0600). Neither value is ever written to the log.
import { createHash, randomBytes, randomInt, timingSafeEqual } from 'node:crypto';
import fs from 'node:fs';
import path from 'node:path';
import { validDropCode, validStationKey } from './config.mjs';

export const STATION_KEY_FILE = 'station-key';
export const DROP_CODE_FILE = 'drop-code';
const SECRET_MODE = 0o600;

// Write a small secret file through a temporary name and a rename, so a
// crash never leaves a half-written key behind.
export function writeSecretFile(file, value) {
  const temp = `${file}.${randomBytes(6).toString('hex')}.tmp`;
  fs.writeFileSync(temp, `${value}\n`, { mode: SECRET_MODE, flag: 'wx' });
  try {
    fs.chmodSync(temp, SECRET_MODE);
    fs.renameSync(temp, file);
  } catch (error) {
    fs.rmSync(temp, { force: true });
    throw error;
  }
}

function readSecretFile(file) {
  try {
    const value = fs.readFileSync(file, 'utf8').trim();
    // An existing file is kept private even if something widened it.
    fs.chmodSync(file, SECRET_MODE);
    return value;
  } catch (error) {
    if (error.code === 'ENOENT') return null;
    throw error;
  }
}

export function generateStationKey() {
  return randomBytes(32).toString('base64url');
}

// Station key: DROP_STATION_KEY when set (nothing is written then), otherwise
// the key generated on the first start and kept in /data/station-key.
export function loadStationKey(dataDir, configuredKey) {
  if (configuredKey) return { key: configuredKey, source: 'environment' };
  const file = path.join(dataDir, STATION_KEY_FILE);
  const stored = readSecretFile(file);
  if (stored !== null && validStationKey(stored)) return { key: stored, source: 'file' };
  const key = generateStationKey();
  writeSecretFile(file, key);
  return { key, source: 'generated' };
}

export function generateDropCode(previous = null) {
  for (;;) {
    const code = String(randomInt(0, 1000000)).padStart(6, '0');
    if (code !== previous) return code;
  }
}

export function loadDropCode(dataDir, fixedCode) {
  if (fixedCode) return { code: fixedCode, fixed: true };
  const file = path.join(dataDir, DROP_CODE_FILE);
  const stored = readSecretFile(file);
  if (stored !== null && validDropCode(stored)) return { code: stored, fixed: false };
  const code = generateDropCode();
  writeSecretFile(file, code);
  return { code, fixed: false };
}

export function storeDropCode(dataDir, code) {
  writeSecretFile(path.join(dataDir, DROP_CODE_FILE), code);
}

// Constant-time comparison. Both sides are hashed first, so the comparison
// always runs over 32 bytes and the time taken says nothing about the length
// or the content of the secret.
export function secretsEqual(candidate, secret) {
  if (typeof candidate !== 'string' || typeof secret !== 'string') return false;
  const a = createHash('sha256').update(candidate, 'utf8').digest();
  const b = createHash('sha256').update(secret, 'utf8').digest();
  return timingSafeEqual(a, b);
}
