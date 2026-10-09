// The extension's behaviour: take model downloads away from the browser and
// hand them to Bambu Studio MD3 through its native messaging host.
//
// The browser's own transfer is paused before anything is sent, and the
// browser is only told to cancel it after the application answers that it has
// queued the download behind its Start download dialog. Any other answer, or no
// answer, resumes the browser's transfer so the download is never lost. The
// extension itself never starts a transfer.

import {
  HOST_NAME, LOG_KEY, REPLY_TIMEOUT_MS, SETTINGS_KEY,
  appendToLog, buildCaptureMessage, buildHelloMessage, classifyHostError, contextMenuPatterns,
  decideCapture, decideLinkCapture, interpretCaptureReply, interpretHelloReply, logEntry,
  normalizeSettings, reasonMessageKey, sanitizeFileName,
} from './capture-rules.js';
import { createTranslator, loadCatalogues } from './i18n.js';

export const MENU_ID = 'open-link-in-bambu-studio-md3';
export const ICON_PATH = 'icons/icon-128.png';

function hostTimeout() {
  const error = new Error('The application did not answer in time.');
  error.code = 'host-timeout';
  return error;
}

// Sends one message over a native messaging port and resolves with the first
// reply. A port keeps the service worker alive while the host works, which a
// one-shot message does not promise for a slow application start.
export function exchangeWithHost(chrome, message, timeoutMs = REPLY_TIMEOUT_MS) {
  return new Promise((resolve, reject) => {
    let settled = false;
    let port = null;
    const finish = (callback, value) => {
      if (settled) return;
      settled = true;
      clearTimeout(timer);
      try {
        port?.disconnect();
      } catch {
        // The port is already closed.
      }
      callback(value);
    };
    const timer = setTimeout(() => finish(reject, hostTimeout()), timeoutMs);
    try {
      port = chrome.runtime.connectNative(HOST_NAME);
      port.onMessage.addListener((reply) => finish(resolve, reply));
      port.onDisconnect.addListener(() => {
        const text = chrome.runtime.lastError?.message || 'Native host has exited.';
        finish(reject, new Error(text));
      });
      port.postMessage(message);
    } catch (error) {
      finish(reject, error);
    }
  });
}

export async function checkConnection(chrome, timeoutMs = REPLY_TIMEOUT_MS) {
  try {
    return interpretHelloReply(await exchangeWithHost(chrome, buildHelloMessage(), timeoutMs));
  } catch (error) {
    return { ok: false, code: classifyHostError(error) };
  }
}

export function createCaptureService(chrome, options = {}) {
  const now = options.now ?? (() => new Date());
  const newId = options.newId ?? (() => crypto.randomUUID());
  const replyTimeoutMs = options.replyTimeoutMs ?? REPLY_TIMEOUT_MS;
  const inFlight = new Set();
  let cataloguePromise = options.catalogues ? Promise.resolve(options.catalogues) : null;
  let logChain = Promise.resolve();
  let presentationChain = Promise.resolve();

  function track(promise) {
    const tracked = Promise.resolve(promise).catch(() => undefined).finally(() => inFlight.delete(tracked));
    inFlight.add(tracked);
    return tracked;
  }

  function catalogues() {
    if (!cataloguePromise) {
      cataloguePromise = loadCatalogues(async (path) => {
        const response = await fetch(chrome.runtime.getURL(path));
        return response.json();
      }).catch((error) => {
        cataloguePromise = null;
        throw error;
      });
    }
    return cataloguePromise;
  }

  async function translator(settings) {
    let loaded = {};
    try {
      loaded = await catalogues();
    } catch {
      // Without catalogues every message shows its key; English is still read
      // from the manifest strings the browser resolved itself.
    }
    return createTranslator(loaded, settings.language, chrome.i18n?.getUILanguage?.());
  }

  async function readSettings() {
    const stored = await chrome.storage.local.get(SETTINGS_KEY);
    return normalizeSettings(stored?.[SETTINGS_KEY]);
  }

  // Log writes run one after another so two captures that finish together
  // both reach the list.
  function record(entry) {
    logChain = logChain.then(async () => {
      const stored = await chrome.storage.local.get(LOG_KEY);
      await chrome.storage.local.set({ [LOG_KEY]: appendToLog(stored?.[LOG_KEY], entry) });
    }).catch(() => undefined);
    return logChain;
  }

  async function notify(settings, titleKey, fileName, code) {
    if (!chrome.notifications?.create) return;
    const t = await translator(settings);
    try {
      await chrome.notifications.create(`bambu-capture-${newId()}`, {
        type: 'basic',
        iconUrl: chrome.runtime.getURL(ICON_PATH),
        title: t.text(titleKey),
        message: t.lines('notifyBody', [fileName, { key: reasonMessageKey(code) }]),
        priority: 0,
      });
    } catch {
      // Notifications can be blocked by the operating system; the log keeps
      // the outcome either way.
    }
  }

  async function handOver(message) {
    try {
      return interpretCaptureReply(await exchangeWithHost(chrome, message, replyTimeoutMs), message.captureId);
    } catch (error) {
      return { ok: false, code: classifyHostError(error) };
    }
  }

  async function captureDownload(item, release) {
    const settings = await readSettings();
    const decision = decideCapture(item, settings);
    if (!decision.capture) {
      release();
      return { outcome: 'ignored', code: decision.reason };
    }
    const at = now().toISOString();
    const entry = (outcome, code, extra = {}) => logEntry({
      at, fileName: decision.fileName, url: decision.url, referrer: item.referrer, origin: 'download', outcome, code, ...extra,
    });

    // Stop the browser's own transfer before anything leaves the browser.
    try {
      await chrome.downloads.pause(item.id);
    } catch {
      release();
      await record(entry('kept', 'pause-failed'));
      if (settings.notifyOnFallback) await notify(settings, 'notifyKeptTitle', sanitizeFileName(decision.fileName), 'pause-failed');
      return { outcome: 'kept', code: 'pause-failed' };
    }

    const message = buildCaptureMessage({
      origin: 'download', url: decision.url, referrer: item.referrer, fileName: decision.fileName,
      type: decision.type, mime: item.mime, totalBytes: item.totalBytes,
    }, { captureId: newId(), capturedAt: at });
    const result = await handOver(message);

    if (result.ok) {
      // The application owns the download now and asks before transferring.
      try {
        await chrome.downloads.cancel(item.id);
      } catch {
        // Already gone from the browser.
      }
      release();
      try {
        await chrome.downloads.erase({ id: item.id });
      } catch {
        // The browser's list may already have dropped it.
      }
      await record(entry('queued', 'queued', { queueItemId: result.queueItemId }));
      return { outcome: 'queued', code: 'queued', message, queueItemId: result.queueItemId };
    }

    try {
      await chrome.downloads.resume(item.id);
    } catch {
      // The person may have cancelled it in the browser meanwhile.
    }
    release();
    await record(entry('kept', result.code));
    if (settings.notifyOnFallback) await notify(settings, 'notifyKeptTitle', message.fileName, result.code);
    return { outcome: 'kept', code: result.code, message };
  }

  // downloads.onDeterminingFilename holds the browser's download until
  // suggest() runs, so the download cannot finish in the browser while the
  // application is being asked. suggest() runs exactly once on every path.
  function handleDownload(item, suggest) {
    let released = false;
    const release = () => {
      if (released) return;
      released = true;
      try {
        suggest();
      } catch {
        // The download no longer exists.
      }
    };
    return track(captureDownload(item, release).catch(() => ({ outcome: 'error' })).finally(release));
  }

  function onDeterminingFilename(item, suggest) {
    handleDownload(item, suggest);
    return true;
  }

  async function captureLink(info) {
    const settings = await readSettings();
    const decision = decideLinkCapture(info?.linkUrl);
    const at = now().toISOString();
    const referrer = info?.frameUrl || info?.pageUrl || '';
    const entry = (outcome, code, extra = {}) => logEntry({
      at, fileName: decision.fileName, url: decision.url, referrer, origin: 'link', outcome, code, ...extra,
    });
    if (!decision.capture) {
      await record(entry('not-sent', decision.reason));
      await notify(settings, 'notifyNotSentTitle', sanitizeFileName(decision.fileName) || decision.url, decision.reason);
      return { outcome: 'not-sent', code: decision.reason };
    }
    const message = buildCaptureMessage({
      origin: 'link', url: decision.url, referrer, fileName: decision.fileName, type: decision.type, mime: null, totalBytes: null,
    }, { captureId: newId(), capturedAt: at });
    const result = await handOver(message);
    if (result.ok) {
      await record(entry('queued', 'queued', { queueItemId: result.queueItemId }));
      return { outcome: 'queued', code: 'queued', message, queueItemId: result.queueItemId };
    }
    // An explicit request always explains a failure, whatever the setting.
    await record(entry('not-sent', result.code));
    await notify(settings, 'notifyNotSentTitle', message.fileName, result.code);
    return { outcome: 'not-sent', code: result.code, message };
  }

  function handleMenuClick(info, tab) {
    if (info?.menuItemId !== MENU_ID) return Promise.resolve({ outcome: 'ignored' });
    return track(captureLink(info, tab));
  }

  // Menu title, toolbar title and badge follow the language and the switch.
  function refreshPresentation() {
    presentationChain = presentationChain.then(async () => {
      const settings = await readSettings();
      const t = await translator(settings);
      try {
        await chrome.contextMenus.removeAll();
      } catch {
        // Nothing to remove.
      }
      await new Promise((resolve) => {
        chrome.contextMenus.create({
          id: MENU_ID,
          title: t.text('menuOpenLink'),
          contexts: ['link'],
          targetUrlPatterns: contextMenuPatterns(),
        }, () => {
          void chrome.runtime.lastError;
          resolve();
        });
      });
      await chrome.action.setTitle({ title: settings.enabled ? t.text('actionTitleOn') : t.text('actionTitleOff') });
      await chrome.action.setBadgeText({ text: settings.enabled ? '' : t.parts('badgeOff').primary });
    }).catch(() => undefined);
    return track(presentationChain);
  }

  function install() {
    chrome.downloads.onDeterminingFilename.addListener(onDeterminingFilename);
    chrome.contextMenus.onClicked.addListener(handleMenuClick);
    chrome.runtime.onInstalled.addListener(() => refreshPresentation());
    chrome.runtime.onStartup.addListener(() => refreshPresentation());
    chrome.storage.onChanged.addListener((changes, area) => {
      if (area === 'local' && changes?.[SETTINGS_KEY]) refreshPresentation();
    });
    chrome.action.onClicked.addListener(() => chrome.runtime.openOptionsPage());
  }

  // Resolves once every capture, log write and menu update started so far has
  // settled; the tests use it, and it costs nothing in the browser.
  async function idle() {
    while (inFlight.size) await Promise.all([...inFlight]);
    await logChain;
  }

  return { install, handleDownload, handleMenuClick, refreshPresentation, idle };
}
