// src/i18n.ts
import i18n from 'i18next';
import { initReactI18next } from 'react-i18next';

// Import all locale resources statically (bundled into JS, no runtime fetch needed)
import en from '@locales/en.json';
import zh_CN from '@locales/zh_CN.json';
import ja_JP from '@locales/ja_JP.json';
import it_IT from '@locales/it_IT.json';
import fr_FR from '@locales/fr_FR.json';
import de_DE from '@locales/de_DE.json';
import hu_HU from '@locales/hu_HU.json';
import es_ES from '@locales/es_ES.json';
import sv_SE from '@locales/sv_SE.json';
import cs_CZ from '@locales/cs_CZ.json';
import nl_NL from '@locales/nl_NL.json';
import uk_UA from '@locales/uk_UA.json';
import ru_RU from '@locales/ru_RU.json';
import tr_TR from '@locales/tr_TR.json';
import pt_BR from '@locales/pt_BR.json';
import ko_KR from '@locales/ko_KR.json';
import pl_PL from '@locales/pl_PL.json';
import yue_HK from '@locales/yue_HK.json';
import ro_RO from '@locales/ro_RO.json';
import th_TH from '@locales/th_TH.json';
import el_GR from '@locales/el_GR.json';
import id_ID from '@locales/id_ID.json';
import vi_VN from '@locales/vi_VN.json';
import zh_TW from '@locales/zh_TW.json';
import { buildEnglishCantoneseTranslation, languageFallbacks } from './i18nResources.ts';

const bilingual_en_yue_HK = buildEnglishCantoneseTranslation(en, yue_HK);

// Detect language: URL ?lang= param > localStorage > fallback 'en'
// Consistent with other webview pages (text.js TranslatePage pattern)
function detectLanguage(): string {
  const params = new URLSearchParams(window.location.search);
  const urlLang = params.get('lang');
  if (urlLang) {
    localStorage.setItem('BambuWebLang', urlLang);
    return urlLang;
  }
  return localStorage.getItem('BambuWebLang') || 'en';
}

// <html lang> follows the active mode: 'en' for English and for bilingual
// (the primary, first-read line is always English), 'yue-HK' for Cantonese.
// Any other real locale (zh_CN, de_DE, ...) just swaps '_' for '-' to match
// the BCP-47 form the `lang` attribute expects.
function htmlLangFor(language: string): string {
  if (language === 'yue_HK') return 'yue-HK';
  if (language === 'bilingual_en_yue_HK') return 'en';
  return language.replace(/_/g, '-');
}

function applyHtmlLang(language: string): void {
  if (typeof document !== 'undefined' && document.documentElement) {
    document.documentElement.lang = htmlLangFor(language);
  }
}

i18n
  .use(initReactI18next)
  .init({
    lng: detectLanguage(),
    fallbackLng: languageFallbacks,
    // Empty translation values must fall back to English instead of rendering
    // as blank UI labels (STUDIO-18236).
    returnEmptyString: false,
    debug: false,
    interpolation: {
      escapeValue: false,
    },
    resources: {
      en: { translation: en },
      zh_CN: { translation: zh_CN },
      ja_JP: { translation: ja_JP },
      it_IT: { translation: it_IT },
      fr_FR: { translation: fr_FR },
      de_DE: { translation: de_DE },
      hu_HU: { translation: hu_HU },
      es_ES: { translation: es_ES },
      sv_SE: { translation: sv_SE },
      cs_CZ: { translation: cs_CZ },
      nl_NL: { translation: nl_NL },
      uk_UA: { translation: uk_UA },
      ru_RU: { translation: ru_RU },
      tr_TR: { translation: tr_TR },
      pt_BR: { translation: pt_BR },
      ko_KR: { translation: ko_KR },
      pl_PL: { translation: pl_PL },
      yue_HK: { translation: yue_HK },
      bilingual_en_yue_HK: { translation: bilingual_en_yue_HK },
      ro_RO: { translation: ro_RO },
      th_TH: { translation: th_TH },
      el_GR: { translation: el_GR },
      id_ID: { translation: id_ID },
      vi_VN: { translation: vi_VN },
      zh_TW: { translation: zh_TW },
    },
    // When a key has no translation, return the key itself (English original text)
    parseMissingKeyHandler: (key) => key,
  });

// Keep <html lang> in step with the active mode: once at boot, then again on
// every runtime switch (Settings, or a fresh ?lang= navigation).
applyHtmlLang(i18n.language);
i18n.on('languageChanged', applyHtmlLang);

export default i18n;
