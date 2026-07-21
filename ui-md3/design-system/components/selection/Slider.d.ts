/** Native range slider tinted with accent-color: var(--md-primary). */
export interface SliderProps {
  min?: number;
  max?: number;
  value: number;
  onChange?: (value: number) => void;
  /** Vertical layer slider (Preview right edge, 300px tall) */
  vertical?: boolean;
  /** Fixed track length in px (defaults: flex:1 horizontal, 300 vertical) */
  length?: number;
  style?: React.CSSProperties;
}
