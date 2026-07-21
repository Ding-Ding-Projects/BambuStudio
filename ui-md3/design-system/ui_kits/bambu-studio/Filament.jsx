window.Screens = window.Screens || {};
window.FILAMENT_ROWS = [
  { name: 'Bambu PLA Basic', vendor: 'Bambu Lab', type: 'PLA', nozzle: '220', bed: '55', color: '#111418' },
  { name: 'Bambu PLA Matte', vendor: 'Bambu Lab', type: 'PLA', nozzle: '210', bed: '55', color: '#e11d2e' },
  { name: 'Bambu PETG HF', vendor: 'Bambu Lab', type: 'PETG', nozzle: '255', bed: '70', color: '#1560d4' },
  { name: 'Bambu ABS', vendor: 'Bambu Lab', type: 'ABS', nozzle: '270', bed: '90', color: '#f5c518' },
  { name: 'Generic TPU', vendor: 'Generic', type: 'TPU', nozzle: '230', bed: '35', color: '#16a34a' },
  { name: 'PolyTerra PLA', vendor: 'Polymaker', type: 'PLA', nozzle: '215', bed: '55', color: '#7c5cff' },
];
window.Screens.Filament = function Filament({ notify, openDialog }) {
  const { Button, IconButton, SearchField, Badge } = MD;
  const cols = '2.4fr 1.4fr 1fr 1fr 1fr 84px';
  return (
    <div style={{ position: 'absolute', inset: 0, overflow: 'auto', background: 'var(--md-surface)', padding: '24px 28px' }}>
      <div style={{ maxWidth: 1080, margin: '0 auto', display: 'flex', flexDirection: 'column', gap: 18 }}>
        <div style={{ display: 'flex', gap: 14, alignItems: 'center' }}>
          <div style={{ fontSize: 20, fontWeight: 700, flex: '0 0 auto' }}>Filament Manager</div>
          <div style={{ flex: 1, maxWidth: 380 }}><SearchField placeholder="Search filaments" /></div>
          <div style={{ flex: 1 }} />
          <Button variant="outlined" icon="file_download" onClick={() => openDialog('export')}>Export all</Button>
          <Button variant="outlined" icon="download" onClick={() => notify('Import filament presets', 'download')}>Import</Button>
          <Button variant="filled" icon="add" onClick={() => openDialog('addfil')}>New filament</Button>
        </div>
        <div style={{ border: '1px solid var(--md-outline-variant)', borderRadius: 'var(--radius)', overflow: 'hidden', background: 'var(--md-sc-low)' }}>
          <div style={{ display: 'grid', gridTemplateColumns: cols, gap: 12, padding: '12px 16px', background: 'var(--md-sc-high)', fontSize: 11, fontWeight: 700, letterSpacing: '.4px', textTransform: 'uppercase', color: 'var(--md-on-surface-variant)' }}>
            <span>Filament</span><span>Vendor</span><span>Type</span><span>Nozzle °C</span><span>Bed °C</span><span />
          </div>
          {window.FILAMENT_ROWS.map(f => (
            <div key={f.name + f.color} style={{ display: 'grid', gridTemplateColumns: cols, gap: 12, padding: '12px 16px', alignItems: 'center', borderTop: '1px solid var(--md-outline-variant)', fontSize: 13 }}>
              <span style={{ display: 'flex', alignItems: 'center', gap: 10, minWidth: 0 }}>
                <span style={{ width: 22, height: 22, flex: '0 0 auto', borderRadius: 6, background: f.color, border: '1px solid rgba(0,0,0,.25)' }} />
                <span style={{ fontWeight: 500, overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>{f.name}</span>
              </span>
              <span style={{ color: 'var(--md-on-surface-variant)' }}>{f.vendor}</span>
              <span><Badge>{f.type}</Badge></span>
              <span style={{ fontFamily: 'var(--md-font-mono)' }}>{f.nozzle}</span>
              <span style={{ fontFamily: 'var(--md-font-mono)' }}>{f.bed}</span>
              <span style={{ display: 'flex', gap: 2, justifyContent: 'flex-end' }}>
                <IconButton icon="file_download" size={32} title="Export filament" onClick={() => notify('Exported ' + f.name, 'file_download')} />
                <IconButton icon="more_vert" size={32} />
              </span>
            </div>
          ))}
        </div>
      </div>
    </div>
  );
};
