import type { ReactNode } from 'react';
import { splitStructuredTranslation } from '../../i18nResources';

interface Props {
  children: string;
  className?: string;
  primaryClassName?: string;
  secondaryClassName?: string;
  as?: 'span' | 'div';
}

/** Render bilingual translations as intrinsic primary and secondary lines. */
export function BilingualText({
  children,
  className = '',
  primaryClassName = '',
  secondaryClassName = '',
  as = 'span',
}: Props) {
  const { primary, secondary } = splitStructuredTranslation(children);
  const content: ReactNode = (
    <>
      <span className={`fm-label-primary ${primaryClassName}`}>{primary}</span>
      {secondary && (
        <span className={`fm-label-secondary ${secondaryClassName}`}>{secondary}</span>
      )}
    </>
  );

  return as === 'div'
    ? <div className={`fm-bilingual-label ${className}`}>{content}</div>
    : <span className={`fm-bilingual-label ${className}`}>{content}</span>;
}
