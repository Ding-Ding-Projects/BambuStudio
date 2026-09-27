import React from 'react';
export function Chip({ label, icon, selected = false, tonal = false, mono = false, size = 'sm', onClick, style }) {
  const [hov, setHov] = React.useState(false);
  const h = size === 'md' ? 32 : 30;
  const sel = selected
    ? (tonal ? { background: 'var(--md-secondary-container)', color: 'var(--md-on-secondary-container)', border: '1px solid var(--md-primary)' }
             : { background: 'var(--md-primary)', color: 'var(--md-on-primary)', border: '1px solid var(--md-primary)' })
    : { background: hov ? 'var(--md-sc-high)' : 'transparent', color: 'var(--md-on-surface-variant)', border: '1px solid var(--md-outline)' };
  return (
    <button onClick={onClick} onMouseEnter={() => setHov(true)} onMouseLeave={() => setHov(false)}
      style={{ display: 'inline-flex', alignItems: 'center', gap: 6, height: h, padding: '0 ' + (size === 'md' ? 12 : 13) + 'px',
        borderRadius: h / 2, cursor: 'pointer', fontSize: 12, fontWeight: 500, transition: '.15s',
        fontFamily: mono ? 'var(--md-font-mono)' : 'var(--md-font)', ...sel, ...style }}>
      {icon && <span data-icon style={{ fontSize: 16 }}>{icon}</span>}
      {label}
    </button>
  );
}
