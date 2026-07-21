import React from 'react';
export function IconButton({ icon, size = 36, iconSize, title, danger = false, filled = false, shape = 'circle', onClick, style }) {
  const [hov, setHov] = React.useState(false);
  const isz = iconSize || (size >= 40 ? 22 : (size >= 34 ? 19 : 17));
  const hoverBg = danger ? 'var(--md-error)' : 'var(--md-sc-high)';
  return (
    <button title={title} onClick={onClick} onMouseEnter={() => setHov(true)} onMouseLeave={() => setHov(false)}
      style={{ width: size + (shape === 'square' ? 4 : 0), height: size, border: 'none', cursor: 'pointer',
        borderRadius: shape === 'circle' ? '50%' : 8,
        background: hov ? hoverBg : (filled ? 'var(--md-sc-highest)' : 'transparent'),
        color: hov && danger ? 'var(--md-on-error)' : 'var(--md-on-surface-variant)',
        display: 'inline-flex', alignItems: 'center', justifyContent: 'center', transition: '.15s', ...style }}>
      <span data-icon style={{ fontSize: isz }}>{icon}</span>
    </button>
  );
}
