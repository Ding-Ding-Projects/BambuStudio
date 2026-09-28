export type TranslationTable = Record<string, string>;

const CANTONESE_LABEL = '粵語：';
export const BILINGUAL_SEPARATOR = '\n';

export interface StructuredTranslation {
  primary: string;
  secondary?: string;
}

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
 *
 * The Cantonese line is only appended when it actually differs from the
 * English source (HTTP, RFID, and most product names translate to
 * themselves). Without this check every one of those entries would render a
 * redundant "RFID\n粵語：RFID" pair.
 */
export function buildEnglishCantoneseTranslation(
  english: TranslationTable,
  cantonese: TranslationTable,
): TranslationTable {
  return Object.fromEntries(
    Object.entries(english).map(([key, englishText]) => {
      const cantoneseText = cantonese[key] || englishText;
      if (cantoneseText === englishText) return [key, englishText];
      return [key, `${englishText}${BILINGUAL_SEPARATOR}${CANTONESE_LABEL}${cantoneseText}`];
    }),
  );
}

/** Split bilingual resources into visual lines without hiding either line from AT. */
export function splitStructuredTranslation(value: string): StructuredTranslation {
  const separatorIndex = value.indexOf(BILINGUAL_SEPARATOR);
  if (separatorIndex < 0) return { primary: value };
  return {
    primary: value.slice(0, separatorIndex),
    secondary: value.slice(separatorIndex + BILINGUAL_SEPARATOR.length),
  };
}

/**
 * Compact "English / 粵語" form for surfaces that cannot host a second JSX
 * element (an `<option>` cannot contain a `<span>`, and a native `<title>`
 * tooltip has no markup at all). Falls back to the plain string when the
 * value carries no Cantonese secondary line. The stacked "粵語：" label that
 * `BilingualText` shows on its own line would just be noise squeezed onto
 * one line here, so it is dropped in favour of the "/" separator alone.
 */
export function toInlineBilingual(value: string): string {
  const { primary, secondary } = splitStructuredTranslation(value);
  if (!secondary) return primary;
  const cantoneseOnly = secondary.startsWith(CANTONESE_LABEL)
    ? secondary.slice(CANTONESE_LABEL.length)
    : secondary;
  return `${primary} / ${cantoneseOnly}`;
}
