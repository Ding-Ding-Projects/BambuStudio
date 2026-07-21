const { useState: useOvState } = React;
window.Overlays = {};

const HISTORY = [
  { hash: 'a1f9c02', full: 'a1f9c02e8b34', message: 'Set sparse infill density → 15%', icon: 'tune', time: '2 min ago', author: 'You', files: 'process_settings.json', diff: [{ label: 'Sparse infill density', from: '10%', to: '15%' }, { label: 'Infill pattern', from: 'Gyroid', to: 'Grid' }] },
  { hash: '7d3e5b8', full: '7d3e5b8a1c90', message: 'Rotate 3DBenchy 45° around Z', icon: 'rotate_right', time: '5 min ago', author: 'You', files: '3DBenchy.stl', diff: [{ label: 'Rotation Z', from: '0°', to: '45°' }] },
  { hash: 'e0b41aa', full: 'e0b41aa77d12', message: 'Scale 3DBenchy 100% → 120%', icon: 'open_in_full', time: '6 min ago', author: 'You', files: '3DBenchy.stl', diff: [{ label: 'Scale', from: '100%', to: '120%' }, { label: 'Size Z', from: '48.0mm', to: '57.6mm' }] },
  { hash: 'c92f7d1', full: 'c92f7d10ab55', message: 'Move 3DBenchy to plate center', icon: 'open_with', time: '9 min ago', author: 'You', files: '3DBenchy.stl', diff: [{ label: 'Position X', from: '96.0mm', to: '128.0mm' }, { label: 'Position Y', from: '110.0mm', to: '128.0mm' }] },
  { hash: '4b8a1f0', full: '4b8a1f0d3e77', message: 'Add 3DBenchy.stl', icon: 'add', time: '12 min ago', author: 'You', files: '3DBenchy.stl, project.json', diff: [{ label: 'Objects', from: '0', to: '1' }, { label: 'Faces', from: '—', to: '15,842' }] },
  { hash: '0000000', full: '000000000000', message: 'Initialize project repository', icon: 'flag', time: '12 min ago', author: 'Bambu Studio', files: 'project.3mf', diff: [{ label: 'Repository', from: '—', to: 'created' }] },
];

window.Overlays.HistoryDrawer = function HistoryDrawer({ onClose, notify }) {
  const [sel, setSel] = useOvState('a1f9c02');
  const { IconButton, SearchField } = MD;
  return (
    <div onClick={onClose} style={{ position: 'absolute', inset: 0, zIndex: 55, background: 'var(--md-scrim)', animation: 'scrimin .2s' }}>
      <div onClick={e => e.stopPropagation()} style={{ position: 'absolute', top: 0, right: 0, bottom: 0, width: 'var(--md-history-drawer-w)', maxWidth: '92vw', background: 'var(--md-sc)', borderLeft: '1px solid var(--md-outline-variant)', boxShadow: '-8px 0 30px var(--md-shadow)', display: 'flex', flexDirection: 'column', animation: 'mdfade .22s ease' }}>
        <div style={{ padding: '18px 18px 14px', display: 'flex', alignItems: 'center', gap: 10, borderBottom: '1px solid var(--md-outline-variant)' }}>
          <span data-icon style={{ fontSize: 22, color: 'var(--md-primary)' }}>account_tree</span>
          <div style={{ flex: 1 }}>
            <div style={{ fontSize: 15, fontWeight: 600 }}>Version history</div>
            <div style={{ fontSize: 11.5, color: 'var(--md-on-surface-variant)', fontFamily: 'var(--md-font-mono)' }}>main · {HISTORY.length} commits</div>
          </div>
          <IconButton icon="close" onClick={onClose} />
        </div>
        <div style={{ padding: '12px 18px', display: 'flex', alignItems: 'center', gap: 8, background: 'var(--md-secondary-container)', color: 'var(--md-on-secondary-container)', fontSize: 12 }}>
          <span data-icon style={{ fontSize: 18 }}>bolt</span> Auto-commit is on — every edit is saved automatically.
        </div>
        <div style={{ padding: '12px 18px 4px' }}><SearchField placeholder="Search commits" /></div>
        <div style={{ flex: 1, overflowY: 'auto', padding: '8px 0' }}>
          {HISTORY.map(h => {
            const on = sel === h.hash;
            return (
              <div key={h.hash}>
                <button onClick={() => setSel(on ? null : h.hash)} style={{ width: '100%', textAlign: 'left', display: 'flex', gap: 12, padding: '10px 18px', border: 'none', background: 'transparent', cursor: 'pointer' }}>
                  <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', flex: '0 0 auto' }}>
                    <div style={{ width: 30, height: 30, borderRadius: '50%', background: on ? 'var(--md-primary)' : 'var(--md-sc-highest)', color: on ? 'var(--md-on-primary)' : 'var(--md-on-surface-variant)', display: 'flex', alignItems: 'center', justifyContent: 'center' }}><span data-icon style={{ fontSize: 17 }}>{h.icon}</span></div>
                    <div style={{ width: 2, flex: 1, background: 'var(--md-outline-variant)', marginTop: 2, minHeight: 8 }} />
                  </div>
                  <div style={{ flex: 1, minWidth: 0, paddingBottom: 6 }}>
                    <div style={{ fontSize: 13, fontWeight: 500, color: 'var(--md-on-surface)' }}>{h.message}</div>
                    <div style={{ display: 'flex', alignItems: 'center', gap: 8, marginTop: 3 }}>
                      <span style={{ fontSize: 11, fontFamily: 'var(--md-font-mono)', color: 'var(--md-primary)' }}>#{h.hash}</span>
                      <span style={{ fontSize: 11, color: 'var(--md-on-surface-variant)' }}>{h.time}</span>
                    </div>
                  </div>
                  <span data-icon style={{ fontSize: 20, color: 'var(--md-on-surface-variant)', flex: '0 0 auto' }}>{on ? 'expand_less' : 'expand_more'}</span>
                </button>
                {on && (
                  <div style={{ margin: '0 18px 12px 48px', background: 'var(--md-sc-low)', border: '1px solid var(--md-outline-variant)', borderRadius: 14, overflow: 'hidden', animation: 'mdfade .16s ease' }}>
                    <div style={{ height: 118, background: 'radial-gradient(120% 100% at 50% 0%, var(--md-surface-bright), var(--md-surface-dim))', position: 'relative', display: 'flex', alignItems: 'center', justifyContent: 'center', borderBottom: '1px solid var(--md-outline-variant)' }}>
                      <div style={{ position: 'relative', width: 92, height: 100, filter: 'drop-shadow(0 8px 8px rgba(0,0,0,.3))' }}>
                        <div style={{ position: 'absolute', inset: 0, background: 'color-mix(in srgb, var(--md-primary) 84%, #fff)', clipPath: 'polygon(50% 2%, 98% 27%, 50% 52%, 2% 27%)' }} />
                        <div style={{ position: 'absolute', inset: 0, background: 'color-mix(in srgb, var(--md-primary) 60%, #000 8%)', clipPath: 'polygon(2% 27%, 50% 52%, 50% 98%, 2% 73%)' }} />
                        <div style={{ position: 'absolute', inset: 0, background: 'color-mix(in srgb, var(--md-primary) 42%, #000 22%)', clipPath: 'polygon(98% 27%, 98% 73%, 50% 98%, 50% 52%)' }} />
                      </div>
                      <div style={{ position: 'absolute', left: 10, top: 10, fontSize: 10, fontFamily: 'var(--md-font-mono)', color: 'var(--md-on-surface-variant)', background: 'var(--md-sc)', padding: '2px 7px', borderRadius: 8 }}>Plate 1 · snapshot</div>
                    </div>
                    <div style={{ padding: '12px 14px', display: 'flex', flexDirection: 'column', gap: 10 }}>
                      <div style={{ display: 'flex', flexDirection: 'column', gap: 4, fontSize: 11, color: 'var(--md-on-surface-variant)' }}>
                        <div style={{ display: 'flex', alignItems: 'center', gap: 6 }}><span data-icon style={{ fontSize: 14 }}>person</span> {h.author} · {h.time}</div>
                        <div style={{ display: 'flex', alignItems: 'center', gap: 6 }}><span data-icon style={{ fontSize: 14 }}>tag</span> <span style={{ fontFamily: 'var(--md-font-mono)' }}>{h.full}</span></div>
                        <div style={{ display: 'flex', alignItems: 'center', gap: 6 }}><span data-icon style={{ fontSize: 14 }}>description</span> {h.files}</div>
                      </div>
                      <div style={{ height: 1, background: 'var(--md-outline-variant)' }} />
                      <div style={{ fontSize: 10.5, fontWeight: 600, letterSpacing: '.4px', textTransform: 'uppercase', color: 'var(--md-on-surface-variant)' }}>Changes in this commit</div>
                      {h.diff.map(d => (
                        <div key={d.label} style={{ display: 'flex', alignItems: 'center', gap: 8, fontSize: 12 }}>
                          <span style={{ flex: 1, minWidth: 0, color: 'var(--md-on-surface)', overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>{d.label}</span>
                          <span style={{ fontFamily: 'var(--md-font-mono)', color: 'var(--md-error)', textDecoration: 'line-through', opacity: .75 }}>{d.from}</span>
                          <span data-icon style={{ fontSize: 15, color: 'var(--md-on-surface-variant)' }}>arrow_forward</span>
                          <span style={{ fontFamily: 'var(--md-font-mono)', color: 'var(--md-primary)' }}>{d.to}</span>
                        </div>
                      ))}
                      <button onClick={() => notify('Restored project to #' + h.hash, 'history')} style={{ height: 36, marginTop: 2, border: 'none', borderRadius: 18, background: 'var(--md-secondary-container)', color: 'var(--md-on-secondary-container)', fontSize: 12, fontWeight: 600, cursor: 'pointer', display: 'flex', alignItems: 'center', justifyContent: 'center', gap: 6, width: '100%' }}>
                        <span data-icon style={{ fontSize: 16 }}>restore</span> Restore this version</button>
                    </div>
                  </div>
                )}
              </div>
            );
          })}
        </div>
        <div style={{ padding: '14px 18px', borderTop: '1px solid var(--md-outline-variant)', fontSize: 11.5, color: 'var(--md-on-surface-variant)', display: 'flex', alignItems: 'center', gap: 8 }}>
          <span data-icon style={{ fontSize: 16 }}>save</span> This repository is bundled into the .3mf when you save.
        </div>
      </div>
    </div>
  );
};

window.Overlays.ExportDialog = function ExportDialog({ onClose, notify }) {
  const rows = window.FILAMENT_ROWS;
  const [q, setQ] = useOvState('');
  const [type, setType] = useOvState('All');
  const [vendor, setVendor] = useOvState('All');
  const [format, setFormat] = useOvState('.bbsflmt');
  const [sel, setSel] = useOvState(() => Object.fromEntries(rows.map(r => [r.name, true])));
  const { Chip, IconButton, SearchField, Badge, Button, Checkbox } = MD;
  let filtered = rows.filter(r => (type === 'All' || r.type === type) && (vendor === 'All' || r.vendor === vendor));
  if (q) { try { const re = new RegExp(q, 'gi'); filtered = filtered.filter(r => re.test(r.name)); } catch (e) { filtered = filtered.filter(r => r.name.toLowerCase().includes(q.toLowerCase())); } }
  const count = filtered.filter(r => sel[r.name]).length;
  const allOn = count === filtered.length && filtered.length > 0;
  const label = (t) => <span style={{ fontSize: 11, fontWeight: 600, letterSpacing: '.4px', textTransform: 'uppercase', color: 'var(--md-on-surface-variant)', width: 56 }}>{t}</span>;
  return (
    <div onClick={onClose} style={{ position: 'absolute', inset: 0, zIndex: 60, background: 'var(--md-scrim)', display: 'flex', alignItems: 'center', justifyContent: 'center', animation: 'scrimin .2s' }}>
      <div onClick={e => e.stopPropagation()} style={{ width: 580, maxWidth: '94vw', maxHeight: '88vh', background: 'var(--md-sc)', borderRadius: 28, boxShadow: 'var(--md-elev-5)', display: 'flex', flexDirection: 'column', overflow: 'hidden', animation: 'mdfade .2s' }}>
        <div style={{ padding: '22px 24px 14px', display: 'flex', alignItems: 'center', gap: 12 }}>
          <div style={{ width: 44, height: 44, borderRadius: 14, background: 'var(--md-primary-container)', color: 'var(--md-on-primary-container)', display: 'flex', alignItems: 'center', justifyContent: 'center' }}><span data-icon style={{ fontSize: 24 }}>file_download</span></div>
          <div style={{ flex: 1 }}><div style={{ fontSize: 18, fontWeight: 600 }}>Export filaments</div><div style={{ fontSize: 12.5, color: 'var(--md-on-surface-variant)' }}>Filter, select and bulk-export presets</div></div>
          <IconButton icon="close" onClick={onClose} />
        </div>
        <div style={{ padding: '0 24px 14px', display: 'flex', flexDirection: 'column', gap: 12 }}>
          <SearchField placeholder="Name containing… (supports regex)" onQuery={setQ} />
          <div style={{ display: 'flex', alignItems: 'center', gap: 8, flexWrap: 'wrap' }}>{label('Type')}
            {['All', 'PLA', 'PETG', 'ABS', 'TPU'].map(t => <Chip key={t} label={t} selected={type === t} onClick={() => setType(t)} />)}</div>
          <div style={{ display: 'flex', alignItems: 'center', gap: 8, flexWrap: 'wrap' }}>{label('Vendor')}
            {['All', 'Bambu Lab', 'Generic', 'Polymaker'].map(t => <Chip key={t} label={t} tonal selected={vendor === t} onClick={() => setVendor(t)} />)}</div>
        </div>
        <div style={{ padding: '8px 24px', display: 'flex', alignItems: 'center', gap: 10, borderTop: '1px solid var(--md-outline-variant)', background: 'var(--md-sc-low)' }}>
          <Checkbox checked={allOn} label="Select all" onChange={() => { const next = { ...sel }; filtered.forEach(r => next[r.name] = !allOn); setSel(next); }} />
          <div style={{ flex: 1 }} />
          <span style={{ fontSize: 12, color: 'var(--md-on-surface-variant)', fontFamily: 'var(--md-font-mono)' }}>{count} of {filtered.length} selected</span>
        </div>
        <div style={{ flex: 1, overflowY: 'auto', padding: '6px 12px', minHeight: 120 }}>
          {filtered.map(r => (
            <button key={r.name} onClick={() => setSel({ ...sel, [r.name]: !sel[r.name] })}
              style={{ width: '100%', textAlign: 'left', display: 'flex', alignItems: 'center', gap: 12, padding: '9px 12px', border: 'none', borderRadius: 12, cursor: 'pointer', background: sel[r.name] ? 'var(--md-secondary-container)' : 'transparent' }}>
              <span data-icon style={{ fontSize: 22, color: sel[r.name] ? 'var(--md-primary)' : 'var(--md-on-surface-variant)' }}>{sel[r.name] ? 'check_box' : 'check_box_outline_blank'}</span>
              <span style={{ width: 22, height: 22, borderRadius: 6, background: r.color, border: '1px solid rgba(0,0,0,.25)', flex: '0 0 auto' }} />
              <span style={{ flex: 1, minWidth: 0, fontSize: 13.5, fontWeight: 500, overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap', color: 'var(--md-on-surface)' }}>{r.name}</span>
              <span style={{ fontSize: 11.5, color: 'var(--md-on-surface-variant)', width: 80 }}>{r.vendor}</span>
              <Badge>{r.type}</Badge>
              <span style={{ fontFamily: 'var(--md-font-mono)', fontSize: 11.5, color: 'var(--md-on-surface-variant)', width: 78, textAlign: 'right' }}>{r.nozzle}/{r.bed}°</span>
            </button>
          ))}
          {filtered.length === 0 && <div style={{ padding: '36px 12px', textAlign: 'center', color: 'var(--md-on-surface-variant)', fontSize: 13 }}>No filaments match your filter.</div>}
        </div>
        <div style={{ padding: '14px 24px', borderTop: '1px solid var(--md-outline-variant)', display: 'flex', alignItems: 'center', gap: 10, flexWrap: 'wrap' }}>
          <span style={{ fontSize: 11, fontWeight: 600, letterSpacing: '.4px', textTransform: 'uppercase', color: 'var(--md-on-surface-variant)' }}>Format</span>
          {['.bbsflmt', 'JSON', 'ZIP bundle'].map(t => <Chip key={t} label={t} mono selected={format === t} onClick={() => setFormat(t)} />)}
          <div style={{ flex: 1 }} />
          <Button variant="text" onClick={onClose}>Cancel</Button>
          <Button variant="filled" icon="file_download" onClick={() => { notify('Exported ' + count + ' filament presets as ' + format, 'file_download'); onClose(); }}>Export {count}</Button>
        </div>
      </div>
    </div>
  );
};

window.Overlays.SendDialog = function SendDialog({ onClose, notify }) {
  const { Button, IconButton, Switch } = MD;
  const [opts, setOpts] = useOvState({ Timelapse: true, 'Bed leveling': true, 'Flow dynamics calibration': false });
  const icons = { Timelapse: 'videocam', 'Bed leveling': 'grid_on', 'Flow dynamics calibration': 'tune' };
  return (
    <div onClick={onClose} style={{ position: 'absolute', inset: 0, zIndex: 60, background: 'var(--md-scrim)', display: 'flex', alignItems: 'center', justifyContent: 'center', animation: 'scrimin .2s' }}>
      <div onClick={e => e.stopPropagation()} style={{ width: 460, maxWidth: '92vw', background: 'var(--md-sc)', borderRadius: 28, boxShadow: 'var(--md-elev-5)', padding: 24, display: 'flex', flexDirection: 'column', gap: 16, animation: 'mdfade .2s' }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: 12 }}>
          <div style={{ width: 44, height: 44, borderRadius: 14, background: 'var(--md-primary-container)', color: 'var(--md-on-primary-container)', display: 'flex', alignItems: 'center', justifyContent: 'center' }}><span data-icon style={{ fontSize: 24 }}>print</span></div>
          <div style={{ flex: 1 }}><div style={{ fontSize: 18, fontWeight: 600 }}>Print Plate 1</div><div style={{ fontSize: 12.5, color: 'var(--md-on-surface-variant)' }}>Send to printer over the network</div></div>
          <IconButton icon="close" onClick={onClose} />
        </div>
        <div style={{ display: 'flex', alignItems: 'center', gap: 12, padding: 12, background: 'var(--md-sc-highest)', borderRadius: 16 }}>
          <div style={{ width: 40, height: 40, borderRadius: 10, background: 'var(--md-sc-lowest)', color: 'var(--md-on-surface-variant)', display: 'flex', alignItems: 'center', justifyContent: 'center' }}><span data-icon style={{ fontSize: 24 }}>print</span></div>
          <div style={{ flex: 1 }}><div style={{ fontSize: 13.5, fontWeight: 600 }}>Bambu Lab X1 Carbon</div><div style={{ fontSize: 11.5, color: 'var(--md-primary)' }}>● Connected · Idle</div></div>
          <span data-icon style={{ fontSize: 20, color: 'var(--md-on-surface-variant)' }}>expand_more</span>
        </div>
        <div style={{ display: 'flex', flexDirection: 'column', gap: 8 }}>
          <div style={{ fontSize: 11, fontWeight: 600, letterSpacing: '.5px', textTransform: 'uppercase', color: 'var(--md-on-surface-variant)' }}>AMS mapping</div>
          <div style={{ display: 'flex', gap: 8 }}>
            {window.KIT_FILAMENTS.map(f => (
              <div key={f.slot} style={{ flex: 1, display: 'flex', flexDirection: 'column', alignItems: 'center', gap: 4, padding: '8px 4px', background: 'var(--md-sc-highest)', borderRadius: 10 }}>
                <div style={{ width: 24, height: 24, borderRadius: 6, background: f.color, border: '1px solid rgba(0,0,0,.25)' }} />
                <span style={{ fontSize: 9.5, color: 'var(--md-on-surface-variant)' }}>{f.type}</span>
              </div>
            ))}
          </div>
        </div>
        <div style={{ display: 'flex', flexDirection: 'column' }}>
          {Object.keys(opts).map(o => (
            <div key={o} style={{ display: 'flex', alignItems: 'center', gap: 10, height: 40 }}>
              <span data-icon style={{ fontSize: 20, color: 'var(--md-on-surface-variant)' }}>{icons[o]}</span>
              <span style={{ flex: 1, fontSize: 13 }}>{o}</span>
              <Switch checked={opts[o]} onChange={v => setOpts({ ...opts, [o]: v })} style={{ transform: 'scale(.91)' }} />
            </div>
          ))}
        </div>
        <div style={{ display: 'flex', gap: 10, marginTop: 2 }}>
          <Button variant="text" size="lg" onClick={onClose}>Cancel</Button>
          <Button variant="filled" size="lg" icon="send" style={{ flex: 1 }} onClick={() => { notify('Plate 1 sent to X1 Carbon', 'send'); onClose(); }}>Send print</Button>
        </div>
      </div>
    </div>
  );
};

window.Overlays.AddFilamentDialog = function AddFilamentDialog({ onClose, notify }) {
  const { Button, IconButton, SearchField } = MD;
  const [q, setQ] = useOvState('');
  const [hov, setHov] = useOvState(null);
  const rows = window.FILAMENT_ROWS.filter(r => !q || r.name.toLowerCase().includes(q.toLowerCase()));
  return (
    <div onClick={onClose} style={{ position: 'absolute', inset: 0, zIndex: 60, background: 'var(--md-scrim)', display: 'flex', alignItems: 'center', justifyContent: 'center', animation: 'scrimin .2s' }}>
      <div onClick={e => e.stopPropagation()} style={{ width: 520, maxWidth: '92vw', maxHeight: '86vh', background: 'var(--md-sc)', borderRadius: 28, boxShadow: 'var(--md-elev-5)', display: 'flex', flexDirection: 'column', overflow: 'hidden', animation: 'mdfade .2s' }}>
        <div style={{ padding: '22px 24px 12px', display: 'flex', alignItems: 'center', gap: 12 }}>
          <div style={{ width: 44, height: 44, borderRadius: 14, background: 'var(--md-primary-container)', color: 'var(--md-on-primary-container)', display: 'flex', alignItems: 'center', justifyContent: 'center' }}><span data-icon style={{ fontSize: 24 }}>palette</span></div>
          <div style={{ flex: 1 }}><div style={{ fontSize: 18, fontWeight: 600 }}>Add filament</div><div style={{ fontSize: 12.5, color: 'var(--md-on-surface-variant)' }}>Pick a preset to add to this project</div></div>
          <IconButton icon="close" onClose={onClose} onClick={onClose} />
        </div>
        <div style={{ padding: '0 24px 12px' }}><SearchField placeholder="Search filament presets" onQuery={setQ} /></div>
        <div style={{ flex: 1, overflowY: 'auto', padding: '4px 16px', display: 'grid', gridTemplateColumns: '1fr 1fr', gap: 10, alignContent: 'start' }}>
          {rows.map(f => (
            <div key={f.name + f.color} onMouseEnter={() => setHov(f.name)} onMouseLeave={() => setHov(null)}
              style={{ display: 'flex', alignItems: 'center', gap: 10, padding: 12, background: hov === f.name ? 'var(--md-sc-high)' : 'var(--md-sc-low)', border: '1px solid ' + (hov === f.name ? 'var(--md-primary)' : 'var(--md-outline-variant)'), borderRadius: 14, cursor: 'pointer' }}>
              <div style={{ width: 30, height: 30, borderRadius: 8, background: f.color, border: '1px solid rgba(0,0,0,.25)', flex: '0 0 auto' }} />
              <div style={{ flex: 1, minWidth: 0 }}>
                <div style={{ fontSize: 13, fontWeight: 600, overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>{f.name}</div>
                <div style={{ fontSize: 11, color: 'var(--md-on-surface-variant)' }}>{f.vendor} · {f.type}</div>
              </div>
            </div>
          ))}
        </div>
        <div style={{ padding: '14px 24px', borderTop: '1px solid var(--md-outline-variant)', display: 'flex', justifyContent: 'flex-end', gap: 10 }}>
          <Button variant="text" size="lg" onClick={onClose}>Cancel</Button>
          <Button variant="filled" size="lg" icon="add" onClick={() => { notify('Added filament to project', 'add'); onClose(); }}>Add to project</Button>
        </div>
      </div>
    </div>
  );
};
