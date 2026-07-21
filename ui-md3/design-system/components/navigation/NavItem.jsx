import React from 'react';
export function NavItem({ icon, label, selected = false, onClick, style }) {
  const [hov, setHov] = React.useState(false);
  return (
    <div onClick={onClick} onMouseEnter={() => setHov(true)} onMouseLeave={() => setHov(false)}
      style={{ display: 'flex', alignItems: 'center', gap: 10, height: 44, padding: '0 14px', borderRadius: 22, cursor: 'pointer',
        background: selected ? 'var(--md-secondary-container)' : (hov ? 'var(--md-sc-high)' : 'transparent'),
        color: selected ? 'var(--md-on-secondary-container)' : 'var(--md-on-surface-variant)',
        fontSize: 13.5, fontWeight: selected ? 600 : 400, fontFamily: 'var(--md-font)', ...style }}>
      <span data-icon style={{ fontSize: 20 }}>{icon}</span> {label}
    </div>
  );
}
