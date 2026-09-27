import React from 'react';
export function Button({ variant = 'filled', size = 'md', icon, iconFill = false, elevated = false, onClick, title, style, children }) {
  const [hov, setHov] = React.useState(false);
  const h = { sm: 36, md: 42, lg: 44 }[size] || 42;
  const pad = { sm: 16, md: 18, lg: 22 }[size] || 18;
  const iconSz = size === 'sm' ? 18 : 20;
  const v = {
    filled:   { background: 'var(--md-primary)', color: 'var(--md-on-primary)', border: 'none', fontWeight: 600 },
    tonal:    { background: 'var(--md-secondary-container)', color: 'var(--md-on-secondary-container)', border: 'none', fontWeight: 600 },
    outlined: { background: 'transparent', color: 'var(--md-on-surface)', border: '1px solid var(--md-outline)', fontWeight: 500 },
    text:     { background: 'transparent', color: 'var(--md-primary)', border: 'none', fontWeight: 600 },
    danger:   { background: 'transparent', color: 'var(--md-error)', border: '1px solid var(--md-error)', fontWeight: 600 },
  }[variant];
  const hover = hov ? (variant === 'filled' ? { filter: 'brightness(1.06)' }
    : variant === 'tonal' ? { filter: 'brightness(1.04)' }
    : { background: variant === 'text' ? 'var(--md-secondary-container)' : 'var(--md-sc-high)' }) : {};
  return (
    <button title={title} onClick={onClick} onMouseEnter={() => setHov(true)} onMouseLeave={() => setHov(false)}
      style={{ display: 'inline-flex', alignItems: 'center', justifyContent: 'center', gap: 8, height: h, padding: '0 ' + pad + 'px',
        borderRadius: h / 2, cursor: 'pointer', fontFamily: 'var(--md-font)', fontSize: size === 'sm' ? 12.5 : (size === 'lg' ? 14 : 13.5),
        boxShadow: elevated ? 'var(--md-elev-2)' : 'none', transition: '.15s', ...v, ...hover, ...style }}>
      {icon && <span data-icon style={{ fontSize: iconSz, fontVariationSettings: iconFill ? "'FILL' 1" : undefined }}>{icon}</span>}
      {children}
    </button>
  );
}
