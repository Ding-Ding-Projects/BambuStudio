import assert from 'node:assert/strict';
import { access, readFile } from 'node:fs/promises';
import path from 'node:path';

const siteDir = path.resolve(process.argv[2] || '_site');
const indexPath = path.join(siteDir, 'index.html');
const index = await readFile(indexPath, 'utf8');
const baseUrl = new URL('https://pages-layout.invalid/');
const localScripts = [...index.matchAll(/<script\b[^>]*\bsrc=(['"])(.*?)\1/gi)]
  .map((match) => match[2])
  .filter((source) => new URL(source, baseUrl).origin === baseUrl.origin);

assert.ok(localScripts.length > 0, 'The composed landing page must load at least one local script.');
await access(path.join(siteDir, 'app', 'index.html'));

for (const source of localScripts) {
  const pathname = decodeURIComponent(new URL(source, baseUrl).pathname).replace(/^\/+/, '');
  const resolved = path.resolve(siteDir, pathname);
  assert.ok(
    resolved.startsWith(`${siteDir}${path.sep}`),
    `Landing script escapes the composed Pages root: ${source}`
  );
  await assert.doesNotReject(
    access(resolved),
    undefined,
    `Landing script does not resolve in the composed Pages layout: ${source}`
  );
}

console.log(`Pages layout OK: ${localScripts.join(', ')}`);
