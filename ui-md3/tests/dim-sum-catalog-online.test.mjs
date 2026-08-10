/*
 * Online provenance checks for the bounded dim-sum startup cache.
 *
 * GitHub data is read through the authenticated `gh` CLI so this test uses
 * the same public records locally and in Actions. The catalog is requested at
 * the exact revision recorded by the site rather than from a moving branch.
 */
import assert from 'node:assert/strict';
import { execFile } from 'node:child_process';
import { readFile } from 'node:fs/promises';
import path from 'node:path';
import test from 'node:test';
import { promisify } from 'node:util';
import { fileURLToPath } from 'node:url';
import vm from 'node:vm';

const execFileAsync = promisify(execFile);
const testDir = path.dirname(fileURLToPath(import.meta.url));
const dataPath = path.resolve(testDir, '../site/dimsum.data.js');
const repository = 'Ding-Ding-Projects/dim-sum-photos';
const releaseRoot = `https://github.com/${repository}/releases/download/`;
const rawCatalogRoot = `https://raw.githubusercontent.com/${repository}/`;
const ghExecutable = process.platform === 'win32' ? 'gh.exe' : 'gh';
const maxBuffer = 32 * 1024 * 1024;

async function runGh(args) {
  const { stdout } = await execFileAsync(ghExecutable, args, {
    encoding: 'utf8',
    maxBuffer,
    windowsHide: true,
  });
  return stdout;
}

async function loadCache() {
  const source = await readFile(dataPath, 'utf8');
  const sandbox = { window: {} };
  vm.runInNewContext(source, sandbox, { filename: dataPath });
  return sandbox.window.BAMBU_DIM_SUM;
}

async function loadPinnedCatalog(revision) {
  const raw = await runGh([
    'api',
    '-X', 'GET',
    '-H', 'Accept: application/vnd.github.raw+json',
    `repos/${repository}/contents/catalog/index.json`,
    '-f', `ref=${revision}`,
  ]);
  return JSON.parse(raw);
}

async function loadRelease(tag) {
  const raw = await runGh([
    'release', 'view', tag,
    '--repo', repository,
    '--json', 'tagName,isDraft,isPrerelease,assets',
  ]);
  return JSON.parse(raw);
}

function changedSha256(sha256) {
  const first = sha256[0] === '0' ? '1' : '0';
  return `${first}${sha256.slice(1)}`;
}

function changedReleaseUrl(url) {
  const parsed = new URL(url);
  const segments = parsed.pathname.split('/');
  segments[segments.length - 1] = 'hk-dish-9999-valid-looking-but-wrong.png';
  parsed.pathname = segments.join('/');
  return parsed.href;
}

function assertDishMatches(dish, catalogDish, asset, releaseTag) {
  assert.ok(catalogDish, `${dish.id}: pinned catalog record is missing`);
  assert.equal(catalogDish.id, dish.id, `${dish.id}: catalog id changed`);
  assert.equal(catalogDish.name?.en, dish.en, `${dish.id}: English name changed`);
  assert.equal(catalogDish.name?.zhHant, dish.yue, `${dish.id}: Traditional Chinese name changed`);
  assert.equal(catalogDish.image?.alt?.en, dish.altEn, `${dish.id}: English image alt text changed`);
  assert.equal(catalogDish.image?.alt?.yue, dish.altYue, `${dish.id}: Cantonese image alt text changed`);
  assert.equal(catalogDish.image?.path, `images/${dish.photo.file}`,
    `${dish.id}: catalog image path changed`);

  const expectedUrl = `${releaseRoot}${encodeURIComponent(releaseTag)}/${encodeURIComponent(dish.photo.file)}`;
  assert.equal(dish.photo.url, expectedUrl, `${dish.id}: cached release URL changed`);
  assert.match(dish.photo.sha256, /^[0-9a-f]{64}$/u, `${dish.id}: cached SHA-256 is malformed`);

  assert.ok(asset, `${dish.id}: published release asset is missing`);
  assert.equal(asset.name, dish.photo.file, `${dish.id}: release asset name changed`);
  assert.equal(asset.url, expectedUrl, `${dish.id}: published release URL changed`);
  assert.equal(asset.contentType, 'image/png', `${dish.id}: release asset content type changed`);
  assert.equal(asset.state, 'uploaded', `${dish.id}: release asset is not uploaded`);
  assert.equal(asset.digest, `sha256:${dish.photo.sha256}`, `${dish.id}: release asset digest changed`);
  assert.ok(Number.isSafeInteger(asset.size) && asset.size > 0,
    `${dish.id}: release asset size must be a positive integer`);
}

function applyRequestedNegativeControl(cache) {
  const requested = process.env.BAMBU_DIM_SUM_CATALOG_NEGATIVE_CONTROL;
  if (!requested) return cache;
  assert.ok(requested === 'sha256' || requested === 'url',
    'BAMBU_DIM_SUM_CATALOG_NEGATIVE_CONTROL must be sha256 or url');

  const dishes = cache.dishes.map((dish, index) => {
    if (index !== 0) return dish;
    return {
      ...dish,
      photo: {
        ...dish.photo,
        ...(requested === 'sha256'
          ? { sha256: changedSha256(dish.photo.sha256) }
          : { url: changedReleaseUrl(dish.photo.url) }),
      },
    };
  });
  return { ...cache, dishes };
}

let onlineEvidence;

async function evidence() {
  if (onlineEvidence) return onlineEvidence;

  const cache = await loadCache();
  assert.equal(cache.schemaVersion, 1, 'dim-sum cache schema version changed');
  assert.equal(cache.chance, 0.10, 'startup surprise probability changed');
  assert.equal(cache.source.catalogUrl,
    `https://raw.githubusercontent.com/${repository}/main/catalog/index.json`,
    'catalog source URL changed');
  assert.match(cache.source.revision, /^[0-9a-f]{40}$/u, 'catalog revision must be a full commit SHA');
  assert.equal(cache.source.releaseTag, 'catalog-v1', 'catalog release tag changed');
  assert.equal(cache.dishes.length, 10, 'the bounded cache must contain exactly 10 records');
  assert.equal(new Set(cache.dishes.map((dish) => dish.id)).size, cache.dishes.length,
    'cached dish ids must be unique');
  assert.equal(new Set(cache.dishes.map((dish) => dish.photo.file)).size, cache.dishes.length,
    'cached photo filenames must be unique');

  const pinnedRawUrl = `${rawCatalogRoot}${cache.source.revision}/catalog/index.json`;
  assert.match(pinnedRawUrl,
    /^https:\/\/raw\.githubusercontent\.com\/Ding-Ding-Projects\/dim-sum-photos\/[0-9a-f]{40}\/catalog\/index\.json$/u,
    'pinned raw catalog URL is malformed');

  const [catalog, release] = await Promise.all([
    loadPinnedCatalog(cache.source.revision),
    loadRelease(cache.source.releaseTag),
  ]);
  assert.equal(catalog.schemaVersion, '1.0.0', 'pinned catalog schema version changed');
  assert.ok(Array.isArray(catalog.dishes), 'pinned catalog must expose a dishes array');
  assert.equal(catalog.total, catalog.dishes.length, 'pinned catalog total must match its dishes array');
  assert.equal(release.tagName, cache.source.releaseTag, 'resolved release tag changed');
  assert.equal(release.isDraft, false, 'catalog release must be published');
  assert.equal(release.isPrerelease, false, 'catalog release must not be a prerelease');
  assert.ok(Array.isArray(release.assets), 'catalog release must expose an assets array');

  onlineEvidence = { cache, catalog, release, pinnedRawUrl };
  return onlineEvidence;
}

test('pinned public catalog metadata and published assets match all 10 cached records', async (t) => {
  const { cache: originalCache, catalog, release, pinnedRawUrl } = await evidence();
  const cache = applyRequestedNegativeControl(originalCache);

  for (const dish of cache.dishes) {
    const matchingCatalogRecords = catalog.dishes.filter((candidate) => candidate.id === dish.id);
    assert.equal(matchingCatalogRecords.length, 1, `${dish.id}: expected exactly one pinned catalog record`);
    const matchingAssets = release.assets.filter((asset) => asset.name === dish.photo.file);
    assert.equal(matchingAssets.length, 1, `${dish.id}: expected exactly one published release asset`);
    assertDishMatches(dish, matchingCatalogRecords[0], matchingAssets[0], cache.source.releaseTag);
  }

  t.diagnostic(`Verified ${cache.dishes.length}/${cache.dishes.length} cached records at ${pinnedRawUrl}`);
  t.diagnostic(`Verified ${cache.dishes.length}/${cache.dishes.length} published ${cache.source.releaseTag} assets`);
});

test('validator rejects valid-looking changed release URLs and SHA-256 digests', async () => {
  const { cache, catalog, release } = await evidence();
  const dish = cache.dishes[0];
  const catalogDish = catalog.dishes.find((candidate) => candidate.id === dish.id);
  const asset = release.assets.find((candidate) => candidate.name === dish.photo.file);

  const changedDigestDish = {
    ...dish,
    photo: { ...dish.photo, sha256: changedSha256(dish.photo.sha256) },
  };
  assert.match(changedDigestDish.photo.sha256, /^[0-9a-f]{64}$/u);
  assert.throws(
    () => assertDishMatches(changedDigestDish, catalogDish, asset, cache.source.releaseTag),
    /release asset digest changed/u,
  );

  const changedUrlDish = {
    ...dish,
    photo: { ...dish.photo, url: changedReleaseUrl(dish.photo.url) },
  };
  assert.match(changedUrlDish.photo.url,
    /^https:\/\/github\.com\/Ding-Ding-Projects\/dim-sum-photos\/releases\/download\/catalog-v1\/[a-z0-9-]+\.png$/u);
  assert.throws(
    () => assertDishMatches(changedUrlDish, catalogDish, asset, cache.source.releaseTag),
    /cached release URL changed/u,
  );
});
