window.Screens = window.Screens || {};
window.Screens.Multi = function Multi({ notify }) {
  const { Button, Card, SearchField, SectionHeader } = MD;
  const devices = [
    { name: 'X1 Carbon', model: 'Bambu Lab X1C', status: 'Printing', pct: 68, dot: 'var(--md-primary)' },
    { name: 'P1S — Studio', model: 'Bambu Lab P1S', status: 'Idle', pct: 0, dot: 'var(--md-outline)' },
    { name: 'A1 mini', model: 'Bambu Lab A1 mini', status: 'Printing', pct: 12, dot: 'var(--md-primary)' },
    { name: 'X1E — Lab', model: 'Bambu Lab X1E', status: 'Offline', pct: 0, dot: 'var(--md-error)' },
  ];
  return (
    <div style={{ position: 'absolute', inset: 0, overflow: 'auto', background: 'var(--md-surface)', padding: '28px 32px' }}>
      <div style={{ maxWidth: 1120, margin: '0 auto' }}>
        <div style={{ display: 'flex', alignItems: 'center', marginBottom: 20 }}>
          <div style={{ fontSize: 20, fontWeight: 700 }}>Device farm</div>
          <div style={{ flex: 1, maxWidth: 340, margin: '0 16px' }}><SearchField placeholder="Search devices" /></div>
          <div style={{ flex: 1 }} />
          <Button variant="filled" size="lg" icon="add" onClick={() => notify('Add device', 'add')}>Add device</Button>
        </div>
        <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fill,minmax(300px,1fr))', gap: 16 }}>
          {devices.map(d => (
            <Card key={d.name} style={{ display: 'flex', flexDirection: 'column', gap: 12, padding: 16 }}>
              <div style={{ display: 'flex', alignItems: 'center', gap: 12 }}>
                <div style={{ width: 44, height: 44, borderRadius: 12, background: 'var(--md-sc-highest)', color: 'var(--md-on-surface-variant)', display: 'flex', alignItems: 'center', justifyContent: 'center' }}><span data-icon style={{ fontSize: 26 }}>print</span></div>
                <div style={{ flex: 1, minWidth: 0 }}><div style={{ fontSize: 14, fontWeight: 600 }}>{d.name}</div><div style={{ fontSize: 11.5, color: 'var(--md-on-surface-variant)' }}>{d.model}</div></div>
                <div style={{ display: 'flex', alignItems: 'center', gap: 5, fontSize: 11.5, color: 'var(--md-on-surface-variant)' }}><span style={{ width: 8, height: 8, borderRadius: '50%', background: d.dot }} /> {d.status}</div>
              </div>
              <div style={{ height: 96, borderRadius: 12, background: 'repeating-linear-gradient(45deg,#15171d 0 10px,#1b1e25 10px 20px)', display: 'flex', alignItems: 'center', justifyContent: 'center', color: '#3a3f47' }}><span data-icon style={{ fontSize: 32 }}>videocam</span></div>
              <div style={{ height: 6, background: 'var(--md-sc-highest)', borderRadius: 4, overflow: 'hidden' }}><div style={{ width: d.pct + '%', height: '100%', background: 'var(--md-primary)' }} /></div>
            </Card>
          ))}
        </div>
      </div>
    </div>
  );
};
