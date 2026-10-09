// The settings page: reads and writes the extension's settings, checks the
// connection to Bambu Studio MD3 and shows the recent handoffs. Every change
// is saved at once and announced in the page's status line.

import { HOST_NAME, LOG_KEY, SETTINGS_KEY, normalizeSettings, parseSiteList } from './capture-rules.js';
import { checkConnection } from './capture-service.js';
import { createTranslator, loadCatalogues } from './i18n.js';
import { connectionMessage, languageOptions, logRows, typeRows } from './options-model.js';

const $ = (id) => document.getElementById(id);

const state = {
  catalogues: {},
  settings: normalizeSettings(null),
  log: [],
  connection: null,
  status: null,
  excludedError: null,
  checking: false,
};

function translator() {
  return createTranslator(state.catalogues, state.settings.language, chrome.i18n.getUILanguage());
}

// Writes one message into an element: the primary text, and in bilingual mode
// the Cantonese text below it with its own language tag.
function putText(element, t, key, substitutions) {
  const { primary, primaryLang, secondary, secondaryLang } = t.parts(key, substitutions);
  element.textContent = primary;
  if (primaryLang !== t.htmlLang) element.lang = primaryLang;
  else element.removeAttribute('lang');
  if (secondary) {
    const line = document.createElement('span');
    line.className = 'secondary';
    line.lang = secondaryLang;
    line.textContent = secondary;
    element.append(line);
  }
}

// The type checkboxes are built once, so a re-render after a save never takes
// keyboard focus away from the box the person just changed.
function buildTypes() {
  $('types').replaceChildren(...typeRows(state.settings).map((row) => {
    const wrapper = document.createElement('div');
    wrapper.className = 'check';
    const input = document.createElement('input');
    input.type = 'checkbox';
    input.id = row.inputId;
    input.addEventListener('change', () => {
      const checked = input.checked;
      save((settings) => { settings.types[row.id] = checked; });
    });
    const label = document.createElement('label');
    label.htmlFor = row.inputId;
    label.dataset.i18n = row.labelKey;
    wrapper.append(input, label);
    return wrapper;
  }));
}

function renderTypes() {
  for (const row of typeRows(state.settings)) $(row.inputId).checked = row.checked;
}

function renderLanguage(t, settled) {
  const select = $('language');
  const shown = select.value;
  select.replaceChildren(...languageOptions(t).map((option) => {
    const element = document.createElement('option');
    element.value = option.value;
    element.textContent = option.label;
    if (option.lang) element.lang = option.lang;
    return element;
  }));
  select.value = settled || !shown ? state.settings.language : shown;
}

function renderLog(t) {
  const rows = logRows(state.log, t);
  $('recent-empty').hidden = rows.length > 0;
  $('recent').hidden = rows.length === 0;
  $('clear-recent').disabled = rows.length === 0;
  const labels = {
    time: t.text('colTime'), file: t.text('colFile'), site: t.text('colSite'), outcome: t.text('colOutcome'),
  };
  $('recent').tBodies[0].replaceChildren(...rows.map((row) => {
    const tr = document.createElement('tr');
    const cell = (label, className) => {
      const td = document.createElement('td');
      td.dataset.label = label;
      if (className) td.className = className;
      tr.append(td);
      return td;
    };
    // One line per language, each tagged with its own language.
    const lines = (parts, className) => [parts.primary, parts.secondary].filter(Boolean).map((text, index) => {
      const span = document.createElement('span');
      span.className = className;
      span.textContent = text;
      const lang = index === 0 ? parts.primaryLang : parts.secondaryLang;
      if (lang !== t.htmlLang) span.lang = lang;
      return span;
    });
    const time = document.createElement('time');
    time.dateTime = row.dateTime;
    time.textContent = row.time;
    cell(labels.time, 'when').append(time);
    cell(labels.file, 'file').textContent = row.file;
    cell(labels.site).textContent = row.site;
    const outcomeCell = cell(labels.outcome);
    outcomeCell.append(...lines(row.outcome, 'outcome'), ...lines(row.detail, 'detail'));
    if (row.fromLink) outcomeCell.append(...lines(t.parts('originLink'), 'origin'));
    return tr;
  }));
}

function render() {
  const t = translator();
  document.documentElement.lang = t.htmlLang;
  document.title = t.text('extName');
  for (const element of document.querySelectorAll('[data-i18n]')) putText(element, t, element.dataset.i18n);

  $('host-name').textContent = HOST_NAME;
  $('extension-id').textContent = chrome.runtime.id;
  // While saves are still queued the controls already show the person's
  // latest choices; showing the stored values now would flick them back.
  const settled = pendingSaves === 0;
  if (settled) {
    $('enabled').checked = state.settings.enabled;
    $('notify').checked = state.settings.notifyOnFallback;
    renderTypes();
  }
  const excluded = $('excluded');
  if (document.activeElement !== excluded && !state.excludedError) excluded.value = state.settings.excludedSites.join('\n');

  const error = $('excluded-error');
  if (state.excludedError) {
    putText(error, t, 'excludedInvalid', [state.excludedError.join(', ')]);
    error.hidden = false;
    excluded.setAttribute('aria-invalid', 'true');
  } else {
    error.hidden = true;
    error.textContent = '';
    excluded.removeAttribute('aria-invalid');
  }

  const connection = connectionMessage(state.connection);
  putText($('connection-state'), t, connection.key, connection.substitutions);
  $('check-connection').disabled = state.checking;

  if (state.status) putText($('status'), t, state.status.key, state.status.substitutions);
  else $('status').textContent = '';

  renderLanguage(t, settled);
  renderLog(t);
}

function announce(key, substitutions) {
  state.status = { key, substitutions };
  render();
}

// Saves run one after another, each starting from the settings the previous
// one stored, so two quick changes never overwrite each other.
let saveChain = Promise.resolve();
let pendingSaves = 0;
function save(change) {
  pendingSaves += 1;
  saveChain = saveChain.then(async () => {
    const next = normalizeSettings(state.settings);
    change(next);
    try {
      await chrome.storage.local.set({ [SETTINGS_KEY]: normalizeSettings(next) });
      state.settings = normalizeSettings(next);
      pendingSaves -= 1;
      announce('saved');
    } catch (error) {
      // The controls return to the stored values once nothing else is queued.
      pendingSaves -= 1;
      announce('saveFailed', [String(error?.message ?? error)]);
    }
  });
  return saveChain;
}

async function saveSites() {
  const { patterns, invalid } = parseSiteList($('excluded').value);
  state.excludedError = invalid.length ? invalid : null;
  await save((settings) => { settings.excludedSites = patterns; });
  if (!invalid.length) $('excluded').value = patterns.join('\n');
}

async function runConnectionCheck() {
  if (state.checking) return;
  state.checking = true;
  state.connection = { checking: true };
  render();
  try {
    state.connection = await checkConnection(chrome);
  } finally {
    state.checking = false;
    render();
  }
}

async function copyExtensionId() {
  try {
    await navigator.clipboard.writeText(chrome.runtime.id);
    announce('copied');
  } catch {
    announce('copyFailed');
  }
}

async function clearRecent() {
  try {
    await chrome.storage.local.remove(LOG_KEY);
    state.log = [];
    announce('recentCleared');
  } catch (error) {
    announce('saveFailed', [String(error?.message ?? error)]);
  }
}

async function reload() {
  const stored = await chrome.storage.local.get([SETTINGS_KEY, LOG_KEY]);
  state.settings = normalizeSettings(stored[SETTINGS_KEY]);
  state.log = Array.isArray(stored[LOG_KEY]) ? stored[LOG_KEY] : [];
}

async function start() {
  try {
    state.catalogues = await loadCatalogues(async (path) => (await fetch(chrome.runtime.getURL(path))).json());
  } catch {
    state.catalogues = {};
  }
  await reload();
  buildTypes();
  // Each change keeps the value the person chose when it happened; a render
  // for an earlier save must not change what a later save stores.
  $('enabled').addEventListener('change', (event) => {
    const enabled = event.target.checked;
    save((settings) => { settings.enabled = enabled; });
  });
  $('notify').addEventListener('change', (event) => {
    const notify = event.target.checked;
    save((settings) => { settings.notifyOnFallback = notify; });
  });
  $('language').addEventListener('change', (event) => {
    const language = event.target.value;
    save((settings) => { settings.language = language; });
  });
  $('save-sites').addEventListener('click', saveSites);
  $('excluded').addEventListener('keydown', (event) => {
    if (event.key === 'Enter' && (event.ctrlKey || event.metaKey)) {
      event.preventDefault();
      saveSites();
    }
  });
  $('check-connection').addEventListener('click', runConnectionCheck);
  $('copy-id').addEventListener('click', copyExtensionId);
  $('clear-recent').addEventListener('click', clearRecent);
  chrome.storage.onChanged.addListener(async (changes, area) => {
    if (area !== 'local' || (!changes[SETTINGS_KEY] && !changes[LOG_KEY])) return;
    await reload();
    render();
  });
  render();
  // The page is ready for input once its controls hold the stored values.
  document.querySelector('main').removeAttribute('aria-busy');
}

start();
