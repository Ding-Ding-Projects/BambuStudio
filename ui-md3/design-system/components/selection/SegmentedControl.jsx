import React from 'react';
export function SegmentedControl({ options, value, onChange, size = 'sm', grow = true, style }) {
  const pad = size === 'md' ? 4 : 3, r = size === 'md' ? 14 : 12, ir = size === 'md' ? 11 : 9, h = size === 'md' ? 36 : 30;
  return (
    <div style={{ display: 'flex', gap: size === 'md' ? 6 : 4, padding: pad, background: 'var(--md-sc-highest)', borderRadius: r, ...style }}>
      {options.map(o => {
        const on = o.id === value;
        return (
          <button key={o.id} onClick={() => onChange && onChange(o.id)}
            style={{ flex: grow ? 1 : 'none', height: h, padding: '0 ' + (size === 'md' ? 18 : 10) + 'px', border: 'none', borderRadius: ir,
              cursor: 'pointer', fontSize: size === 'md' ? 12.5 : 11.5, fontWeight: 500, display: 'flex', alignItems: 'center', justifyContent: 'center', gap: 6,
              background: on ? 'var(--md-primary)' : 'transparent', color: on ? 'var(--md-on-primary)' : 'var(--md-on-surface-variant)', transition: '.15s' }}>
            {o.icon && <span data-icon style={{ fontSize: 18 }}>{o.icon}</span>}
            {o.label}
          </button>
        );
      })}
    </div>
  );
}
