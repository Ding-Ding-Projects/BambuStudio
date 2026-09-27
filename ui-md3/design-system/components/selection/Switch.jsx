import React from 'react';
export function Switch({ checked, onChange, style }) {
  return (
    <button onClick={() => onChange && onChange(!checked)}
      style={{ width: 44, height: 24, borderRadius: 14, position: 'relative', cursor: 'pointer', padding: 0, flex: '0 0 auto', transition: '.15s',
        border: '2px solid ' + (checked ? 'var(--md-primary)' : 'var(--md-outline)'),
        background: checked ? 'var(--md-primary)' : 'transparent', ...style }}>
      <span style={{ position: 'absolute', top: '50%', transform: 'translateY(-50%)', transition: '.15s', borderRadius: '50%',
        left: checked ? 22 : 4, width: checked ? 16 : 12, height: checked ? 16 : 12,
        background: checked ? 'var(--md-on-primary)' : 'var(--md-outline)' }} />
    </button>
  );
}
