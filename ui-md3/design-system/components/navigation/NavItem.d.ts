/** Settings-nav pill row — 44px, secondary-container when selected. */
export interface NavItemProps {
  icon: string;
  label: string;
  selected?: boolean;
  onClick?: () => void;
  style?: React.CSSProperties;
}
