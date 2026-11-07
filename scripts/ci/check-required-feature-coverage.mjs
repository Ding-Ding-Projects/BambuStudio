import fs from 'node:fs';
import { fileURLToPath } from 'node:url';

// Hand-maintained completeness contract. Never derive this list from inventory rows.
const EXPECTED_IDS = `
job-experience-document-label language funny-emoji personal-vocabulary vocabulary-drop-apply
school-mode narration action-sound-effects scheduled-settings display-name adhd-modes dim-sum
dim-sum-photo-source material-design workflow-navigation motion live-charts appearance-editor
color-picker logo-customization overlay-panels collapse-filters accessibility-sizing functional-ui
onboarding-wizard extension-support tabs regex-search command-palette context-menus menu-shortcuts
rich-controls guided-forms info-icon-guidance settings-explanations blank-editors super-confirmation
element-locks support-tickets unlock-ladder authenticator qr-pairing honest-monetization local-history
exports bulk-actions changelog offline-docs rendered-provider-text notifications file-converter
base64-tools virtual-disk-manager minecraft-server-manager minecraft-world-generator ollama-suite
external-editor download-handoff forge-publishing progress-recovery drawesome-drawing completeness-parity
front-provenance product-evidence screen-recording design-reference-parity design-folder responsive-layout-matrix
public-screenshot-gallery landing-site mobile-documentation-site installer-download-button feature-articles
vendored-fonts shared-link-embed homepage-link vocabulary-unlock-boundary documentation-companion-delivery
status-hub discord-status-bridge panic-webhooks tidbyt-displays build-entrypoints fresh-build-run-command
dependency-fetcher bundled-dependencies squirrel-installer self-signing auto-updates app-icon release-workflow
release-timing release-line-counts release-dim-sum-photo dim-sum-code-names ci-bootstrap runner-selection
encrypted-public-builder tabbed-readme human-time-estimate sanitized-instruction-copy agents-md-vocabulary-block
vocabulary-hash-lock roadmap-checklist feature-docs postman-collections wiki-documentation-sync handoff-record
closeout-prompt discussion-records project-board operational-skill instruction-prompt-banner
instruction-single-file-editions project-profile roblox-model-catalogue roblox-visual-realism
`.trim().split(/\s+/);
const EXPECTED_SURFACES = ['native-application', 'embedded-owned-views', 'embedded-third-party-views',
  'lan-sender', 'lan-hosting-setup', 'build-companion', 'documentation-companion', 'public-documentation', 'repository-delivery'];
const EXPECTED_LEGACY = ['critic-accessibility-and-sizing', 'critic-article-structure', 'critic-bundled-assets-and-fonts',
  'critic-discard-history', 'critic-frameless-window-chrome', 'critic-funny-level-first-run-disclosure', 'critic-no-analytics',
  'critic-no-promotional-prompts', 'critic-provider-markup', 'critic-remote-hosted-pages', 'critic-status-corrections',
  'discord-embed', 'landing-and-docs-site', 'line-count', 'readme-and-recording'];
const EXPECTED_NATIVE_IDS = 'shell prepare preview print monitor farm home-web ink-web printer-web project calibration preferences parameters wizard menus appearance-editor palette regex notifications history documentation changelog import-export model-creator smart-home schedules confirmations canonical-tools'.split(' ');
const PORTABLE_CLASSES = ['lan-hosting-setup', 'build-companion', 'documentation-companion'];
const STATES = new Set(['unknown', 'unverified', 'partial', 'source-verified']);
const APPLICABILITY = new Set(['applicable', 'conditional', 'not-applicable', 'suspended', 'external-owner',
  'covered-at-delivery-boundary', 'evidence-owner', 'owned-equivalent-required', 'scope-review-needed']);
function fail(code) { throw new Error(code); }
function exact(actual, expected, code) {
  if (!Array.isArray(actual) || actual.length !== expected.length || new Set(actual).size !== actual.length ||
      expected.some(id => !actual.includes(id))) fail(code);
}
export function validate(data) {
  if (EXPECTED_IDS.length !== 117 || new Set(EXPECTED_IDS).size !== 117) fail('invalid_expected_identity_contract');
  if (data.schemaVersion !== 1 || data.completionClaim !== false || data.expectedFeatureCount !== 117) fail('invalid_inventory_header');
  exact(data.features?.map(row => row.id), EXPECTED_IDS, 'identity_inventory_mismatch');
  exact(data.surfaces?.map(row => row.id), EXPECTED_SURFACES, 'surface_inventory_mismatch');
  exact(data.legacyCrosswalks?.map(row => row.legacyFamilyId), EXPECTED_LEGACY, 'legacy_inventory_mismatch');
  exact(data.nativeIndividualSurfaces?.map(row => row.id), EXPECTED_NATIVE_IDS, 'native_individual_omission');
  if (data.surfaceInventoryScope?.individualFeatureCoverage !== 'pending' || data.surfaceInventoryScope?.completionClaim !== false) fail('false_individual_completion');
  for (const surface of data.nativeIndividualSurfaces) {
    if (!surface.sourceAnchor || surface.sourceCommit !== data.recordCommit || surface.status !== 'unknown' || surface.featureCoverage !== 'pending' || !surface.classLinks?.length || surface.classLinks.some(id => !EXPECTED_SURFACES.includes(id))) fail('invalid_individual_reference');
  }
  const installer = data.features.find(row => row.id === 'squirrel-installer');
  for (const id of PORTABLE_CLASSES) {
    const record = installer.surfaceCoverage.find(row => row.surfaceId === id);
    if (record?.applicability !== 'not-applicable' || record.exceptionId !== 'portable-companion-format' || record.alternativeObligation !== 'single-portable-executable') fail('portable_exception_gap');
  }
  const drop = data.features.find(row => row.id === 'vocabulary-drop-apply').surfaceCoverage.find(row => row.surfaceId === 'public-documentation');
  if (drop?.applicability !== 'not-applicable' || drop.exceptionId !== 'documentation-website-drop-exclusion') fail('website_drop_exception_gap');
  for (const row of data.features) {
    if (!STATES.has(row.status) || row.completionClaim !== false) fail('unsupported_completion_claim');
    if (row.status === 'partial' && !row.evidence?.length) fail('inherited_historical_status');
    exact(row.surfaceCoverage?.map(item => item.surfaceId), EXPECTED_SURFACES, 'feature_surface_omission');
    for (const item of row.surfaceCoverage) {
      if (item.status === 'partial' && !item.evidence?.length) fail('inherited_historical_status');
      if (item.surfaceId === 'embedded-third-party-views' && item.applicability === 'owned-equivalent-required') fail('unproven_third_party_exemption');
      if (!APPLICABILITY.has(item.applicability) || !STATES.has(item.status) || item.runtimeStatus !== 'unverified' ||
          typeof item.reason !== 'string' || item.reason.trim().length < 30) fail('invalid_surface_record');
    }
  }
  for (const row of data.legacyCrosswalks) {
    if (!Array.isArray(row.featureIds) || !row.featureIds.length || row.featureIds.some(id => !EXPECTED_IDS.includes(id)) ||
        typeof row.rationale !== 'string' || row.rationale.length < 30) fail('invalid_legacy_crosswalk');
  }
  for (const unit of data.knownSourceUnits) {
    if (!/^[a-f0-9]{40}$/.test(unit.sourceCommit) || unit.status !== 'source-verified' ||
        unit.runtimeStatus !== 'unverified-for-complete-contract' || unit.featureIds.some(id => !EXPECTED_IDS.includes(id)) ||
        unit.surfaces.some(id => !EXPECTED_SURFACES.includes(id))) fail('invalid_source_unit');
  }
  return { features: data.features.length, surfaceClasses: data.surfaces.length, nativeIndividuals: data.nativeIndividualSurfaces.length,
    surfaceRecords: data.features.reduce((count, row) => count + row.surfaceCoverage.length, 0), crosswalks: data.legacyCrosswalks.length };
}
const filename = process.argv.find(arg => arg.endsWith('.json')) || fileURLToPath(new URL('../../design/required-feature-coverage.json', import.meta.url));
const input = JSON.parse(fs.readFileSync(filename, 'utf8'));
const result = validate(input);
if (process.argv.includes('--self-test')) {
  const cases = [
    ['identity_inventory_mismatch', value => value.features.pop()],
    ['surface_inventory_mismatch', value => value.surfaces.pop()],
    ['feature_surface_omission', value => value.features[0].surfaceCoverage.pop()],
    ['legacy_inventory_mismatch', value => value.legacyCrosswalks.pop()],
    ['native_individual_omission', value => value.nativeIndividualSurfaces.pop()],
    ['portable_exception_gap', value => delete value.features.find(row => row.id === 'squirrel-installer').surfaceCoverage.find(row => row.surfaceId === 'build-companion').alternativeObligation],
    ['website_drop_exception_gap', value => value.features.find(row => row.id === 'vocabulary-drop-apply').surfaceCoverage.find(row => row.surfaceId === 'public-documentation').applicability = 'applicable'],
    ['inherited_historical_status', value => value.features[0].surfaceCoverage[1].status = 'partial'],
    ['unproven_third_party_exemption', value => value.features[0].surfaceCoverage[2].applicability = 'owned-equivalent-required']
  ];
  for (const [expected, mutate] of cases) {
    const broken = structuredClone(input); mutate(broken);
    let observed = null;
    try { validate(broken); } catch (error) { observed = error.message; }
    if (observed !== expected) fail('negative_mutation_not_rejected:' + expected);
  }
  console.log('PASS: nine independent omission and claim mutations rejected; original inventory remains valid.');
}
console.log('PASS: ' + result.features + ' identities, ' + result.surfaceClasses + ' surface classes, ' + result.nativeIndividuals + ' historical individual surfaces, ' + result.surfaceRecords + ' surface records, ' + result.crosswalks + ' legacy crosswalks. Inventory consistency is not runtime acceptance.');
