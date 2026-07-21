const { useState: usePrepState } = React;
window.Screens = window.Screens || {};

const FILAMENTS = [
  { name: 'Bambu PLA Basic', slot: 'AMS A1', type: 'PLA', color: '#22c55e' },
  { name: 'Bambu PLA Basic', slot: 'AMS A2', type: 'PLA', color: '#f5f5f4' },
  { name: 'Bambu PETG HF', slot: 'AMS A3', type: 'PETG', color: '#3b82f6' },
  { name: 'Bambu Support W', slot: 'AMS A4', type: 'PLA-S', color: '#78716c' },
];
window.KIT_FILAMENTS = FILAMENTS;

const GIZMOS = [
  { id: 'move', icon: 'open_with', label: 'Move' }, { id: 'rotate', icon: 'rotate_right', label: 'Rotate' },
  { id: 'scale', icon: 'open_in_full', label: 'Scale' }, { id: 'mirror', icon: 'flip', label: 'Mirror' },
  { divider: true },
  { id: 'lay', icon: 'align_horizontal_left', label: 'Lay on face' }, { id: 'cut', icon: 'carpenter', label: 'Cut' },
  { id: 'support', icon: 'foundation', label: 'Supports' }, { id: 'seam', icon: 'line_start_circle', label: 'Seam painting' },
  { id: 'color', icon: 'brush', label: 'Color painting' }, { divider: true },
  { id: 'text', icon: 'match_case', label: 'Text' }, { id: 'measure', icon: 'straighten', label: 'Measure' },
];
const SCENE_TOOLS = ['center_focus_strong', 'grid_view', 'auto_fix_high', 'content_copy', 'delete', 'undo', 'redo'];
const MANIP = [
  { label: 'Position', x: '128.0', y: '128.0', z: '0.0' }, { label: 'Rotation', x: '0°', y: '0°', z: '45°' },
  { label: 'Scale %', x: '120', y: '120', z: '120' }, { label: 'Size', x: '72.0', y: '37.2', z: '57.6' },
];

window.Screens.Prepare = function Prepare({ notify, openDialog }) {
  const [tool, setTool] = usePrepState('move');
  const [tab, setTab] = usePrepState('Quality');
  const [support, setSupport] = usePrepState(false);
  const { Button, IconButton, SectionHeader, SegmentedControl, Switch, SelectField, ValueField, SearchField, Badge, Viewport } = MD;
  return (
    <div style={{ position: 'absolute', inset: 0, display: 'flex' }}>
      <div style={{ width: 'var(--rail)', flex: '0 0 auto', background: 'var(--md-sc-low)', borderRight: '1px solid var(--md-outline-variant)', display: 'flex', flexDirection: 'column', alignItems: 'center', padding: '8px 0', gap: 3, overflowY: 'auto' }}>
        {GIZMOS.map((g, i) => g.divider ? <div key={i} style={{ width: 28, height: 1, background: 'var(--md-outline-variant)', margin: '4px 0', flex: '0 0 auto' }} />
          : <button key={g.id} title={g.label} onClick={() => { setTool(g.id); notify('Tool: ' + g.label, 'build'); }}
              style={{ width: 44, height: 44, flex: '0 0 auto', border: 'none', borderRadius: 12, cursor: 'pointer', display: 'flex', alignItems: 'center', justifyContent: 'center',
                background: tool === g.id ? 'var(--md-primary)' : 'transparent', color: tool === g.id ? 'var(--md-on-primary)' : 'var(--md-on-surface-variant)' }}>
              <span data-icon style={{ fontSize: 21, fontVariationSettings: tool === g.id ? "'FILL' 1" : "'FILL' 0" }}>{g.icon}</span></button>)}
      </div>
      <div style={{ flex: '1 1 auto', position: 'relative', display: 'flex', flexDirection: 'column', minWidth: 0 }}>
        <div style={{ position: 'absolute', top: 12, left: '50%', transform: 'translateX(-50%)', zIndex: 5, display: 'flex', alignItems: 'center', gap: 3, padding: 5, background: 'var(--md-sc)', border: '1px solid var(--md-outline-variant)', borderRadius: 16, boxShadow: 'var(--md-elev-3)' }}>
          {SCENE_TOOLS.map(s => <IconButton key={s} icon={s} size={40} iconSize={22} shape="square" style={{ borderRadius: 10, width: 40 }} />)}
        </div>
        <Viewport>
          <div style={{ position: 'absolute', left: 16, bottom: 16, width: 70, height: 70 }}>
            <div style={{ position: 'absolute', left: 6, bottom: 6, width: 2, height: 40, background: 'var(--md-axis-z)' }} />
            <div style={{ position: 'absolute', left: 6, bottom: 6, width: 40, height: 2, background: 'var(--md-axis-x)' }} />
            <div style={{ position: 'absolute', left: 8, bottom: 8, width: 26, height: 14, background: 'var(--md-axis-y)', transform: 'skewX(-45deg)', opacity: .85 }} />
            <div style={{ position: 'absolute', left: 1, bottom: 44, fontSize: 10, color: 'var(--md-axis-z)', fontWeight: 700 }}>Z</div>
            <div style={{ position: 'absolute', left: 46, bottom: 0, fontSize: 10, color: 'var(--md-axis-x)', fontWeight: 700 }}>X</div>
          </div>
          <div style={{ position: 'absolute', right: 16, bottom: 16, display: 'flex', flexDirection: 'column', gap: 6, padding: 6, background: 'var(--md-sc)', border: '1px solid var(--md-outline-variant)', borderRadius: 26, boxShadow: 'var(--md-elev-3)' }}>
            <IconButton icon="add" size={40} iconSize={22} /><IconButton icon="remove" size={40} iconSize={22} /><IconButton icon="filter_center_focus" size={40} iconSize={22} />
          </div>
          <div style={{ position: 'absolute', left: '50%', bottom: 16, transform: 'translateX(-50%)', whiteSpace: 'nowrap', display: 'flex', gap: 8, alignItems: 'center', padding: '7px 14px', background: 'var(--md-sc)', border: '1px solid var(--md-outline-variant)', borderRadius: 20, fontSize: 12.5, color: 'var(--md-on-surface-variant)', boxShadow: 'var(--md-elev-2)' }}>
            <span data-icon style={{ fontSize: 16 }}>deployed_code</span> 1 object · 15,842 faces
          </div>
        </Viewport>
        <div style={{ flex: '0 0 auto', height: 'var(--md-prepare-actions-h)', display: 'flex', alignItems: 'center', gap: 12, padding: '0 16px', background: 'var(--md-sc-low)', borderTop: '1px solid var(--md-outline-variant)' }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: 8 }}>
            <button style={{ display: 'flex', alignItems: 'center', gap: 8, height: 40, padding: '0 14px 0 10px', border: '2px solid var(--md-primary)', background: 'var(--md-secondary-container)', color: 'var(--md-on-secondary-container)', borderRadius: 12, cursor: 'pointer', fontSize: 13, fontWeight: 500 }}>
              <span data-icon style={{ fontSize: 20 }}>grid_view</span> Plate 1</button>
            <button title="Add plate" style={{ width: 40, height: 40, border: '1px dashed var(--md-outline)', background: 'transparent', color: 'var(--md-on-surface-variant)', borderRadius: 12, cursor: 'pointer' }}><span data-icon style={{ fontSize: 20 }}>add</span></button>
          </div>
          <div style={{ flex: 1 }} />
          <div style={{ textAlign: 'right', marginRight: 4, lineHeight: 1.3 }}>
            <div style={{ fontFamily: 'var(--md-font-mono)', fontSize: 15, fontWeight: 500 }}>1h 24m</div>
            <div style={{ fontSize: 11, color: 'var(--md-on-surface-variant)' }}>23.4 g · 7.85 m</div>
          </div>
          <Button variant="outlined" size="lg" icon="deployed_code" onClick={() => notify('Slicing plate 1…', 'deployed_code')} style={{ background: 'var(--md-sc-high)' }}>Slice plate</Button>
          <Button variant="filled" size="lg" icon="print" iconFill elevated onClick={() => openDialog('send')}>Print plate</Button>
        </div>
      </div>
      <div style={{ width: 'var(--sidebar)', flex: '0 0 auto', background: 'var(--md-sc-low)', borderLeft: '1px solid var(--md-outline-variant)', display: 'flex', flexDirection: 'column', overflowY: 'auto' }}>
        <div style={{ padding: 'var(--pad)', display: 'flex', flexDirection: 'column', gap: 10 }}>
          <SectionHeader icon="print">Printer</SectionHeader>
          <div style={{ display: 'flex', gap: 10, alignItems: 'center', padding: 10, background: 'var(--md-sc-highest)', borderRadius: 'var(--radius)' }}>
            <div style={{ width: 52, height: 52, borderRadius: 12, background: 'var(--md-sc-lowest)', border: '1px solid var(--md-outline-variant)', display: 'flex', alignItems: 'center', justifyContent: 'center', color: 'var(--md-on-surface-variant)' }}><span data-icon style={{ fontSize: 30 }}>print</span></div>
            <div style={{ flex: 1, minWidth: 0 }}>
              <div style={{ fontSize: 13.5, fontWeight: 600, overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>Bambu Lab X1 Carbon</div>
              <div style={{ display: 'flex', alignItems: 'center', gap: 5, fontSize: 11.5, color: 'var(--md-primary)', marginTop: 2 }}><span style={{ width: 7, height: 7, borderRadius: '50%', background: 'var(--md-primary)' }} /> 0.4 nozzle · Connected</div>
            </div>
            <IconButton icon="edit" size={34} />
          </div>
          <SelectField caption="Bed type" value="Textured PEI Plate" />
        </div>
        <div style={{ height: 1, background: 'var(--md-outline-variant)', margin: '0 var(--pad)' }} />
        <div style={{ padding: 'var(--pad)', display: 'flex', flexDirection: 'column', gap: 10 }}>
          <SectionHeader icon="palette" trailing={<Button variant="outlined" size="sm" icon="sync" style={{ height: 30, fontSize: 11.5, color: 'var(--md-primary)', padding: '0 10px' }} onClick={() => notify('Synced with AMS', 'sync')}>Sync AMS</Button>}>Filament</SectionHeader>
          {FILAMENTS.map(f => (
            <div key={f.slot} style={{ display: 'flex', alignItems: 'center', gap: 10, height: 44, padding: '0 8px', background: 'var(--md-sc-highest)', borderRadius: 12 }}>
              <div style={{ width: 28, height: 28, borderRadius: 8, background: f.color, border: '1px solid rgba(0,0,0,.25)', boxShadow: 'inset 0 0 0 2px var(--md-sc-highest), inset 0 0 0 3px rgba(255,255,255,.15)' }} />
              <div style={{ flex: 1, minWidth: 0 }}>
                <div style={{ fontSize: 12.5, fontWeight: 500, overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>{f.name}</div>
                <div style={{ fontSize: 10.5, color: 'var(--md-on-surface-variant)' }}>{f.slot}</div>
              </div>
              <Badge>{f.type}</Badge>
            </div>
          ))}
          <Button variant="outlined" size="sm" icon="add" style={{ color: 'var(--md-primary)' }} onClick={() => openDialog('addfil')}>Add filament</Button>
        </div>
        <div style={{ height: 1, background: 'var(--md-outline-variant)', margin: '0 var(--pad)' }} />
        <div style={{ padding: 'var(--pad)', display: 'flex', flexDirection: 'column', gap: 10 }}>
          <SectionHeader icon="tune">Process</SectionHeader>
          <SelectField value="0.20mm Standard @BBL X1C" />
          <SegmentedControl options={['Quality', 'Strength', 'Support', 'Others'].map(t => ({ id: t, label: t }))} value={tab} onChange={setTab} />
          <div style={{ display: 'flex', alignItems: 'center', gap: 10, minHeight: 36 }}><div style={{ flex: 1, fontSize: 12.5 }}>Layer height</div><ValueField value="0.20" unit="mm" /></div>
          <div style={{ display: 'flex', alignItems: 'center', gap: 10, minHeight: 36 }}><div style={{ flex: 1, fontSize: 12.5 }}>Sparse infill density</div><ValueField value="15" unit="%" /></div>
          <div style={{ display: 'flex', alignItems: 'center', gap: 10, minHeight: 36 }}><div style={{ flex: 1, fontSize: 12.5 }}>Infill pattern</div><SelectField filled value="Grid" /></div>
          <div style={{ display: 'flex', alignItems: 'center', gap: 10, minHeight: 36 }}><div style={{ flex: 1, fontSize: 12.5 }}>Enable support</div><Switch checked={support} onChange={v => { setSupport(v); notify('Support ' + (v ? 'enabled' : 'disabled'), 'foundation'); }} /></div>
          <button style={{ alignSelf: 'flex-start', display: 'flex', alignItems: 'center', gap: 4, height: 30, padding: '0 6px', border: 'none', background: 'transparent', color: 'var(--md-primary)', fontSize: 12, fontWeight: 500, cursor: 'pointer' }}><span data-icon style={{ fontSize: 17 }}>tune</span> Advanced settings</button>
        </div>
        <div style={{ height: 1, background: 'var(--md-outline-variant)', margin: '0 var(--pad)' }} />
        <div style={{ padding: 'var(--pad)', display: 'flex', flexDirection: 'column', gap: 6 }}>
          <SectionHeader icon="account_tree" style={{ marginBottom: 2 }}>Objects</SectionHeader>
          <SearchField placeholder="Search objects" />
          <div style={{ display: 'flex', alignItems: 'center', gap: 8, height: 36, padding: '0 8px', background: 'var(--md-secondary-container)', color: 'var(--md-on-secondary-container)', borderRadius: 10, marginTop: 4 }}>
            <span data-icon style={{ fontSize: 18 }}>deployed_code</span>
            <span style={{ flex: 1, fontSize: 12.5, fontWeight: 500 }}>3DBenchy.stl</span>
            <span data-icon style={{ fontSize: 17, opacity: .7 }}>visibility</span>
            <div style={{ width: 16, height: 16, borderRadius: 5, background: 'var(--md-primary)', border: '1px solid rgba(0,0,0,.2)' }} />
          </div>
          <div style={{ display: 'flex', alignItems: 'center', gap: 8, height: 32, padding: '0 8px 0 26px', color: 'var(--md-on-surface-variant)', fontSize: 12 }}>
            <span data-icon style={{ fontSize: 16 }}>layers</span> <span style={{ flex: 1 }}>Layers &amp; height range</span>
          </div>
        </div>
        <div style={{ height: 1, background: 'var(--md-outline-variant)', margin: '0 var(--pad)' }} />
        <div style={{ padding: 'var(--pad)', display: 'flex', flexDirection: 'column', gap: 8 }}>
          <SectionHeader icon="transform">Object manipulation</SectionHeader>
          <div style={{ display: 'grid', gridTemplateColumns: 'auto 1fr 1fr 1fr', gap: '6px 8px', alignItems: 'center' }}>
            <div />
            <div style={{ textAlign: 'center', fontSize: 10.5, color: 'var(--md-axis-x)', fontWeight: 600 }}>X</div>
            <div style={{ textAlign: 'center', fontSize: 10.5, color: 'var(--md-axis-y)', fontWeight: 600 }}>Y</div>
            <div style={{ textAlign: 'center', fontSize: 10.5, color: 'var(--md-axis-z)', fontWeight: 600 }}>Z</div>
            {MANIP.map(r => [
              <div key={r.label} style={{ fontSize: 11.5, color: 'var(--md-on-surface-variant)' }}>{r.label}</div>,
              ...[r.x, r.y, r.z].map((v, i) => <div key={r.label + i} style={{ height: 32, display: 'flex', alignItems: 'center', justifyContent: 'center', background: 'var(--md-sc-highest)', borderRadius: 8, fontFamily: 'var(--md-font-mono)', fontSize: 12 }}>{v}</div>),
            ])}
          </div>
        </div>
      </div>
    </div>
  );
};
