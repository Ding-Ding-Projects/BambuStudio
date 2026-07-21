/** Linear progress — primary fill on sc-highest track. */
export interface ProgressBarProps {
  /** 0–100 */
  value: number;
  /** 8px on Device print card, 6px on multi-device cards */
  height?: number;
  style?: React.CSSProperties;
}
