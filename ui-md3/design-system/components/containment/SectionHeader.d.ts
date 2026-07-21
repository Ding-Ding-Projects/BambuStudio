/** 11px/600 uppercase section label with 16px leading icon — the app's strongest signature. */
export interface SectionHeaderProps {
  /** Material Symbols ligature (print, palette, tune, thermostat…) */
  icon?: string;
  /** Right-aligned slot (Sync AMS button, humidity status) */
  trailing?: React.ReactNode;
  style?: React.CSSProperties;
  children?: React.ReactNode;
}
