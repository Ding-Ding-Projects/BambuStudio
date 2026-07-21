import React from 'react';
export function Card({ interactive = false, pad = true, radius, onClick, style, children }) {
  const [hov, setHov] = React.useState(false);
  return (
    <div onClick={onClick} onMouseEnter={() => setHov(true)} onMouseLeave={() => setHov(false)}
      style={{ background: 'var(--md-sc-low)', borderRadius: radius || 'var(--radius)', cursor: interactive ? 'pointer' : 'default',
        border: '1px solid ' + (interactive && hov ? 'var(--md-primary)' : 'var(--md-outline-variant)'),
        padding: pad ? 'var(--pad)' : 0, transition: 'border-color .15s', ...style }}>
      {children}
    </div>
  );
}
