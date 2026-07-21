/** 52px workspace tab bar: icon+label tabs, FILL'd icon + 3px bottom indicator when active. */
export interface TabBarProps {
  /** Defaults to the nine app workspaces (APP_TABS export) */
  tabs?: { id: string; label: string; icon: string }[];
  value: string;
  onChange?: (id: string) => void;
  style?: React.CSSProperties;
}
