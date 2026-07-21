import React from 'react';
export const APP_TABS = [
  { id: 'home', label: 'Home', icon: 'home' },
  { id: 'prepare', label: 'Prepare', icon: 'view_in_ar' },
  { id: 'preview', label: 'Preview', icon: 'layers' },
  { id: 'device', label: 'Device', icon: 'cast' },
  { id: 'multi', label: 'Multi-device', icon: 'devices' },
  { id: 'project', label: 'Project', icon: 'folder_open' },
  { id: 'calibration', label: 'Calibration', icon: 'build' },
  { id: 'filament', label: 'Filament', icon: 'palette' },
  { id: 'settings', label: 'Settings', icon: 'settings' },
];
export function TabBar({ tabs = APP_TABS, value, onChange, style }) {
  const [hov, setHov] = React.useState(null);
  return (
    <div style={{ height: 52, flex: '0 0 auto', display: 'flex', alignItems: 'stretch', padding: '0 8px', gap: 2,
      background: 'var(--md-surface)', borderBottom: '1px solid var(--md-outline-variant)', overflowX: 'auto', fontFamily: 'var(--md-font)', ...style }}>
      {tabs.map(t => {
        const on = t.id === value;
        return (
          <button key={t.id} onClick={() => onChange && onChange(t.id)} onMouseEnter={() => setHov(t.id)} onMouseLeave={() => setHov(null)}
            style={{ position: 'relative', display: 'flex', alignItems: 'center', gap: 8, padding: '0 16px', border: 'none',
              background: hov === t.id ? 'var(--md-sc-low)' : 'transparent', cursor: 'pointer', whiteSpace: 'nowrap',
              color: on ? 'var(--md-primary)' : 'var(--md-on-surface-variant)', fontSize: 13.5, fontWeight: on ? 600 : 400 }}>
            <span data-icon style={{ fontSize: 20, fontVariationSettings: on ? "'FILL' 1" : "'FILL' 0" }}>{t.icon}</span>
            <span>{t.label}</span>
            <div style={{ position: 'absolute', left: 12, right: 12, bottom: 0, height: 3, borderRadius: '3px 3px 0 0',
              background: on ? 'var(--md-primary)' : 'transparent' }} />
          </button>
        );
      })}
    </div>
  );
}
