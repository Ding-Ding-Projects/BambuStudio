/** Modal dialog: scrim, 28px panel on --md-sc, 44px primary-container icon tile header. @startingPoint section="Containment" subtitle="MD3 dialog shell with icon-tile header" viewport="560x420" */
export interface DialogProps {
  /** Header tile Material Symbol (file_download, print, palette) */
  icon: string;
  title: string;
  subtitle?: string;
  /** 460 (send) · 520 (add filament) · 580 (export) */
  width?: number;
  onClose?: () => void;
  /** Action row: text Cancel + filled confirm */
  footer?: React.ReactNode;
  /** Edge-to-edge body with bordered footer (list dialogs) */
  flush?: boolean;
  style?: React.CSSProperties;
  children?: React.ReactNode;
}
