#!/usr/bin/env node
/*
 * Regenerates ui-md3/site/changelog.data.js from facts that already exist:
 * the repository's published GitHub Releases and the commits between them.
 *
 * Nothing here invents a version, a date, or a change. Every entry is either
 * copied from the release payload or read out of `git log`, and a release with
 * no commits in its range is emitted with an empty change list rather than a
 * plausible-sounding filler line.
 *
 *   node ui-md3/scripts/build-changelog.mjs            # refresh from the GitHub API
 *   node ui-md3/scripts/build-changelog.mjs --check    # fail if the file is stale
 *
 * Requires an authenticated `gh` and a checkout whose release tags are fetched.
 */
import { execFileSync } from 'node:child_process';
import { readFile, writeFile } from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const scriptDir = path.dirname(fileURLToPath(import.meta.url));
const uiDir = path.resolve(scriptDir, '..');
const outputPath = path.join(uiDir, 'site', 'changelog.data.js');
const REPO = process.env.BAMBU_CHANGELOG_REPO || 'Ding-Ding-Projects/BambuStudio';
const checkOnly = process.argv.includes('--check');

const run = (file, args) =>
  execFileSync(file, args, { encoding: 'utf8', maxBuffer: 64 * 1024 * 1024 }).trim();

const gh = (endpoint) => JSON.parse(run('gh', ['api', endpoint, '--paginate', '--slurp']));

/** `Bambu Studio MD3 v27 — Water Chestnut Cake 馬蹄糕 (superseded master build)` */
function parseReleaseName(name) {
  const dish = /—\s*([^()—]+?)\s*([㐀-鿿][㐀-鿿　-〿]*)\s*(?:\(([^)]*)\))?\s*$/u.exec(name);
  // `v27` is a release number; the `v02` inside the app version `02.08.01.55`
  // is not, so a v-token followed by a digit or a dot is rejected.
  const version = /\bv(\d+)(?![\d.])/.exec(name);
  return {
    version: version ? `v${version[1]}` : '',
    ordinal: version ? Number(version[1]) : 0,
    dishEnglish: dish ? dish[1].trim() : '',
    dishCantonese: dish ? dish[2].trim() : '',
    qualifier: dish?.[3]?.trim() || /\(([^)]*)\)\s*$/.exec(name)?.[1]?.trim() || '',
  };
}

/** Pulls the release body's `- Key: value` metadata without guessing missing keys. */
function parseBodyMetadata(body) {
  const metadata = {};
  for (const line of String(body || '').split(/\r?\n/)) {
    const match = /^\s*[-*]\s*([A-Za-z][A-Za-z0-9 /_-]*?)\s*:\s*(.+?)\s*$/.exec(line);
    if (match) metadata[match[1].trim().toLowerCase()] = match[2].trim();
  }
  return metadata;
}

/**
 * Mechanical category from the commit subject's leading verb. The viewer states
 * that categories are derived this way, so a miscategorized line is visibly a
 * mapping artifact rather than an invented claim about the change.
 */
function categorize(subject) {
  const head = subject.trim().split(/\s+/)[0].toLowerCase().replace(/[:,.]$/, '');
  if (['fix', 'fixes', 'fixed', 'repair', 'unbreak', 'restore', 'correct'].includes(head)) return 'fixed';
  if (['add', 'adds', 'added', 'introduce', 'implement', 'enable', 'ship'].includes(head)) return 'added';
  if (['remove', 'removes', 'removed', 'delete', 'drop', 'retire'].includes(head)) return 'removed';
  if (['document', 'documents', 'documented', 'handoff', 'record', 'note'].includes(head)) return 'documented';
  return 'changed';
}

/*
 * A tag this checkout has not fetched is normal, not an error: the release
 * body's own `Commit:` line is the fallback. git's complaint is silenced so a
 * routine miss does not read like a failure in the log.
 */
function tagCommit(tag) {
  try {
    return execFileSync('git', ['rev-list', '-n', '1', tag], {
      encoding: 'utf8',
      stdio: ['ignore', 'pipe', 'ignore'],
    }).trim();
  } catch {
    return '';
  }
}

function commitsBetween(fromCommit, toCommit) {
  // The oldest release has no predecessor to diff against; listing the whole
  // upstream history under it would be noise, not that release's changes.
  if (!toCommit || !fromCommit) return [];
  const range = `${fromCommit}..${toCommit}`;
  let output = '';
  try {
    output = run('git', ['log', '--no-merges', '--format=%H%x1f%s%x1f%aI', range]);
  } catch {
    return [];
  }
  if (!output) return [];
  return output.split('\n').map((line) => {
    const [sha, subject, authored] = line.split('');
    return { sha, short: sha.slice(0, 9), subject, authored, category: categorize(subject) };
  });
}

let payloadPages;
try {
  payloadPages = gh(`repos/${REPO}/releases?per_page=100`);
} catch (error) {
  // The committed data file is the source of truth for the site; this script
  // only refreshes it. A GitHub outage must not fail a deploy that is not
  // otherwise changing the changelog, so --check reports and stands down.
  if (checkOnly) {
    console.warn(`Skipped the changelog freshness check: the releases API is unavailable (${error.message.split('\n')[0]}).`);
    process.exit(0);
  }
  throw error;
}

const releases = payloadPages
  .flat()
  .filter((release) => !release.draft)
  .sort((a, b) => new Date(a.published_at) - new Date(b.published_at));

if (!releases.length) throw new Error(`No published releases found for ${REPO}.`);

const entries = [];
let previousCommit = '';
for (const release of releases) {
  const parsed = parseReleaseName(release.name || release.tag_name);
  const metadata = parseBodyMetadata(release.body);
  const commitFromBody = /^([0-9a-f]{7,40})$/i.exec(metadata.commit || '')?.[1] || '';
  const commit = tagCommit(release.tag_name) || commitFromBody;
  const changes = commitsBetween(previousCommit, commit);
  const baseline = !previousCommit;
  // Distinguishes "tagged the same commit again" from "no commits recorded",
  // so the viewer never has to guess which of the two it is looking at.
  const sameCommit = Boolean(previousCommit && commit && previousCommit === commit);
  if (commit) previousCommit = commit;

  entries.push({
    tag: release.tag_name,
    name: release.name || release.tag_name,
    version: parsed.version || release.tag_name,
    ordinal: parsed.ordinal,
    dish: { en: parsed.dishEnglish, yue: parsed.dishCantonese },
    qualifier: parsed.qualifier,
    published: release.published_at,
    url: release.html_url,
    prerelease: Boolean(release.prerelease),
    // The first published release predates any recorded range, so it lists no changes.
    baseline,
    sameCommit,
    commit,
    build: metadata.version || '',
    workflow: metadata.workflow || '',
    installScope: metadata['install scope'] || '',
    signing: metadata.signing || '',
    sbom: metadata.sbom || '',
    assets: (release.assets || []).map((asset) => ({
      name: asset.name,
      bytes: asset.size,
      url: asset.browser_download_url,
    })),
    changes: changes.map(({ sha, short, subject, category }) => ({ sha, short, subject, category })),
  });
}

const latest = releases.reduce((newest, release) =>
  new Date(release.published_at) > new Date(newest.published_at) ? release : newest);

const payload = {
  repository: REPO,
  latestTag: latest.tag_name,
  releaseCount: entries.length,
  // Categories are derived from each commit subject's leading verb; the viewer says so.
  categoryDerivation: 'leading-verb of the commit subject',
  releases: entries.reverse(),
};

const banner = `/* GENERATED FILE — do not edit by hand.
 * Source: published GitHub Releases of ${REPO} plus the commits between their tags.
 * Regenerate: node ui-md3/scripts/build-changelog.mjs
 * Every version, date, commit and asset below is copied from those two sources.
 */\n`;
const serialized = `${banner}window.BAMBU_CHANGELOG = ${JSON.stringify(payload, null, 2)};\n`;

if (checkOnly) {
  const current = await readFile(outputPath, 'utf8').catch(() => '');
  if (current !== serialized) {
    console.error('changelog.data.js is stale — rerun: node ui-md3/scripts/build-changelog.mjs');
    process.exit(1);
  }
  console.log(`changelog.data.js is current (${entries.length} releases).`);
} else {
  await writeFile(outputPath, serialized);
  console.log(`Wrote ${path.relative(uiDir, outputPath)} — ${entries.length} releases, latest ${payload.latestTag}.`);
}
