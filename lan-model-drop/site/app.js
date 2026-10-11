// The sender page: reads an invite code, lets people choose or drop models,
// sends them one by one to the drop box on this network and reports each
// result. Talks only to this site (api/drop); loads nothing else.
import { createTranslator, modeFromLanguages, normalizeMode, sizeParts } from './i18n.js';
import { normalizeCode, takeInviteCode, validCode } from './invite.js';
import { outcomeFor, precheck } from './outcome.js';

// The invite code leaves the address bar before anything else runs.
const invite = takeInviteCode(window);

const LANGUAGE_KEY = 'lan-model-drop.language';
const $ = (id) => document.getElementById(id);

function meta(name) {
  return document.querySelector(`meta[name="${name}"]`)?.content ?? '';
}

function metaNumber(name, fallback) {
  const value = Number(meta(name));
  return Number.isFinite(value) && value > 0 ? value : fallback;
}

const settings = {
  stationName: meta('drop-station-name').startsWith('{{') ? 'Bambu Studio' : meta('drop-station-name') || 'Bambu Studio',
  maxBytes: metaNumber('drop-max-bytes', 268435456),
  ttlHours: metaNumber('drop-ttl-hours', 24),
  protocol: metaNumber('drop-protocol', 1),
};

// Browser storage holds only the language choice, and the page works the
// same without it (private windows, blocked storage).
function storedMode() {
  try {
    return normalizeMode(window.localStorage.getItem(LANGUAGE_KEY));
  } catch {
    return null;
  }
}

function storeMode(mode) {
  try {
    window.localStorage.setItem(LANGUAGE_KEY, mode);
  } catch {
    // Not remembered; the page still switches.
  }
}

const state = {
  t: createTranslator(storedMode() ?? modeFromLanguages(navigator.languages ?? [navigator.language])),
  files: [],
  sending: false,
  nextId: 1,
  live: null,
  codeHelp: { key: 'codeHelp' },
  codeError: null,
};

// ------------------------------------------------------------- rendering

function sizeValue(bytes) {
  const { key, value } = sizeParts(bytes);
  return { key, values: [value] };
}

// Message substitutions: a size becomes a translated unit, everything else
// stays as given.
function valuesOf(message) {
  if (message.size !== undefined) return [sizeValue(message.size)];
  return message.values ?? [];
}

// Writes a message into an element. In bilingual mode the Cantonese line
// follows the English one in its own span, marked as Cantonese.
function render(element, message) {
  const { primary, primaryLang, secondary, secondaryLang } = state.t.parts(message.key, valuesOf(message));
  element.textContent = primary;
  if (state.t.mode === 'yue_HK') element.setAttribute('lang', primaryLang);
  else element.removeAttribute('lang');
  if (secondary) {
    const line = document.createElement('span');
    line.className = 'secondary';
    line.lang = secondaryLang;
    line.textContent = secondary;
    element.append(line);
  }
}

function text(message) {
  return state.t.text(message.key, valuesOf(message));
}

function icon(name) {
  const glyph = document.createElement('span');
  glyph.setAttribute('data-icon', '');
  glyph.setAttribute('aria-hidden', 'true');
  glyph.textContent = name;
  return glyph;
}

const STATUS_ICONS = {
  ready: 'view_in_ar',
  waiting: 'hourglass_top',
  sending: 'progress_activity',
  sent: 'check_circle',
  failed: 'error',
  rejected: 'block',
};

function stateMessage(entry) {
  switch (entry.status) {
    case 'waiting': return { key: 'stateWaiting' };
    case 'sending': return { key: 'stateSending', values: [Math.round(entry.progress * 100)] };
    case 'sent': return { key: 'stateSent' };
    case 'failed':
    case 'rejected': return { key: 'stateNotSent', values: [{ key: entry.reason.key, values: valuesOf(entry.reason) }] };
    default: return { key: 'stateReady' };
  }
}

function fileRow(entry) {
  const row = document.createElement('li');
  row.className = 'file-row';
  row.dataset.status = entry.status;
  row.dataset.id = String(entry.id);

  const tile = document.createElement('span');
  tile.className = 'file-icon';
  tile.append(icon(STATUS_ICONS[entry.status] ?? 'view_in_ar'));

  const main = document.createElement('div');
  main.className = 'file-main';
  const name = document.createElement('p');
  name.className = 'file-name';
  name.textContent = entry.file.name;
  const details = document.createElement('p');
  details.className = 'file-meta';
  const size = document.createElement('span');
  size.className = 'mono';
  render(size, sizeValue(entry.file.size));
  const separator = document.createElement('span');
  separator.setAttribute('aria-hidden', 'true');
  separator.textContent = '·';
  const status = document.createElement('span');
  status.className = 'file-state';
  render(status, stateMessage(entry));
  details.append(size, separator, status);
  main.append(name, details);

  if (entry.status === 'sending') {
    const track = document.createElement('div');
    track.className = 'progress';
    track.setAttribute('role', 'progressbar');
    track.setAttribute('aria-valuemin', '0');
    track.setAttribute('aria-valuemax', '100');
    track.setAttribute('aria-valuenow', String(Math.round(entry.progress * 100)));
    track.setAttribute('aria-label', text({ key: 'progressLabel', values: [entry.file.name] }));
    const fill = document.createElement('div');
    fill.className = 'progress-fill';
    fill.style.width = `${Math.round(entry.progress * 100)}%`;
    track.append(fill);
    main.append(track);
  }

  const remove = document.createElement('button');
  remove.type = 'button';
  remove.className = 'icon-button';
  remove.dataset.remove = String(entry.id);
  remove.setAttribute('aria-label', text({ key: 'removeFile', values: [entry.file.name] }));
  remove.title = text({ key: 'removeFile', values: [entry.file.name] });
  remove.disabled = state.sending;
  remove.append(icon('close'));

  row.append(tile, main, remove);
  return row;
}

function readyFiles() {
  return state.files.filter((entry) => entry.status === 'ready' || entry.status === 'failed');
}

function renderList() {
  const list = $('file-list');
  list.replaceChildren(...state.files.map(fileRow));
  $('empty-list').hidden = state.files.length > 0;
  const count = readyFiles().length;
  render($('send-label'), count > 0 ? { key: 'sendCount', values: [count] } : { key: 'send' });
  $('send').disabled = state.sending || count === 0;
  $('clear').disabled = state.sending || state.files.length === 0;
  $('choose').disabled = state.sending;
}

// Updates one row's progress in place, so a screen reader is not sent the
// whole list again on every step.
function renderProgress(entry) {
  const row = $('file-list').querySelector(`[data-id="${entry.id}"]`);
  if (!row) return;
  const percent = Math.round(entry.progress * 100);
  const track = row.querySelector('.progress');
  if (track) {
    track.setAttribute('aria-valuenow', String(percent));
    track.querySelector('.progress-fill').style.width = `${percent}%`;
  }
  const status = row.querySelector('.file-state');
  if (status) render(status, stateMessage(entry));
}

function setLive(message, tone = 'info') {
  state.live = message ? { message, tone } : null;
  renderLive();
}

function renderLive() {
  const live = $('live');
  if (!state.live) {
    live.textContent = '';
    delete live.dataset.tone;
    return;
  }
  live.dataset.tone = state.live.tone;
  render(live, state.live.message);
}

function setCodeError(message) {
  state.codeError = message;
  renderCodeField();
}

function renderCodeField() {
  const field = $('code-field');
  const error = $('code-error');
  const input = $('code');
  render($('code-help'), state.codeHelp);
  if (state.codeError) {
    render(error, state.codeError);
    error.hidden = false;
    field.classList.add('invalid');
    input.setAttribute('aria-invalid', 'true');
  } else {
    error.textContent = '';
    error.hidden = true;
    field.classList.remove('invalid');
    input.removeAttribute('aria-invalid');
  }
}

function renderStatic() {
  const { t } = state;
  document.documentElement.lang = t.htmlLang;
  document.title = t.text('pageTitle');
  for (const element of document.querySelectorAll('[data-i18n]')) render(element, { key: element.dataset.i18n });
  render($('station-line'), { key: 'stationLine', values: [settings.stationName] });
  render($('drop-hint'), { key: 'dropHint', values: [sizeValue(settings.maxBytes)] });
  render($('privacy-text'), { key: 'privacyText', values: [settings.ttlHours] });
  render($('footer'), { key: 'footer', values: [settings.protocol] });
  for (const button of document.querySelectorAll('.segmented button')) {
    button.setAttribute('aria-pressed', String(button.dataset.mode === t.mode));
  }
  renderCodeField();
  renderList();
  renderLive();
}

// ------------------------------------------------------------ choosing files

function addFiles(fileList) {
  if (state.sending) return;
  const added = [];
  for (const file of fileList) {
    const duplicate = state.files.some((entry) => entry.file.name === file.name && entry.file.size === file.size
      && entry.file.lastModified === file.lastModified && entry.status !== 'sent');
    if (duplicate) continue;
    const problem = precheck(file, settings.maxBytes);
    const entry = { id: state.nextId++, file, status: problem ? 'rejected' : 'ready', reason: problem, progress: 0 };
    state.files.push(entry);
    added.push(entry);
  }
  if (!added.length) return;
  renderList();
  setLive({ key: 'liveAdded', values: [added.length, readyFiles().length] });
}

function removeFile(id) {
  if (state.sending) return;
  const index = state.files.findIndex((entry) => entry.id === id);
  if (index < 0) return;
  state.files.splice(index, 1);
  renderList();
  // Keep the keyboard in the list, or move it to the next sensible control.
  const rows = $('file-list').querySelectorAll('[data-remove]');
  const next = rows[Math.min(index, rows.length - 1)];
  if (next) next.focus();
  else $('choose').focus();
}

// ---------------------------------------------------------------- sending

function sendOne(entry, code, sender, onProgress) {
  return new Promise((resolve) => {
    const request = new XMLHttpRequest();
    request.open('POST', 'api/drop');
    request.setRequestHeader('X-Drop-Code', code);
    request.setRequestHeader('X-Drop-Filename', encodeURIComponent(entry.file.name));
    if (sender) request.setRequestHeader('X-Drop-Sender', encodeURIComponent(sender));
    request.upload.addEventListener('progress', (event) => {
      if (event.lengthComputable && event.total > 0) onProgress(event.loaded / event.total);
    });
    request.addEventListener('load', () => {
      let body = null;
      try {
        body = JSON.parse(request.responseText);
      } catch {
        body = null;
      }
      resolve({ status: request.status, body, retryAfter: request.getResponseHeader('Retry-After') });
    });
    for (const type of ['error', 'abort', 'timeout']) request.addEventListener(type, () => resolve({ status: 0, body: null }));
    request.send(entry.file);
  });
}

async function sendAll(event) {
  event.preventDefault();
  if (state.sending) return;
  const codeInput = $('code');
  const code = normalizeCode(codeInput.value);
  if (code === '') {
    setCodeError({ key: 'codeMissing' });
    codeInput.focus();
    return;
  }
  if (!validCode(code)) {
    setCodeError({ key: 'codeFormat' });
    codeInput.focus();
    return;
  }
  setCodeError(null);
  const queue = readyFiles();
  if (!queue.length) {
    setLive({ key: 'liveNothing' }, 'error');
    $('choose').focus();
    return;
  }
  const sender = $('sender').value.replace(/\s+/g, ' ').trim();

  state.sending = true;
  for (const entry of queue) {
    entry.status = 'waiting';
    entry.reason = null;
    entry.progress = 0;
  }
  renderList();

  let sent = 0;
  let stopped = null;
  for (const [index, entry] of queue.entries()) {
    entry.status = 'sending';
    renderList();
    setLive({ key: 'liveSending', values: [index + 1, queue.length, entry.file.name] });
    const answer = await sendOne(entry, code, sender, (fraction) => {
      entry.progress = fraction;
      renderProgress(entry);
    });
    const outcome = outcomeFor(answer, settings.maxBytes);
    if (outcome.sent) {
      entry.status = 'sent';
      entry.progress = 1;
      sent += 1;
    } else {
      // A refusal that stops the send (wrong code, lockout, full box, no
      // connection) leaves the file ready to try again; a refusal of this
      // file itself (type, size, name) would only repeat.
      entry.status = outcome.stop ? 'failed' : 'rejected';
      entry.reason = outcome.reason;
    }
    if (outcome.codeError) setCodeError(outcome.codeError);
    if (outcome.stop) {
      stopped = outcome;
      for (const rest of queue.slice(index + 1)) rest.status = 'ready';
      break;
    }
  }

  state.sending = false;
  renderList();
  // The send button was disabled while it had the focus; give the keyboard
  // a place to continue from.
  if (document.activeElement === document.body || document.activeElement === null) {
    ($('send').disabled ? $('choose') : $('send')).focus();
  }
  if (stopped?.live) {
    setLive(stopped.live, 'error');
    if (stopped.codeError) codeInput.focus();
  } else {
    setLive({ key: 'liveDone', values: [sent, queue.length] }, sent === queue.length ? 'success' : 'error');
  }
}

// --------------------------------------------------------------- wiring

// Fills the code field from an invite link. The field stays editable.
function applyInvite({ present, code }) {
  if (!present) return;
  if (code) {
    $('code').value = code;
    state.codeHelp = { key: 'codeFromLink' };
  } else {
    state.codeHelp = { key: 'codeFromLinkInvalid' };
  }
  state.codeError = null;
  renderCodeField();
}

function setUp() {
  applyInvite(invite);

  for (const button of document.querySelectorAll('.segmented button')) {
    button.addEventListener('click', () => {
      state.t = createTranslator(button.dataset.mode);
      storeMode(state.t.mode);
      renderStatic();
    });
  }

  const picker = $('files');
  $('choose').addEventListener('click', () => picker.click());
  picker.addEventListener('change', () => {
    addFiles(picker.files ?? []);
    picker.value = '';
  });

  $('file-list').addEventListener('click', (event) => {
    const button = event.target.closest('[data-remove]');
    if (button) removeFile(Number(button.dataset.remove));
  });
  $('clear').addEventListener('click', () => {
    if (state.sending) return;
    state.files = [];
    renderList();
    setLive(null);
    $('choose').focus();
  });
  $('drop-form').addEventListener('submit', sendAll);
  $('code').addEventListener('input', () => {
    // Once the person types, the code is theirs, not the link's.
    state.codeHelp = { key: 'codeHelp' };
    state.codeError = null;
    renderCodeField();
  });

  // The whole page accepts dropped files, so a near miss does not make the
  // browser open the model instead.
  const zone = $('drop-zone');
  let depth = 0;
  const carriesFiles = (event) => Array.from(event.dataTransfer?.types ?? []).includes('Files');
  document.addEventListener('dragenter', (event) => {
    if (!carriesFiles(event)) return;
    event.preventDefault();
    depth += 1;
    zone.classList.add('dragging');
  });
  document.addEventListener('dragover', (event) => {
    if (!carriesFiles(event)) return;
    event.preventDefault();
    event.dataTransfer.dropEffect = state.sending ? 'none' : 'copy';
  });
  document.addEventListener('dragleave', () => {
    depth = Math.max(0, depth - 1);
    if (depth === 0) zone.classList.remove('dragging');
  });
  document.addEventListener('drop', (event) => {
    if (!carriesFiles(event)) return;
    event.preventDefault();
    depth = 0;
    zone.classList.remove('dragging');
    addFiles(event.dataTransfer.files);
  });

  // An invite link opened while the page is already showing.
  window.addEventListener('hashchange', () => applyInvite(takeInviteCode(window)));

  renderStatic();
}

setUp();
