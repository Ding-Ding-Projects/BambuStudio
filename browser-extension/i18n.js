// Language handling for the extension's own surfaces (settings page, context
// menu, toolbar title and notifications).
//
// The browser picks a _locales catalogue from its own interface language, and
// no Chromium browser offers Hong Kong Cantonese as an interface language, so
// the extension reads both catalogues itself and follows the person's choice:
// English, Hong Kong Cantonese, both (English first), or the browser's choice.

export const CATALOGUE_LANGUAGES = Object.freeze(['en', 'yue_HK']);

// The language list names each choice in its own language, as the application
// does, so a person can find their language whatever is showing now.
export const LANGUAGE_CHOICES = Object.freeze([
  Object.freeze({ id: 'auto', label: null, labelKey: 'languageAuto' }),
  Object.freeze({ id: 'en', label: 'English', lang: 'en' }),
  Object.freeze({ id: 'yue_HK', label: '廣東話（香港）', lang: 'yue-HK' }),
  Object.freeze({ id: 'bilingual_en_yue_HK', label: 'English + 廣東話', lang: 'en' }),
]);

const HTML_LANG = Object.freeze({ en: 'en', yue_HK: 'yue-HK', bilingual_en_yue_HK: 'en' });

export function normalizeLanguageMode(value) {
  return LANGUAGE_CHOICES.some((choice) => choice.id === value) ? value : 'auto';
}

// "auto" follows the browser: a Cantonese or Hong Kong Chinese interface gets
// Cantonese, everything else English.
export function effectiveLanguage(mode, uiLanguage) {
  const chosen = normalizeLanguageMode(mode);
  if (chosen !== 'auto') return chosen;
  const ui = String(uiLanguage ?? '').replace(/_/g, '-').toLowerCase();
  if (ui === 'yue' || ui.startsWith('yue-') || ui === 'zh-hk' || ui.startsWith('zh-hant-hk')) return 'yue_HK';
  return 'en';
}

// Chrome's own substitution rules for catalogue messages: $1 to $9 take the
// substitutions in order and $$ is a literal dollar sign.
export function formatMessage(template, substitutions = []) {
  const values = Array.isArray(substitutions) ? substitutions : [substitutions];
  return String(template ?? '').replace(/\$(\$|[1-9])/g, (match, which) => {
    if (which === '$') return '$';
    const value = values[Number(which) - 1];
    return value === undefined || value === null ? '' : String(value);
  });
}

export async function loadCatalogues(readJson) {
  const entries = await Promise.all(CATALOGUE_LANGUAGES.map(async (language) => {
    const raw = await readJson(`_locales/${language}/messages.json`);
    const messages = {};
    for (const [key, value] of Object.entries(raw ?? {})) {
      if (value && typeof value.message === 'string') messages[key] = value.message;
    }
    return [language, messages];
  }));
  return Object.fromEntries(entries);
}

export function createTranslator(catalogues, mode, uiLanguage) {
  const language = effectiveLanguage(mode, uiLanguage);
  const english = catalogues?.en ?? {};
  const cantonese = catalogues?.yue_HK ?? {};

  // A substitution may itself be a message, written { key, substitutions }; it
  // is translated into the same language as the message around it, so a
  // bilingual line never mixes an English reason into the Cantonese sentence.
  function resolved(substitutions, translate) {
    const values = Array.isArray(substitutions) ? substitutions : [substitutions];
    return values.map((value) => (value && typeof value === 'object' && typeof value.key === 'string'
      ? translate(value.key, value.substitutions)
      : value));
  }

  function englishText(key, substitutions) {
    return formatMessage(english[key] ?? key, resolved(substitutions, englishText));
  }

  function cantoneseText(key, substitutions) {
    const template = cantonese[key];
    if (template === undefined) return null;
    return formatMessage(template, resolved(substitutions, (inner, innerSubstitutions) =>
      cantoneseText(inner, innerSubstitutions) ?? englishText(inner, innerSubstitutions)));
  }

  // The parts of one message: the primary text in the active language and, in
  // bilingual mode, the Cantonese text as a second line with its own language.
  function parts(key, substitutions) {
    const primaryEnglish = englishText(key, substitutions);
    if (language === 'yue_HK') {
      const text = cantoneseText(key, substitutions);
      return text === null
        ? { primary: primaryEnglish, primaryLang: 'en', secondary: null, secondaryLang: null }
        : { primary: text, primaryLang: 'yue-HK', secondary: null, secondaryLang: null };
    }
    if (language === 'bilingual_en_yue_HK') {
      const text = cantoneseText(key, substitutions);
      return {
        primary: primaryEnglish,
        primaryLang: 'en',
        secondary: text !== null && text !== primaryEnglish ? text : null,
        secondaryLang: 'yue-HK',
      };
    }
    return { primary: primaryEnglish, primaryLang: 'en', secondary: null, secondaryLang: null };
  }

  // One-line text for places that hold a single string (menu titles, button
  // tooltips): bilingual mode joins the two languages with " / ".
  function text(key, substitutions) {
    const { primary, secondary } = parts(key, substitutions);
    return secondary ? `${primary} / ${secondary}` : primary;
  }

  // Multi-line text (notification bodies): bilingual mode puts Cantonese on its
  // own line.
  function lines(key, substitutions) {
    const { primary, secondary } = parts(key, substitutions);
    return secondary ? `${primary}\n${secondary}` : primary;
  }

  return { language, htmlLang: HTML_LANG[language], parts, text, lines };
}
