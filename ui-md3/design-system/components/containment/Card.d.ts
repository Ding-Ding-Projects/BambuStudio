/** Content card: sc-low fill, 1px outline-variant border, --radius corners. */
export interface CardProps {
  /** Hover promotes the border to primary (the signature card hover) */
  interactive?: boolean;
  /** var(--pad) padding (default). false for media cards that bleed to edges */
  pad?: boolean;
  /** Override corner radius (home cards use 20px, hero 28px) */
  radius?: number | string;
  onClick?: () => void;
  style?: React.CSSProperties;
  children?: React.ReactNode;
}
