/** Inverse-surface toast; stack bottom-center, max width 560px. */
export interface SnackbarProps {
  /** Leading glyph, tinted inverse-primary (check_circle, history) */
  icon?: string;
  message: string;
  /** Optional bold inverse-primary action (Undo, View) */
  actionLabel?: string;
  onAction?: () => void;
  onDismiss?: () => void;
  style?: React.CSSProperties;
}
