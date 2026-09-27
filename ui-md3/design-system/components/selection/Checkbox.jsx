import React from 'react';
export function Checkbox({ checked, label, onChange, style }) {
  return (
    <button onClick={() => onChange && onChange(!checked)}
      style={{ display: 'inline-flex', alignItems: 'center', gap: 8, border: 'none', background: 'transparent', cursor: 'pointer',
        color: 'var(--md-on-surface)', fontSize: 12.5, fontWeight: 500, fontFamily: 'var(--md-font)', padding: '6px 8px', borderRadius: 8, ...style }}>
      <span data-icon style={{ fontSize: 20, color: checked ? 'var(--md-primary)' : 'var(--md-on-surface-variant)' }}>
        {checked ? 'check_box' : 'check_box_outline_blank'}
      </span>
      {label}
    </button>
  );
}
