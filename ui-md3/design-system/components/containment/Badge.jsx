import React from 'react';
export function Badge({ style, children }) {
  return (
    <span style={{ fontSize: 11, fontWeight: 600, padding: '2px 8px', borderRadius: 7,
      background: 'var(--md-secondary-container)', color: 'var(--md-on-secondary-container)', ...style }}>
      {children}
    </span>
  );
}
