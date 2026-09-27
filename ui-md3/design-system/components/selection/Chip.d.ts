/** Filter/choice chip — 30px pill, filled primary when selected. */
export interface ChipProps {
  label: string;
  /** Optional 16px leading Material Symbol (Preview options chips) */
  icon?: string;
  selected?: boolean;
  /** Selected state uses secondary-container instead of primary (vendor filters) */
  tonal?: boolean;
  /** Roboto Mono label (format chips: .bbsflmt, JSON) */
  mono?: boolean;
  size?: 'sm' | 'md';
  onClick?: () => void;
  style?: React.CSSProperties;
}
