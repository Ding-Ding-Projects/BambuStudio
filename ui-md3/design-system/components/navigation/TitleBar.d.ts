/** 46px window title bar: logo tile, menus, git-history chip, project chip, palette, window controls. @startingPoint section="Navigation" subtitle="App window title bar" viewport="900x66" */
export interface TitleBarProps {
  appName?: string;
  menus?: string[];
  /** Git branch shown mono in the history chip */
  branch?: string;
  /** Short commit hash after the pulse dot */
  head?: string;
  projectName?: string;
  onMenu?: (menu: string) => void;
  onHistory?: () => void;
  onPalette?: () => void;
  onMinimize?: () => void;
  onMaximize?: () => void;
  onClose?: () => void;
  style?: React.CSSProperties;
}
