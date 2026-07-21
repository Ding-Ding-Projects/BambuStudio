const { useState } = React;

const TABS = [
  { id: 'home', label: 'Home', icon: 'home' }, { id: 'prepare', label: 'Prepare', icon: 'view_in_ar' },
  { id: 'preview', label: 'Preview', icon: 'layers' }, { id: 'device', label: 'Device', icon: 'cast' },
  { id: 'multi', label: 'Multi-device', icon: 'devices' }, { id: 'project', label: 'Project', icon: 'folder_open' },
  { id: 'calibration', label: 'Calibration', icon: 'build' }, { id: 'filament', label: 'Filament', icon: 'palette' },
  { id: 'settings', label: 'Settings', icon: 'settings' },
];
const SEEDS = [['#22c55e','Green'],['#7c5cff','Purple'],['#14b8a6','Teal'],['#3b82f6','Blue'],['#f97316','Orange'],['#ec4899','Pink']];

function hexToHsl(hex){let h=hex.replace('#','');if(h.length===3)h=h.split('').map(c=>c+c).join('');const r=parseInt(h.slice(0,2),16)/255,g=parseInt(h.slice(2,4),16)/255,b=parseInt(h.slice(4,6),16)/255;const mx=Math.max(r,g,b),mn=Math.min(r,g,b);let hu=0,s=0,l=(mx+mn)/2;if(mx!==mn){const d=mx-mn;s=l>0.5?d/(2-mx-mn):d/(mx+mn);if(mx===r)hu=(g-b)/d+(g<b?6:0);else if(mx===g)hu=(b-r)/d+2;else hu=(r-g)/d+4;hu/=6;}return{h:Math.round(hu*360),s:Math.round(s*100),l:Math.round(l*100)};}
// accentVars() — the design source's seed→roles regeneration (see readme.md)
function accentVars(seed, theme){
  if(seed==='#22c55e') return {}; // brand default: keep the hand-tuned token values
  const c=hexToHsl(seed); const h=c.h, s=Math.max(32,Math.min(92,c.s));
  const H=l=>'hsl('+h+' '+s+'% '+l+'%)'; const Hs=(sat,l)=>'hsl('+h+' '+sat+'% '+l+'%)';
  if(theme==='dark') return {'--md-primary':H(76),'--md-on-primary':H(16),'--md-primary-container':Hs(Math.round(s*0.9),28),'--md-on-primary-container':H(90),'--md-accent':H(70),'--md-inverse-primary':H(38),'--md-secondary-container':Hs(Math.round(s*0.35),26),'--md-on-secondary-container':H(88)};
  return {'--md-primary':H(36),'--md-on-primary':'hsl(0 0% 100%)','--md-primary-container':Hs(Math.round(s*0.7),88),'--md-on-primary-container':H(12),'--md-accent':H(38),'--md-inverse-primary':H(76),'--md-secondary-container':Hs(Math.round(s*0.45),90),'--md-on-secondary-container':H(20)};
}

function App() {
  const [view, setView] = useState('prepare');
  const [theme, setTheme] = useState('dark');
  const [density, setDensity] = useState('comfortable');
  const [accent, setAccent] = useState('#22c55e');
  const [pop, setPop] = useState(false);
  const [dialog, setDialog] = useState(null);
  const [history, setHistory] = useState(false);
  const [snacks, setSnacks] = useState([]);
  const [hovTab, setHovTab] = useState(null);
  const notify = (message, icon) => {
    const id = Date.now() + Math.random();
    setSnacks(s => [...s, { id, message, icon: icon || 'check_circle' }].slice(-3));
    setTimeout(() => setSnacks(s => s.filter(x => x.id !== id)), 4000);
  };
  const { IconButton, SegmentedControl } = MD;
  const Screen = window.Screens[view.charAt(0).toUpperCase() + view.slice(1)];
  const screenProps = { go: setView, notify, openDialog: setDialog, openHistory: () => setHistory(true), theme, setTheme, density, setDensity, accent, setAccent, seeds: SEEDS };
  return (
    <div data-theme={theme} data-density={density}
      style={{ position: 'absolute', inset: 0, display: 'flex', flexDirection: 'column', fontFamily: 'var(--md-font)', fontSize: 'var(--fs)',
        background: 'var(--md-surface)', color: 'var(--md-on-surface)', overflow: 'hidden', WebkitFontSmoothing: 'antialiased', ...accentVars(accent, theme) }}>
      {/* title bar */}
      <div style={{ height: 'var(--md-topbar-h)', flex: '0 0 auto', display: 'flex', alignItems: 'center', gap: 6, padding: '0 6px 0 12px', background: 'var(--md-sc-low)', borderBottom: '1px solid var(--md-outline-variant)' }}>
        <div style={{ width: 26, height: 26, borderRadius: 8, background: 'var(--md-primary)', color: 'var(--md-on-primary)', display: 'flex', alignItems: 'center', justifyContent: 'center', boxShadow: 'var(--md-elev-1)' }}>
          <span data-icon style={{ fontSize: 18, fontVariationSettings: "'FILL' 1" }}>deployed_code</span>
        </div>
        <div style={{ fontWeight: 500, fontSize: 14, marginRight: 10, letterSpacing: '.1px' }}>Bambu Studio</div>
        {['File', 'Edit', 'View', 'Objects', 'Help'].map(m => (
          <button key={m} onClick={() => notify(m + ' menu', 'menu')} style={{ height: 30, padding: '0 10px', border: 'none', background: 'transparent', color: 'var(--md-on-surface-variant)', borderRadius: 8, fontSize: 13, cursor: 'pointer' }}>{m}</button>
        ))}
        <div style={{ flex: 1, height: '100%' }} />
        <button onClick={() => setHistory(true)} title="Version history — every edit is auto-saved to this project's Git repo"
          style={{ display: 'flex', alignItems: 'center', gap: 7, height: 30, padding: '0 12px', marginRight: 6, border: 'none', borderRadius: 16, background: 'var(--md-sc)', color: 'var(--md-on-surface-variant)', cursor: 'pointer', fontSize: 12 }}>
          <span data-icon style={{ fontSize: 16, color: 'var(--md-primary)' }}>account_tree</span>
          <span style={{ fontFamily: 'var(--md-font-mono)' }}>main</span>
          <span style={{ width: 5, height: 5, borderRadius: '50%', background: 'var(--md-primary)', animation: 'pulse 2s infinite' }} />
          <span style={{ fontFamily: 'var(--md-font-mono)' }}>#a1f9c02</span>
        </button>
        <div style={{ display: 'flex', alignItems: 'center', gap: 2, marginRight: 6, padding: '0 6px', height: 30, borderRadius: 20, background: 'var(--md-sc)', color: 'var(--md-on-surface-variant)', fontSize: 12.5 }}>
          <span data-icon style={{ fontSize: 16 }}>description</span>
          <span style={{ maxWidth: 150, overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>3DBenchy.3mf</span>
        </div>
        <IconButton icon="palette" size={34} iconSize={20} title="Appearance" onClick={() => setPop(!pop)} />
        <div style={{ width: 1, height: 22, background: 'var(--md-outline-variant)', margin: '0 4px' }} />
        <IconButton icon="remove" shape="square" size={32} iconSize={18} />
        <IconButton icon="crop_square" shape="square" size={32} iconSize={15} />
        <IconButton icon="close" shape="square" size={32} iconSize={18} danger />
      </div>
      {/* tab bar */}
      <div style={{ height: 'var(--md-navbar-h)', flex: '0 0 auto', display: 'flex', alignItems: 'stretch', padding: '0 8px', gap: 2, background: 'var(--md-surface)', borderBottom: '1px solid var(--md-outline-variant)', overflowX: 'auto' }}>
        {TABS.map(t => {
          const on = t.id === view;
          return (
            <button key={t.id} data-screen-label={t.label} onClick={() => setView(t.id)} onMouseEnter={() => setHovTab(t.id)} onMouseLeave={() => setHovTab(null)}
              style={{ position: 'relative', display: 'flex', alignItems: 'center', gap: 8, padding: '0 16px', border: 'none', cursor: 'pointer', whiteSpace: 'nowrap',
                background: hovTab === t.id ? 'var(--md-sc-low)' : 'transparent', color: on ? 'var(--md-primary)' : 'var(--md-on-surface-variant)', fontSize: 13.5, fontWeight: on ? 600 : 400 }}>
              <span data-icon style={{ fontSize: 20, fontVariationSettings: on ? "'FILL' 1" : "'FILL' 0" }}>{t.icon}</span>
              <span>{t.label}</span>
              <div style={{ position: 'absolute', left: 12, right: 12, bottom: 0, height: 3, borderRadius: '3px 3px 0 0', background: on ? 'var(--md-primary)' : 'transparent' }} />
            </button>
          );
        })}
      </div>
      {/* body */}
      <div style={{ flex: '1 1 auto', position: 'relative', overflow: 'hidden', background: 'var(--md-surface-dim)' }}>
        <Screen {...screenProps} />
      </div>
      {/* appearance popover */}
      {pop && (
        <div onClick={() => setPop(false)} style={{ position: 'absolute', inset: 0, zIndex: 40 }}>
          <div onClick={e => e.stopPropagation()} style={{ position: 'absolute', top: 52, right: 12, width: 'var(--md-popover-w)', background: 'var(--md-sc)', border: '1px solid var(--md-outline-variant)', borderRadius: 24, boxShadow: 'var(--md-elev-4)', padding: 18, display: 'flex', flexDirection: 'column', gap: 16, animation: 'mdfade .18s ease' }}>
            <div style={{ fontSize: 15, fontWeight: 600, display: 'flex', alignItems: 'center', gap: 8 }}><span data-icon style={{ fontSize: 20 }}>palette</span> Appearance</div>
            <div style={{ display: 'flex', flexDirection: 'column', gap: 8 }}>
              <div style={{ fontSize: 12, color: 'var(--md-on-surface-variant)' }}>Theme</div>
              <SegmentedControl size="md" value={theme} onChange={setTheme} options={[{ id: 'light', label: 'Light', icon: 'light_mode' }, { id: 'dark', label: 'Dark', icon: 'dark_mode' }]} />
            </div>
            <div style={{ display: 'flex', flexDirection: 'column', gap: 8 }}>
              <div style={{ fontSize: 12, color: 'var(--md-on-surface-variant)' }}>Density</div>
              <SegmentedControl size="md" value={density} onChange={setDensity} options={[{ id: 'comfortable', label: 'Comfortable' }, { id: 'compact', label: 'Compact' }]} />
            </div>
            <div style={{ display: 'flex', flexDirection: 'column', gap: 8 }}>
              <div style={{ fontSize: 12, color: 'var(--md-on-surface-variant)' }}>Accent color</div>
              <div style={{ display: 'flex', gap: 10, flexWrap: 'wrap' }}>
                {SEEDS.map(s => (
                  <button key={s[0]} title={s[1]} onClick={() => setAccent(s[0])}
                    style={{ width: 34, height: 34, borderRadius: '50%', cursor: 'pointer', background: s[0], border: accent === s[0] ? '2px solid var(--md-on-surface)' : '2px solid transparent', display: 'flex', alignItems: 'center', justifyContent: 'center', color: '#fff' }}>
                    <span data-icon style={{ fontSize: 18, opacity: accent === s[0] ? 1 : 0 }}>check</span>
                  </button>
                ))}
              </div>
            </div>
          </div>
        </div>
      )}
      {/* overlays */}
      {history && <window.Overlays.HistoryDrawer onClose={() => setHistory(false)} notify={notify} />}
      {dialog === 'export' && <window.Overlays.ExportDialog onClose={() => setDialog(null)} notify={notify} />}
      {dialog === 'send' && <window.Overlays.SendDialog onClose={() => setDialog(null)} notify={notify} />}
      {dialog === 'addfil' && <window.Overlays.AddFilamentDialog onClose={() => setDialog(null)} notify={notify} />}
      {/* snackbars */}
      <div style={{ position: 'absolute', left: '50%', bottom: 22, transform: 'translateX(-50%)', zIndex: 90, display: 'flex', flexDirection: 'column-reverse', gap: 10, pointerEvents: 'none', width: 'min(560px,92vw)' }}>
        {snacks.map(n => (
          <div key={n.id} style={{ pointerEvents: 'auto', display: 'flex', alignItems: 'center', gap: 12, padding: '12px 10px 12px 16px', background: 'var(--md-inverse-surface)', color: 'var(--md-inverse-on)', borderRadius: 12, boxShadow: 'var(--md-elev-4)', animation: 'mdfade .2s ease' }}>
            <span data-icon style={{ fontSize: 20, color: 'var(--md-inverse-primary)' }}>{n.icon}</span>
            <span style={{ flex: 1, fontSize: 13 }}>{n.message}</span>
            <button onClick={() => setSnacks(s => s.filter(x => x.id !== n.id))} style={{ width: 28, height: 28, border: 'none', background: 'transparent', color: 'var(--md-inverse-on)', borderRadius: '50%', cursor: 'pointer', opacity: .7 }}><span data-icon style={{ fontSize: 16 }}>close</span></button>
          </div>
        ))}
      </div>
    </div>
  );
}
ReactDOM.createRoot(document.getElementById('app')).render(<App />);
