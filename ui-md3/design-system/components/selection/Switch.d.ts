/** MD3 switch — 44×24 track, 2px border, knob grows 12→16px when on. */
export interface SwitchProps {
  checked: boolean;
  onChange?: (checked: boolean) => void;
  style?: React.CSSProperties;
}
