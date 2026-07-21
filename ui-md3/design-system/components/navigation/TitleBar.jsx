import React from 'react';
export function TitleBar({ appName = 'Bambu Studio', menus = ['File', 'Edit', 'View', 'Objects', 'Help'], branch = 'main', head = 'a1f9c02',
  projectName = 'Untitled.3mf', onMenu, onHistory, onPalette, onMinimize, onMaximize, onClose, style }) {
  const [hov, setHov] = React.useState(null);
  const ghost = (id, extra) => ({
    onMouseEnter: () => setHov(id), onMouseLeave: () => setHov(null),
    style: { border: 'none', cursor: 'pointer', color: 'var(--md-on-surface-variant)', background: hov === id ? (extra || 'var(--md-sc-high)') : 'transparent' },
  });
  return (
    <div style={{ height: 46, flex: '0 0 auto', display: 'flex', alignItems: 'center', gap: 6, padding: '0 6px 0 12px',
      background: 'var(--md-sc-low)', borderBottom: '1px solid var(--md-outline-variant)', fontFamily: 'var(--md-font)', ...style }}>
      <div style={{ width: 26, height: 26, borderRadius: 8, background: 'var(--md-primary)', color: 'var(--md-on-primary)',
        display: 'flex', alignItems: 'center', justifyContent: 'center', boxShadow: '0 1px 2px var(--md-shadow)' }}>
        <span data-icon style={{ fontSize: 18, fontVariationSettings: "'FILL' 1" }}>deployed_code</span>
      </div>
      <div style={{ fontWeight: 500, fontSize: 14, marginRight: 10, letterSpacing: '.1px', color: 'var(--md-on-surface)' }}>{appName}</div>
      {menus.map(m => (
        <button key={m} onClick={() => onMenu && onMenu(m)} {...ghost('menu-' + m)}
          style={{ ...ghost('menu-' + m).style, height: 30, padding: '0 10px', borderRadius: 8, fontSize: 13 }}>{m}</button>
      ))}
      <div style={{ flex: 1, WebkitAppRegion: 'drag', height: '100%' }} />
      <button onClick={onHistory} title="Version history" {...ghost('hist')}
        style={{ ...ghost('hist').style, display: 'flex', alignItems: 'center', gap: 7, height: 30, padding: '0 12px', marginRight: 6,
          borderRadius: 16, background: hov === 'hist' ? 'var(--md-sc-high)' : 'var(--md-sc)', fontSize: 12 }}>
        <span data-icon style={{ fontSize: 16, color: 'var(--md-primary)' }}>account_tree</span>
        <span style={{ fontFamily: 'var(--md-font-mono)' }}>{branch}</span>
        <span style={{ width: 5, height: 5, borderRadius: '50%', background: 'var(--md-primary)', animation: 'pulse 2s infinite' }} />
        <span style={{ fontFamily: 'var(--md-font-mono)' }}>#{head}</span>
      </button>
      <div style={{ display: 'flex', alignItems: 'center', gap: 2, marginRight: 6, padding: '0 6px', height: 30, borderRadius: 20,
        background: 'var(--md-sc)', color: 'var(--md-on-surface-variant)', fontSize: 12.5 }}>
        <span data-icon style={{ fontSize: 16 }}>description</span>
        <span style={{ maxWidth: 150, overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>{projectName}</span>
      </div>
      <button onClick={onPalette} title="Appearance" {...ghost('pal')}
        style={{ ...ghost('pal').style, width: 34, height: 34, borderRadius: '50%', display: 'flex', alignItems: 'center', justifyContent: 'center' }}>
        <span data-icon style={{ fontSize: 20 }}>palette</span>
      </button>
      <div style={{ width: 1, height: 22, background: 'var(--md-outline-variant)', margin: '0 4px' }} />
      <button onClick={onMinimize} {...ghost('min')} style={{ ...ghost('min').style, width: 38, height: 32, borderRadius: 8 }}><span data-icon style={{ fontSize: 18 }}>remove</span></button>
      <button onClick={onMaximize} {...ghost('max')} style={{ ...ghost('max').style, width: 38, height: 32, borderRadius: 8 }}><span data-icon style={{ fontSize: 15 }}>crop_square</span></button>
      <button onClick={onClose} onMouseEnter={() => setHov('x')} onMouseLeave={() => setHov(null)}
        style={{ width: 38, height: 32, border: 'none', borderRadius: 8, cursor: 'pointer',
          background: hov === 'x' ? 'var(--md-error)' : 'transparent', color: hov === 'x' ? 'var(--md-on-error)' : 'var(--md-on-surface-variant)' }}>
        <span data-icon style={{ fontSize: 18 }}>close</span>
      </button>
    </div>
  );
}
