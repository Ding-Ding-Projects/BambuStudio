import React from 'react';
export function ValueField({ value, unit, minWidth = 96, style }) {
  return (
    <div style={{ display: 'flex', alignItems: 'center', gap: 6, height: 34, padding: '0 4px 0 12px', background: 'var(--md-sc-highest)',
      borderRadius: 10, minWidth, justifyContent: 'flex-end', ...style }}>
      <span style={{ fontFamily: 'var(--md-font-mono)', fontSize: 12.5, fontWeight: 500, color: 'var(--md-on-surface)' }}>{value}</span>
      {unit && <span style={{ fontSize: 11, color: 'var(--md-on-surface-variant)', paddingRight: 8 }}>{unit}</span>}
    </div>
  );
}
