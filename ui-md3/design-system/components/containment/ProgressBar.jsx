import React from 'react';
export function ProgressBar({ value, height = 8, style }) {
  return (
    <div style={{ height, background: 'var(--md-sc-highest)', borderRadius: 6, overflow: 'hidden', ...style }}>
      <div style={{ width: Math.max(0, Math.min(100, value)) + '%', height: '100%', background: 'var(--md-primary)', borderRadius: 6, transition: 'width .3s' }} />
    </div>
  );
}
