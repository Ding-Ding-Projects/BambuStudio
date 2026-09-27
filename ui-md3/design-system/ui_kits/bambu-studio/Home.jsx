window.Screens = window.Screens || {};
window.Screens.Home = function Home({ go, notify }) {
  const { Button, Card, SearchField } = MD;
  const quick = [
    ['upload_file', 'Import model', 'STL, STEP, 3MF, OBJ', null],
    ['folder_open', 'Open project', 'Resume a .3mf project', null],
    ['public', 'Explore models', 'Browse the online library', null],
    ['cast', 'My devices', 'Monitor & control printers', 'device'],
  ];
  const recent = [
    ['3DBenchy.3mf', 'Edited 2 min ago · 23.4 g'], ['Phone stand v3.3mf', 'Yesterday · 41.2 g'],
    ['Cable clips ×8.3mf', 'Jul 14 · 12.7 g'], ['Vase spiral.3mf', 'Jul 9 · 88.0 g'],
  ];
  return (
    <div style={{ position: 'absolute', inset: 0, overflowY: 'auto', background: 'var(--md-surface)' }}>
      <div style={{ maxWidth: 1120, margin: '0 auto', padding: '32px 32px 56px', display: 'flex', flexDirection: 'column', gap: 26 }}>
        <div style={{ display: 'flex', gap: 20, alignItems: 'center', padding: '26px 28px', borderRadius: 28, background: 'linear-gradient(120deg, var(--md-primary-container), var(--md-secondary-container))', color: 'var(--md-on-primary-container)' }}>
          <div style={{ width: 64, height: 64, borderRadius: 20, background: 'var(--md-primary)', color: 'var(--md-on-primary)', display: 'flex', alignItems: 'center', justifyContent: 'center', boxShadow: '0 6px 16px var(--md-shadow)' }}><span data-icon style={{ fontSize: 36, fontVariationSettings: "'FILL' 1" }}>deployed_code</span></div>
          <div style={{ flex: 1, minWidth: 0 }}>
            <div style={{ fontSize: 23, fontWeight: 700 }}>Welcome back</div>
            <div style={{ fontSize: 14, opacity: .82, marginTop: 3 }}>Start a new project, open an existing one, or continue where you left off.</div>
          </div>
          <div style={{ display: 'flex', gap: 12 }}>
            <Button variant="filled" size="lg" icon="add" elevated style={{ height: 48, borderRadius: 24 }} onClick={() => go('prepare')}>New Project</Button>
            <Button variant="outlined" size="lg" icon="folder_open" style={{ height: 48, borderRadius: 24, border: '1px solid var(--md-primary)', color: 'var(--md-on-primary-container)' }}>Open</Button>
          </div>
        </div>
        <div style={{ maxWidth: 520 }}><SearchField placeholder="Search models on MakerWorld" /></div>
        <div style={{ display: 'grid', gridTemplateColumns: 'repeat(4,1fr)', gap: 16 }}>
          {quick.map(q => (
            <Card key={q[1]} interactive radius={20} style={{ display: 'flex', flexDirection: 'column', gap: 10, padding: 18 }} onClick={() => q[3] ? go(q[3]) : notify(q[1], q[0])}>
              <div style={{ width: 44, height: 44, borderRadius: 14, background: 'var(--md-secondary-container)', color: 'var(--md-on-secondary-container)', display: 'flex', alignItems: 'center', justifyContent: 'center' }}><span data-icon style={{ fontSize: 24 }}>{q[0]}</span></div>
              <div style={{ fontSize: 14, fontWeight: 600 }}>{q[1]}</div>
              <div style={{ fontSize: 12, color: 'var(--md-on-surface-variant)' }}>{q[2]}</div>
            </Card>
          ))}
        </div>
        <div>
          <div style={{ display: 'flex', alignItems: 'center', gap: 8, marginBottom: 14, fontSize: 16, fontWeight: 600 }}><span data-icon style={{ fontSize: 20 }}>history</span> Recent projects</div>
          <div style={{ display: 'grid', gridTemplateColumns: 'repeat(4,1fr)', gap: 16 }}>
            {recent.map(r => (
              <Card key={r[0]} interactive pad={false} radius={20} style={{ overflow: 'hidden' }} onClick={() => go('prepare')}>
                <div style={{ height: 118, background: 'repeating-linear-gradient(45deg, var(--md-sc-high) 0 10px, var(--md-sc) 10px 20px)', display: 'flex', alignItems: 'center', justifyContent: 'center', color: 'var(--md-on-surface-variant)' }}><span data-icon style={{ fontSize: 40 }}>deployed_code</span></div>
                <div style={{ padding: '12px 14px' }}>
                  <div style={{ fontSize: 13.5, fontWeight: 600, overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>{r[0]}</div>
                  <div style={{ fontSize: 11.5, color: 'var(--md-on-surface-variant)', marginTop: 2 }}>{r[1]}</div>
                </div>
              </Card>
            ))}
          </div>
        </div>
      </div>
    </div>
  );
};
