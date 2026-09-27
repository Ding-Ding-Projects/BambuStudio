import React from 'react';
export function SelectField({ value, filled = false, caption, onClick, style }) {
  const inner = filled
    ? { height: 34, padding: '0 8px 0 12px', background: 'var(--md-sc-highest)', border: 'none', borderRadius: 10, gap: 4 }
    : { height: 38, padding: '0 12px', background: 'transparent', border: '1px solid var(--md-outline)', borderRadius: 10, justifyContent: 'space-between', width: '100%' };
  return (
    <div style={{ display: 'flex', flexDirection: 'column', gap: 3, ...style }}>
      {caption && <div style={{ fontSize: 10.5, color: 'var(--md-on-surface-variant)', paddingLeft: 4 }}>{caption}</div>}
      <button onClick={onClick} style={{ display: 'flex', alignItems: 'center', cursor: 'pointer',
        color: 'var(--md-on-surface)', fontSize: 12.5, fontFamily: 'var(--md-font)', ...inner }}>
        <span style={{ fontWeight: filled ? 400 : 500, overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>{value}</span>
        <span data-icon style={{ fontSize: filled ? 16 : 18, color: 'var(--md-on-surface-variant)' }}>expand_more</span>
      </button>
    </div>
  );
}
