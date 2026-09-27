/** Dropdown trigger — outlined (sidebar Bed type / Process preset) or filled (inline param selects). */
export interface SelectFieldProps {
  /** Current selection text */
  value: string;
  /** true = 34px sc-highest chip style; false = 38px outlined full-width */
  filled?: boolean;
  /** 10.5px caption above the field (“Bed type”) */
  caption?: string;
  onClick?: () => void;
  style?: React.CSSProperties;
}
