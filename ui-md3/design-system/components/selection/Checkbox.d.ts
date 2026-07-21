/** Glyph checkbox (check_box / check_box_outline_blank) as used in the export list. */
export interface CheckboxProps {
  checked: boolean;
  label?: string;
  onChange?: (checked: boolean) => void;
  style?: React.CSSProperties;
}
