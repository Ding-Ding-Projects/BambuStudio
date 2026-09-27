import React from 'react';
export function SectionHeader({ icon, trailing, style, children }) {
  return (
    <div style={{ display: 'flex', alignItems: 'center', gap: 6, ...style }}>
      <div style={{ flex: 1, display: 'flex', alignItems: 'center', gap: 6, fontSize: 11, fontWeight: 600, letterSpacing: '.6px',
        color: 'var(--md-on-surface-variant)', textTransform: 'uppercase' }}>
        {icon && <span data-icon style={{ fontSize: 16 }}>{icon}</span>}
        {children}
      </div>
      {trailing}
    </div>
  );
}
