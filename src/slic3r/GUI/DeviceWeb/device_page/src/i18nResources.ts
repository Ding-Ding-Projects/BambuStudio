export type TranslationTable = Record<string, string>;

const CANTONESE_LABEL = '粵語：';

export const languageFallbacks: Record<string, string[]> = {
  yue_HK: ['en'],
  bilingual_en_yue_HK: ['en'],
  default: ['en'],
};

/**
 * Build compact bilingual copy as real text, rather than CSS-only content, so
 * assistive technology receives the Cantonese secondary line too. Repeating
 * interpolation tokens in both lines is intentional: i18next substitutes every
 * occurrence with the same value.
 */
export function buildEnglishCantoneseTranslation(
  english: TranslationTable,
  cantonese: TranslationTable,
): TranslationTable {
  return Object.fromEntries(
    Object.entries(english).map(([key, englishText]) => {
      const cantoneseText = cantonese[key] || englishText;
      return [key, `${englishText}\n${CANTONESE_LABEL}${cantoneseText}`];
    }),
  );
}
