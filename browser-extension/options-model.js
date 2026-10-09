// What the settings page shows, computed from settings, the recent-handoffs
// list and a translator. Kept apart from the page script so it can be tested
// without a browser.

import { MODEL_TYPES, reasonMessageKey } from './capture-rules.js';
import { LANGUAGE_CHOICES } from './i18n.js';

export function typeMessageKey(id) {
  return `type${id.charAt(0).toUpperCase()}${id.slice(1)}`;
}

export function typeRows(settings) {
  return MODEL_TYPES.map((type) => ({
    id: type.id,
    inputId: `type-${type.id}`,
    labelKey: typeMessageKey(type.id),
    checked: Boolean(settings.types[type.id]),
  }));
}

// The language list names every language in its own language; only the
// "same as the browser" choice is translated.
export function languageOptions(t) {
  return LANGUAGE_CHOICES.map((choice) => ({
    value: choice.id,
    label: choice.label ?? t.text(choice.labelKey),
    lang: choice.lang ?? null,
  }));
}

const OUTCOME_KEYS = Object.freeze({ queued: 'outcomeQueued', kept: 'outcomeKept', 'not-sent': 'outcomeNotSent' });

export function dateLocale(language) {
  return language === 'yue_HK' ? 'zh-HK' : 'en-GB';
}

function formatTime(at, language) {
  const date = new Date(at);
  if (Number.isNaN(date.getTime())) return '';
  return new Intl.DateTimeFormat(dateLocale(language), { dateStyle: 'medium', timeStyle: 'short' }).format(date);
}

// One row per log entry, newest first as stored. The outcome names what
// happened and, unless the download was handed over, why.
export function logRows(log, t) {
  if (!Array.isArray(log)) return [];
  return log.filter((entry) => entry && typeof entry === 'object').map((entry) => {
    const outcome = t.parts(OUTCOME_KEYS[entry.outcome] ?? 'outcomeNotSent');
    const detail = entry.outcome === 'queued' ? t.parts('reasonQueued') : t.parts(reasonMessageKey(entry.code));
    return {
      time: formatTime(entry.at, t.language),
      dateTime: typeof entry.at === 'string' ? entry.at : '',
      file: String(entry.fileName ?? ''),
      site: String(entry.site ?? ''),
      fromLink: entry.origin === 'link',
      outcome,
      detail,
    };
  });
}

export function connectionMessage(result) {
  if (!result) return { key: 'connectionUnknown' };
  if (result.checking) return { key: 'connectionChecking' };
  if (result.ok) return { key: 'connectionOk', substitutions: [result.app, result.version] };
  return { key: 'connectionFailed', substitutions: [{ key: reasonMessageKey(result.code) }] };
}
