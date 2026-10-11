// The drop box queue on disk: /data/queue/<id>.bin holds the bytes and
// <id>.json the record. A record is written last, through a temporary name
// and a rename, so an item exists only once both files are complete.
import { randomBytes } from 'node:crypto';
import fs from 'node:fs';
import fsp from 'node:fs/promises';
import path from 'node:path';
import { MODEL_TYPES } from './sniff.mjs';

export const ID_PATTERN = /^[0-9a-f]{32}$/;
const FILE_MODE = 0o600;

export function newId() {
  return randomBytes(16).toString('hex');
}

// UTC to the second, for example 2026-10-09T08:15:42Z.
export function utcSeconds(milliseconds) {
  return new Date(milliseconds).toISOString().replace(/\.\d{3}Z$/, 'Z');
}

function validRecord(record, id) {
  return record && record.id === id
    && typeof record.fileName === 'string' && record.fileName !== ''
    && typeof record.sender === 'string'
    && Number.isSafeInteger(record.bytes) && record.bytes >= 0
    && typeof record.sha256 === 'string' && /^[0-9a-f]{64}$/.test(record.sha256)
    && Number.isFinite(record.receivedAtMs)
    && MODEL_TYPES.includes(record.type);
}

export class DropQueue {
  constructor({ dataDir, maxFiles, maxBytes, now = Date.now, log = () => {} }) {
    this.dir = path.join(dataDir, 'queue');
    this.maxFiles = maxFiles;
    this.maxBytes = maxBytes;
    this.now = now;
    this.log = log;
    this.items = new Map();
    this.sequence = 0;
    this.reservedFiles = 0;
    this.reservedBytes = 0;
  }

  binPath(id) {
    return path.join(this.dir, `${id}.bin`);
  }

  jsonPath(id) {
    return path.join(this.dir, `${id}.json`);
  }

  // Reads the queue back after a restart. Unfinished uploads, temporary
  // files and bytes without a record are removed.
  load() {
    fs.mkdirSync(this.dir, { recursive: true, mode: 0o700 });
    const names = fs.readdirSync(this.dir);
    const loaded = [];
    for (const name of names) {
      const match = /^([0-9a-f]{32})\.json$/.exec(name);
      if (!match) continue;
      const id = match[1];
      try {
        const record = JSON.parse(fs.readFileSync(this.jsonPath(id), 'utf8'));
        const size = fs.statSync(this.binPath(id)).size;
        if (validRecord(record, id) && size === record.bytes) {
          loaded.push(record);
          continue;
        }
      } catch {
        // Unreadable records are removed below.
      }
      fs.rmSync(this.jsonPath(id), { force: true });
      fs.rmSync(this.binPath(id), { force: true });
    }
    loaded.sort((a, b) => a.receivedAtMs - b.receivedAtMs || a.id.localeCompare(b.id));
    for (const record of loaded) this.items.set(record.id, { ...record, sequence: this.sequence++ });
    for (const name of fs.readdirSync(this.dir)) {
      const bin = /^([0-9a-f]{32})\.bin$/.exec(name);
      if (bin && this.items.has(bin[1])) continue;
      if (/^[0-9a-f]{32}\.json$/.test(name) && this.items.has(name.slice(0, 32))) continue;
      fs.rmSync(path.join(this.dir, name), { force: true, recursive: true });
    }
  }

  stats() {
    let bytes = 0;
    for (const item of this.items.values()) bytes += item.bytes;
    return { files: this.items.size, bytes };
  }

  // Space for one upload of the declared size, held until it is committed or
  // released, so parallel uploads cannot overfill the box together.
  reserve(bytes) {
    const { files, bytes: stored } = this.stats();
    if (files + this.reservedFiles + 1 > this.maxFiles) return null;
    if (stored + this.reservedBytes + bytes > this.maxBytes) return null;
    this.reservedFiles += 1;
    this.reservedBytes += bytes;
    let held = true;
    return () => {
      if (!held) return;
      held = false;
      this.reservedFiles -= 1;
      this.reservedBytes -= bytes;
    };
  }

  tempBinPath(id) {
    return path.join(this.dir, `${id}.bin.tmp`);
  }

  // Moves a finished upload into the queue and writes its record.
  async commit(id, { fileName, sender, bytes, sha256, type }) {
    const receivedAtMs = this.now();
    const record = { id, fileName, sender, bytes, sha256, receivedAt: utcSeconds(receivedAtMs), receivedAtMs, type };
    await fsp.rename(this.tempBinPath(id), this.binPath(id));
    const tempJson = `${this.jsonPath(id)}.tmp`;
    try {
      await fsp.writeFile(tempJson, `${JSON.stringify(record)}\n`, { mode: FILE_MODE, flag: 'wx' });
      await fsp.rename(tempJson, this.jsonPath(id));
    } catch (error) {
      await fsp.rm(tempJson, { force: true });
      await fsp.rm(this.binPath(id), { force: true });
      throw error;
    }
    this.items.set(id, { ...record, sequence: this.sequence++ });
    return record;
  }

  // Oldest first, with exactly the fields of the station protocol.
  list() {
    return [...this.items.values()]
      .sort((a, b) => a.receivedAtMs - b.receivedAtMs || a.sequence - b.sequence)
      .map(({ id, fileName, sender, bytes, sha256, receivedAt, type }) => ({ id, fileName, sender, bytes, sha256, receivedAt, type }));
  }

  get(id) {
    return ID_PATTERN.test(id) ? this.items.get(id) ?? null : null;
  }

  // Removing an item that is already gone is not an error.
  async remove(id) {
    if (!ID_PATTERN.test(id)) return false;
    const existed = this.items.delete(id);
    await fsp.rm(this.jsonPath(id), { force: true });
    await fsp.rm(this.binPath(id), { force: true });
    return existed;
  }

  // Removes items older than the time to live. Returns how many went.
  async expire(ttlMs) {
    const cutoff = this.now() - ttlMs;
    const old = [...this.items.values()].filter((item) => item.receivedAtMs <= cutoff);
    for (const item of old) await this.remove(item.id);
    if (old.length) this.log(`removed ${old.length} expired item(s)`);
    return old.length;
  }
}
