// Centralized locales path — update here + vite.config.ts + tsconfig.app.json when moving
const LOCALES_DIR = 'locales';

export default {
  input: ['src/**/*.{js,jsx,ts,tsx}'],
  output: `${LOCALES_DIR}/$LOCALE.json`,
  locales: [
    'en', 'zh_CN', 'ja_JP', 'it_IT', 'fr_FR', 'de_DE',
    'hu_HU', 'es_ES', 'sv_SE', 'cs_CZ', 'nl_NL', 'uk_UA',
    'ru_RU', 'tr_TR', 'pt_BR', 'ko_KR', 'pl_PL', 'yue_HK',
    'ro_RO', 'th_TH', 'el_GR', 'id_ID', 'vi_VN', 'zh_TW',
  ],
  defaultNamespace: 'translation',
  // Disable key/namespace separators so English sentences with dots/colons
  // are treated as flat keys, not nested paths
  keySeparator: false,
  namespaceSeparator: false,
  // `useKeysAsDefaultValue` is not an i18next-parser@9 option (checked against
  // the installed package: it appears nowhere in its lexers, parser or
  // transform). It was inert here and never seeded anything; `defaultValue`
  // below is the option that actually controls what a freshly extracted key
  // gets written as, per locale.
  //
  // English (and every other already-translated locale) still gets the key
  // itself, since the key text IS the English source string. yue_HK gets an
  // empty string instead: a brand new msgid must be translated by a person
  // or an agent before it ships, and an empty value fails
  // tests/i18nResources.test.ts loudly (an untranslated-but-not-empty copy of
  // the English text would have passed that test silently, which defeats it).
  defaultValue: (locale, _namespace, key) => (locale === 'yue_HK' ? '' : key),
  // Keep existing translations, only add new keys
  createOldCatalogs: false,
  sort: true,
};
