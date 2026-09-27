import React from 'react';
export function Snackbar({ icon = 'check_circle', message, actionLabel, onAction, onDismiss, style }) {
  return (
    <div style={{ pointerEvents: 'auto', display: 'flex', alignItems: 'center', gap: 12, padding: '12px 10px 12px 16px',
      background: 'var(--md-inverse-surface)', color: 'var(--md-inverse-on)', borderRadius: 12,
      boxShadow: '0 8px 24px var(--md-shadow)', animation: 'mdfade .2s ease', ...style }}>
      <span data-icon style={{ fontSize: 20, color: 'var(--md-inverse-primary)' }}>{icon}</span>
      <span style={{ flex: 1, fontSize: 13 }}>{message}</span>
      {actionLabel && <button onClick={onAction} style={{ border: 'none', background: 'transparent', color: 'var(--md-inverse-primary)',
        fontWeight: 600, fontSize: 13, cursor: 'pointer', padding: '6px 10px', borderRadius: 8 }}>{actionLabel}</button>}
      <button onClick={onDismiss} style={{ width: 28, height: 28, border: 'none', background: 'transparent', color: 'var(--md-inverse-on)',
        borderRadius: '50%', cursor: 'pointer', opacity: .7 }}><span data-icon style={{ fontSize: 16 }}>close</span></button>
    </div>
  );
}
