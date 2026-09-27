/** Vertical tool rail (Prepare left edge, width --rail): 44px 12px-radius icon buttons, selected = filled primary. */
export interface GizmoRailProps {
  /** {id, icon, label} tools; {divider:true} inserts a 28px hairline */
  tools: ({ id: string; icon: string; label: string } | { divider: true })[];
  value?: string;
  onChange?: (id: string) => void;
  style?: React.CSSProperties;
}
