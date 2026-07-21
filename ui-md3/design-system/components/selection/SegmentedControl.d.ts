/** Segmented single-select on a surface-container-highest track. */
export interface SegmentedControlProps {
  options: { id: string; label: string; icon?: string }[];
  value: string;
  onChange?: (id: string) => void;
  /** sm = 30px items / 12px track radius (process tabs, speed modes) · md = 36px / 14px (theme & density toggles) */
  size?: 'sm' | 'md';
  /** Items share width equally (default true) */
  grow?: boolean;
  style?: React.CSSProperties;
}
