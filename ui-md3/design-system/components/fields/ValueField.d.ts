/** Read-only numeric value chip — mono value + muted unit on sc-highest. */
export interface ValueFieldProps {
  value: string | number;
  /** Unit suffix (mm, %, °C) */
  unit?: string;
  minWidth?: number;
  style?: React.CSSProperties;
}
