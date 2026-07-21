import React from 'react';
export function Slider({ min = 0, max = 100, value, onChange, vertical = false, length, style }) {
  return (
    <input type="range" min={min} max={max} value={value}
      onChange={e => onChange && onChange(Number(e.target.value))}
      style={vertical
        ? { writingMode: 'vertical-lr', direction: 'rtl', width: 12, height: length || 300, accentColor: 'var(--md-primary)', cursor: 'pointer', ...style }
        : { flex: length ? 'none' : 1, width: length, accentColor: 'var(--md-primary)', cursor: 'pointer', ...style }} />
  );
}
