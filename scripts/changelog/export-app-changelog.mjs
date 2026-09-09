#!/usr/bin/env node
/*
 * Writes resources/changelog/changelog.json — the data the in-app changelog
 * viewer (src/slic3r/GUI/ChangelogDialog.cpp) renders. It reuses the parsing
 * of ui-md3/scripts/build-changelog.mjs, so the app and the site derive their
 * versions, dates, categories and commit lines from exactly the same rules.
 *
 *   node scripts/changelog/export-app-changelog.mjs             # refresh from the GitHub Releases API
 *   node scripts/changelog/export-app-changelog.mjs --offline   # rebuild from ui-md3/site/changelog.data.js
 *   node scripts/changelog/export-app-changelog.mjs --check     # exit 1 when the committed JSON is stale
 *
 * Every commit SHA the file references is resolved with `git rev-parse` and
 * confirmed to be a commit object in this repository; the export fails
 * otherwise, so the viewer can never link to a commit that does not exist.
 *
 * The JSON is committed so the application has data without Node at build time.
 */
import { execFileSync } from 'node:child_process';
import { readFile, writeFile, mkdir } from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

import { REPO, gh, buildReleaseEntries } from '../../ui-md3/scripts/build-changelog.mjs';

const scriptDir = path.dirname(fileURLToPath(import.meta.url));
const repoRoot = path.resolve(scriptDir, '..', '..');
const outputPath = path.join(repoRoot, 'resources', 'changelog', 'changelog.json');
const siteDataPath = path.join(repoRoot, 'ui-md3', 'site', 'changelog.data.js');

const args = new Set(process.argv.slice(2));
const checkOnly = args.has('--check');
const offline = args.has('--offline');

const SCHEMA_VERSION = 1;

/**
 * Resolves a SHA prefix to the full 40-character commit id, or throws when the
 * repository does not hold such a commit. `rev-parse --verify <sha>^{commit}`
 * rejects blobs, trees, tags and unknown or ambiguous prefixes in one call.
 */
function resolveCommit(sha, context) {
  if (!/^[0-9a-f]{7,40}$/i.test(sha)) {
    throw new Error(`${context}: "${sha}" is not a commit SHA.`);
  }
  try {
    return execFileSync('git', ['rev-parse', '--verify', '--quiet', `${sha}^{commit}`], {
      cwd: repoRoot,
      encoding: 'utf8',
      stdio: ['ignore', 'pipe', 'ignore'],
    }).trim();
  } catch {
    throw new Error(
      `${context}: commit ${sha} does not exist in this repository. ` +
      'Fetch the release tags (git fetch --tags) before exporting.'
    );
  }
}

/** Site-shaped release records (newest first) from the GitHub Releases API. */
function releasesFromApi() {
  const releases = gh(`repos/${REPO}/releases?per_page=100`)
    .flat()
    .filter((release) => !release.draft)
    .sort((a, b) => new Date(a.published_at) - new Date(b.published_at));
  if (!releases.length) throw new Error(`No published releases found for ${REPO}.`);
  return buildReleaseEntries(releases).reverse();
}

/** Site-shaped release records from the committed site data file. */
async function releasesFromSiteData() {
  const source = await readFile(siteDataPath, 'utf8');
  const start = source.indexOf('{');
  const end = source.lastIndexOf('}');
  if (start < 0 || end < start) throw new Error(`${siteDataPath} carries no JSON payload.`);
  const payload = JSON.parse(source.slice(start, end + 1));
  if (!Array.isArray(payload.releases) || !payload.releases.length) {
    throw new Error(`${siteDataPath} lists no releases.`);
  }
  return payload.releases;
}

/** ISO instant -> calendar date (UTC) as YYYY-MM-DD. */
function isoDate(instant) {
  const match = /^(\d{4}-\d{2}-\d{2})T/.exec(String(instant || ''));
  if (!match) throw new Error(`Release timestamp "${instant}" is not an ISO-8601 instant.`);
  return match[1];
}

function toAppRelease(site) {
  const context = `release ${site.tag}`;
  const commit = site.commit ? resolveCommit(site.commit, context) : '';
  const entries = (site.changes || []).map((change) => {
    const sha = resolveCommit(change.sha, `${context}, change "${change.subject}"`);
    return {
      sha,
      short: sha.slice(0, 9),
      text: String(change.subject || '').trim(),
      category: change.category || 'changed',
    };
  });
  return {
    tag: site.tag,
    version: site.version || site.tag,
    ordinal: Number(site.ordinal) || 0,
    date: isoDate(site.published),
    published: site.published,
    codeName: { en: site.dish?.en || '', yue: site.dish?.yue || '' },
    qualifier: site.qualifier || '',
    url: site.url || '',
    prerelease: Boolean(site.prerelease),
    baseline: Boolean(site.baseline),
    sameCommit: Boolean(site.sameCommit),
    commit,
    build: site.build || '',
    entries,
  };
}

async function main() {
  let siteReleases;
  if (offline) {
    siteReleases = await releasesFromSiteData();
  } else {
    try {
      siteReleases = releasesFromApi();
    } catch (error) {
      if (checkOnly) {
        console.warn(`Skipped the app changelog freshness check: the releases API is unavailable (${error.message.split('\n')[0]}).`);
        process.exit(0);
      }
      throw error;
    }
  }

  const releases = siteReleases.map(toAppRelease);
  const entryCount = releases.reduce((sum, release) => sum + release.entries.length, 0);

  const previous = await readFile(outputPath, 'utf8').catch(() => '');
  const previousDoc = previous ? JSON.parse(previous) : null;

  const payload = {
    schema: SCHEMA_VERSION,
    repository: REPO,
    commitUrlTemplate: `https://github.com/${REPO}/commit/{sha}`,
    source: offline ? 'ui-md3/site/changelog.data.js' : 'GitHub Releases API + git log',
    // Kept stable across identical refreshes so --check compares content, not clocks.
    generated: previousDoc && JSON.stringify(previousDoc.releases) === JSON.stringify(releases)
      ? previousDoc.generated
      : new Date().toISOString(),
    categoryDerivation: 'leading-verb of the commit subject',
    releaseCount: releases.length,
    entryCount,
    releases,
  };
  const serialized = `${JSON.stringify(payload, null, 2)}\n`;

  if (checkOnly) {
    if (previous !== serialized) {
      console.error('resources/changelog/changelog.json is stale — rerun: node scripts/changelog/export-app-changelog.mjs');
      process.exit(1);
    }
    console.log(`resources/changelog/changelog.json is current (${releases.length} releases, ${entryCount} entries).`);
    return;
  }

  // A shorter file than the committed one is a symptom (partial API page,
  // missing tags), not an update.
  if (previousDoc && releases.length < previousDoc.releaseCount) {
    throw new Error(
      `Refusing to write a shorter changelog: ${releases.length} releases now, ` +
      `${previousDoc.releaseCount} in the committed file.`
    );
  }

  await mkdir(path.dirname(outputPath), { recursive: true });
  await writeFile(outputPath, serialized);
  console.log(
    `Wrote ${path.relative(repoRoot, outputPath)} — ${releases.length} releases, ` +
    `${entryCount} entries, every SHA verified with git rev-parse.`
  );
}

await main();
