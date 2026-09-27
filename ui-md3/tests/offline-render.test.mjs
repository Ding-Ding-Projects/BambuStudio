/*
 * The composed site must render with the network cut.
 *
 * This suite exists because of a page that did not. The UI kit at
 * /app/design-system/ui_kits/bambu-studio/ used to load React, ReactDOM and
 * @babel/standalone from unpkg and compile its own JSX in the visitor's
 * browser. On a good day that was three third-party requests and a 2.7 MB
 * compiler; on a bad one — unpkg slow, blocked by a network policy, or simply
 * down — the page served a <div id="app"></div> and stopped there. Nothing in
 * CI noticed, because the layout gate only inspected the landing page.
 *
 * So the check is deliberately blunt: serve the composed tree, point a headless
 * Chrome at it with every hostname but loopback blackholed, and require that
 * real UI comes back. A page that quietly depends on a CDN cannot pass.
 *
 *   node --test ui-md3/tests/offline-render.test.mjs
 *
 * It composes its own _site into a temporary directory, so it needs no setup
 * and leaves nothing behind.
 */
import assert from 'node:assert/strict';
import { execFile } from 'node:child_process';
import { mkdtemp, readFile, rm } from 'node:fs/promises';
import { createServer } from 'node:http';
import { tmpdir } from 'node:os';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';
import { promisify } from 'node:util';

import { navigate, startChrome, stopChrome } from './devtools.mjs';

const run = promisify(execFile);
const testDir = path.dirname(fileURLToPath(import.meta.url));
const repoRoot = path.resolve(testDir, '..', '..');

// Every DNS name resolves nowhere; loopback literals still connect, so the
// page's own origin works and everything off-site fails as if unplugged.
const OFFLINE = ['--host-resolver-rules=MAP * 0.0.0.0, EXCLUDE 127.0.0.1'];

/*
 * `root` is the element the page's scripts fill in, so the element count below
 * measures what they built rather than the static shell around it — the kit's
 * #app holds exactly zero elements when React never runs. The floors sit well
 * under what these pages actually render (290 and 193 elements when this was
 * written): they are there to separate "rendered" from "blank", not to pin a
 * design down to the element.
 */
const PAGES = [
  {
    name: 'the UI kit prototype',
    url: 'app/design-system/ui_kits/bambu-studio/index.html',
    root: '#app',
    mounted: '#app [data-theme]',
    elements: 150,
    text: 400,
  },
  {
    name: 'the landing site',
    url: 'index.html',
    root: 'body',
    mounted: 'header',
    elements: 100,
    text: 400,
  },
];

const TYPES = new Map(Object.entries({
  '.html': 'text/html; charset=utf-8',
  '.js': 'text/javascript; charset=utf-8',
  '.mjs': 'text/javascript; charset=utf-8',
  '.css': 'text/css; charset=utf-8',
  '.json': 'application/json; charset=utf-8',
  '.webp': 'image/webp',
  '.png': 'image/png',
  '.svg': 'image/svg+xml',
  '.woff2': 'font/woff2',
  '.txt': 'text/plain; charset=utf-8',
}));

async function serve(root) {
  const requested = [];
  const server = createServer(async (request, response) => {
    const { pathname } = new URL(request.url, 'http://localhost');
    requested.push(pathname);
    const resolved = path.resolve(root, `.${decodeURIComponent(pathname)}`);
    if (resolved !== root && !resolved.startsWith(root + path.sep)) {
      response.writeHead(403).end('forbidden');
      return;
    }
    try {
      const body = await readFile(resolved);
      response.writeHead(200, {
        'Content-Type': TYPES.get(path.extname(resolved).toLowerCase()) || 'application/octet-stream',
        'Cache-Control': 'no-store',
      }).end(body);
    } catch {
      response.writeHead(404).end('not found');
    }
  });
  await new Promise((resolve) => server.listen(0, '127.0.0.1', resolve));
  return {
    origin: `http://127.0.0.1:${server.address().port}`,
    requested,
    close: () => new Promise((resolve) => server.close(resolve)),
  };
}

/** Composes the published tree exactly as CI does, into a temporary directory. */
async function composeSite() {
  const directory = await mkdtemp(path.join(tmpdir(), 'bambu-offline-'));
  const out = path.join(directory, '_site');
  await run(process.execPath, [path.join(repoRoot, 'ui-md3', 'scripts', 'compose-site.mjs'), out]);
  return { directory, out };
}

test('the composed site renders with every off-site host unreachable', async (t) => {
  const site = await composeSite();
  const server = await serve(site.out);
  const chrome = await startChrome(OFFLINE);
  t.after(async () => {
    await stopChrome(chrome);
    await server.close();
    await rm(site.directory, { recursive: true, force: true, maxRetries: 20, retryDelay: 100 });
  });

  for (const page of PAGES) {
    await navigate(chrome.session, `${server.origin}/${page.url}`, { requireFonts: false });

    const rendered = await chrome.session.send('Runtime.evaluate', {
      expression: `JSON.stringify((() => {
        const root = document.querySelector(${JSON.stringify(page.root)});
        return {
          mounted: !!document.querySelector(${JSON.stringify(page.mounted)}),
          elements: root ? root.getElementsByTagName('*').length : -1,
          text: root ? root.innerText.replace(/\\s+/g, ' ').trim().length : -1,
        };
      })())`,
      returnByValue: true,
    });
    const result = JSON.parse(rendered.result.value);

    assert.equal(
      result.mounted,
      true,
      `${page.name} did not render offline: no element matched ${page.mounted}. ` +
      'Something on the page is still waiting on a third-party request.'
    );
    assert.ok(
      result.elements >= page.elements,
      `${page.name} rendered ${result.elements} elements inside ${page.root} offline, expected at least ${page.elements}`
    );
    assert.ok(
      result.text >= page.text,
      `${page.name} rendered ${result.text} characters of text offline, expected at least ${page.text}`
    );
  }

  // Nothing may have been asked of anywhere but this server. Chrome cannot
  // reach an off-site host under the resolver rule, but a page that tries is
  // still broken — it would be racing a timeout on a real visitor's browser.
  assert.equal(
    server.requested.some((pathname) => /^https?:/i.test(pathname)),
    false,
    'The composed site requested an absolute URL'
  );
});

// The matching static contract — that no published page names an off-site
// script or stylesheet at all — lives in assert-pages-layout.mjs, which runs
// during compose and needs no browser. This file asks the harder question that
// only a browser can answer: with the network gone, does the page still work?
