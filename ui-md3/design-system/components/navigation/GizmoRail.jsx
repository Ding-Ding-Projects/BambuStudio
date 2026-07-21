import React from 'react';
export function GizmoRail({ tools, value, onChange, style }) {
  const [hov, setHov] = React.useState(null);
  return (
    <div style={{ width: 'var(--rail)', flex: '0 0 auto', background: 'var(--md-sc-low)', borderRight: '1px solid var(--md-outline-variant)',
      display: 'flex', flexDirection: 'column', alignItems: 'center', padding: '8px 0', gap: 3, overflowY: 'auto', ...style }}>
      {tools.map((g, i) => g.divider
        ? <div key={'d' + i} style={{ width: 28, height: 1, background: 'var(--md-outline-variant)', margin: '4px 0', flex: '0 0 auto' }} />
        : (
          <button key={g.id} title={g.label} onClick={() => onChange && onChange(g.id)}
            onMouseEnter={() => setHov(g.id)} onMouseLeave={() => setHov(null)}
            style={{ width: 44, height: 44, flex: '0 0 auto', border: 'none', borderRadius: 12, cursor: 'pointer',
              display: 'flex', flexDirection: 'column', alignItems: 'center', justifyContent: 'center', gap: 1,
              background: g.id === value ? 'var(--md-primary)' : 'transparent',
              color: g.id === value ? 'var(--md-on-primary)' : 'var(--md-on-surface-variant)',
              filter: hov === g.id ? 'brightness(1.08)' : 'none' }}>
            <span data-icon style={{ fontSize: 21, fontVariationSettings: g.id === value ? "'FILL' 1" : "'FILL' 0" }}>{g.icon}</span>
          </button>
        ))}
    </div>
  );
}
