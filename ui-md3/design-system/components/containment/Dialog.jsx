import React from 'react';
export function Dialog({ icon, title, subtitle, width = 460, onClose, footer, flush = false, style, children }) {
  return (
    <div onClick={onClose} style={{ position: 'absolute', inset: 0, zIndex: 60, background: 'var(--md-scrim)',
      display: 'flex', alignItems: 'center', justifyContent: 'center', animation: 'scrimin .2s' }}>
      <div onClick={e => e.stopPropagation()}
        style={{ width, maxWidth: '92vw', maxHeight: '88vh', background: 'var(--md-sc)', borderRadius: 28,
          boxShadow: '0 16px 44px var(--md-shadow)', display: 'flex', flexDirection: 'column', overflow: 'hidden', animation: 'mdfade .2s', ...style }}>
        <div style={{ padding: '22px 24px ' + (flush ? '12px' : '14px'), display: 'flex', alignItems: 'center', gap: 12 }}>
          <div style={{ width: 44, height: 44, borderRadius: 14, background: 'var(--md-primary-container)', color: 'var(--md-on-primary-container)',
            display: 'flex', alignItems: 'center', justifyContent: 'center', flex: '0 0 auto' }}>
            <span data-icon style={{ fontSize: 24 }}>{icon}</span>
          </div>
          <div style={{ flex: 1, minWidth: 0 }}>
            <div style={{ fontSize: 18, fontWeight: 600 }}>{title}</div>
            {subtitle && <div style={{ fontSize: 12.5, color: 'var(--md-on-surface-variant)' }}>{subtitle}</div>}
          </div>
          <button onClick={onClose} style={{ width: 36, height: 36, border: 'none', background: 'transparent', color: 'var(--md-on-surface-variant)', borderRadius: '50%', cursor: 'pointer' }}>
            <span data-icon style={{ fontSize: 20 }}>close</span>
          </button>
        </div>
        <div style={{ flex: 1, overflowY: 'auto', padding: flush ? 0 : '0 24px', display: 'flex', flexDirection: 'column', gap: 16 }}>{children}</div>
        {footer && <div style={{ padding: '14px 24px', borderTop: flush ? '1px solid var(--md-outline-variant)' : 'none',
          display: 'flex', alignItems: 'center', justifyContent: 'flex-end', gap: 10, flexWrap: 'wrap' }}>{footer}</div>}
      </div>
    </div>
  );
}
