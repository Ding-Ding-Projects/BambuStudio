/** Ghost icon button — hover reveals a surface-container-high wash. */
export interface IconButtonProps {
  /** Material Symbols ligature name */
  icon: string;
  /** Outer square size in px (28–44 in the app; 36 default) */
  size?: number;
  /** Override glyph size (defaults 17/19/22 by button size) */
  iconSize?: number;
  title?: string;
  /** Close-window style: hover turns error/on-error */
  danger?: boolean;
  /** Resting surface-container-highest disc (Device temp edit) */
  filled?: boolean;
  /** 'circle' (default) or 'square' (title-bar window controls, 8px radius) */
  shape?: 'circle' | 'square';
  onClick?: () => void;
  style?: React.CSSProperties;
}
