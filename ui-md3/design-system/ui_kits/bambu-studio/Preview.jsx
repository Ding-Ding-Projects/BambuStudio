const { useState: usePrevState } = React;
window.Screens = window.Screens || {};

const LEGEND = [
  ['Inner wall', '#00e0ff'], ['Outer wall', '#ffd500'], ['Overhang wall', '#3c82f6'], ['Sparse infill', '#b28cff'],
  ['Internal solid infill', '#ff7f50'], ['Top surface', '#ff4d6d'], ['Bottom surface', '#59c96b'], ['Bridge', '#8be9fd'],
  ['Support', '#8a8a5c'], ['Travel', '#7a7a7a'],
];

window.Screens.Preview = function Preview({ notify }) {
  const [layer, setLayer] = usePrevState(124);
  const [scheme, setScheme] = usePrevState('Line type');
  const [opts, setOpts] = usePrevState({ Travel: true, Seams: false, Retractions: false, Wipe: false });
  const maxLayer = 180;
  const { Chip, IconButton, SectionHeader, Viewport } = MD;
  const optIcons = { Travel: 'route', Seams: 'line_start_circle', Retractions: 'u_turn_left', Wipe: 'water_drop' };
  return (
    <div data-scheme="preview" style={{ position: 'absolute', inset: 0, display: 'flex' }}>
      <div style={{ flex: 1, position: 'relative', display: 'flex', flexDirection: 'column', minWidth: 0 }}>
        <Viewport model={false}>
          <div style={{ position: 'absolute', left: '50%', top: '44%', transform: 'translate(-50%,-50%)', width: 150, height: 164, filter: 'drop-shadow(0 20px 16px rgba(0,0,0,.35))' }}>
            <div style={{ position: 'absolute', inset: 0, background: '#d2477e', clipPath: 'polygon(50% 2%, 98% 27%, 50% 52%, 2% 27%)' }} />
            <div style={{ position: 'absolute', inset: 0, background: 'repeating-linear-gradient(-6deg, #ef6c3e 0 5px, #c85a34 5px 6px)', clipPath: 'polygon(2% 27%, 50% 52%, 50% 98%, 2% 73%)' }} />
            <div style={{ position: 'absolute', inset: 0, background: 'repeating-linear-gradient(6deg, #b8542f 0 5px, #97452a 5px 6px)', clipPath: 'polygon(98% 27%, 98% 73%, 50% 98%, 50% 52%)' }} />
          </div>
          <div style={{ position: 'absolute', right: 22, top: 0, bottom: 20, display: 'flex', flexDirection: 'column', alignItems: 'center', justifyContent: 'center', gap: 10 }}>
            <div style={{ fontFamily: 'var(--md-font-mono)', fontSize: 11.5, color: 'var(--md-on-surface)', background: 'var(--md-sc)', padding: '4px 9px', borderRadius: 8, border: '1px solid var(--md-outline-variant)' }}>Z {(layer * 0.2).toFixed(2)} mm</div>
            <input type="range" min={1} max={maxLayer} value={layer} onChange={e => setLayer(Number(e.target.value))}
              style={{ writingMode: 'vertical-lr', direction: 'rtl', width: 12, height: 300, accentColor: 'var(--md-primary)', cursor: 'pointer' }} />
            <div style={{ fontFamily: 'var(--md-font-mono)', fontSize: 11.5, color: 'var(--md-on-surface-variant)' }}>{layer} / {maxLayer}</div>
          </div>
          <div style={{ position: 'absolute', left: 16, top: 16, display: 'flex', gap: 8, alignItems: 'center', padding: '7px 14px', background: 'var(--md-sc)', border: '1px solid var(--md-outline-variant)', borderRadius: 20, fontSize: 12.5, color: 'var(--md-on-surface-variant)', boxShadow: 'var(--md-elev-2)' }}>
            <span data-icon style={{ fontSize: 16 }}>layers</span> Sliced · {maxLayer} layers
          </div>
        </Viewport>
        <div style={{ flex: '0 0 auto', height: 'var(--md-preview-timeline-h)', display: 'flex', alignItems: 'center', gap: 14, padding: '0 20px', background: 'var(--md-sc-low)', borderTop: '1px solid var(--md-outline-variant)' }}>
          <IconButton icon="skip_previous" size={38} iconSize={22} />
          <button onClick={() => notify('Playing toolpath', 'play_arrow')} style={{ width: 44, height: 44, border: 'none', background: 'var(--md-primary)', color: 'var(--md-on-primary)', borderRadius: '50%', cursor: 'pointer', display: 'flex', alignItems: 'center', justifyContent: 'center' }}><span data-icon style={{ fontSize: 24, fontVariationSettings: "'FILL' 1" }}>play_arrow</span></button>
          <IconButton icon="skip_next" size={38} iconSize={22} />
          <input type="range" min={0} max={12380} defaultValue={8420} style={{ flex: 1, accentColor: 'var(--md-primary)', cursor: 'pointer' }} />
          <span style={{ fontFamily: 'var(--md-font-mono)', fontSize: 12, color: 'var(--md-on-surface-variant)', minWidth: 120, textAlign: 'right' }}>Move 8420 / 12380</span>
        </div>
      </div>
      <div style={{ width: 'var(--sidebar)', flex: '0 0 auto', background: 'var(--md-sc-low)', borderLeft: '1px solid var(--md-outline-variant)', display: 'flex', flexDirection: 'column', overflowY: 'auto', padding: 'var(--pad)', gap: 18 }}>
        <div style={{ display: 'flex', flexDirection: 'column', gap: 10 }}>
          <SectionHeader icon="palette">Color scheme</SectionHeader>
          <div style={{ display: 'flex', gap: 6, flexWrap: 'wrap' }}>
            {['Line type', 'Speed', 'Layer time', 'Height', 'Filament'].map(c => <Chip key={c} label={c} selected={scheme === c} onClick={() => setScheme(c)} />)}
          </div>
        </div>
        <div style={{ display: 'flex', flexDirection: 'column', gap: 8 }}>
          <SectionHeader>{scheme}</SectionHeader>
          {LEGEND.map(g => (
            <div key={g[0]} style={{ display: 'flex', alignItems: 'center', gap: 10, height: 28 }}>
              <span style={{ width: 16, height: 16, borderRadius: 4, background: g[1] }} />
              <span style={{ fontSize: 12.5, color: 'var(--md-on-surface)' }}>{g[0]}</span>
            </div>
          ))}
        </div>
        <div style={{ display: 'flex', flexDirection: 'column', gap: 10 }}>
          <SectionHeader icon="tune">Options</SectionHeader>
          <div style={{ display: 'flex', gap: 6, flexWrap: 'wrap' }}>
            {Object.keys(opts).map(o => <Chip key={o} label={o} icon={optIcons[o]} selected={opts[o]} onClick={() => setOpts({ ...opts, [o]: !opts[o] })} />)}
          </div>
        </div>
        <div style={{ display: 'flex', flexDirection: 'column', gap: 8, padding: 14, background: 'var(--md-sc-highest)', borderRadius: 'var(--radius)' }}>
          <SectionHeader icon="insights">Statistics</SectionHeader>
          {[['Estimated time', '1h 24m'], ['Filament', '23.4 g · 7.85 m'], ['Est. cost', '$0.47'], ['Layers', '180']].map(r => (
            <div key={r[0]} style={{ display: 'flex', justifyContent: 'space-between', fontSize: 13 }}>
              <span style={{ color: 'var(--md-on-surface-variant)' }}>{r[0]}</span>
              <span style={{ fontFamily: 'var(--md-font-mono)', fontWeight: 500 }}>{r[1]}</span>
            </div>
          ))}
        </div>
      </div>
    </div>
  );
};
