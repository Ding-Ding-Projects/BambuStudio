/** Pill-shaped MD3 button. @startingPoint section="Actions" subtitle="Filled / tonal / outlined / text / danger pill buttons" viewport="480x64" */
export interface ButtonProps {
  /** 'filled' = primary CTA (Print plate) · 'tonal' = secondary-container (Pause) · 'outlined' = 1px outline (Slice plate, Export all) · 'text' = borderless primary (Cancel) · 'danger' = outlined error (Stop) */
  variant?: 'filled' | 'tonal' | 'outlined' | 'text' | 'danger';
  /** sm=36px · md=42px · lg=44px; radius is always height/2 */
  size?: 'sm' | 'md' | 'lg';
  /** Material Symbols ligature name, rendered 18–20px before the label */
  icon?: string;
  /** Set the FILL variation axis (emphasized CTAs like Print) */
  iconFill?: boolean;
  /** Adds --md-elev-2 (primary CTAs in bars) */
  elevated?: boolean;
  onClick?: () => void;
  title?: string;
  style?: React.CSSProperties;
  children?: React.ReactNode;
}
