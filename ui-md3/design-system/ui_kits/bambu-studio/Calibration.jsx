window.Screens = window.Screens || {};
window.Screens.Calibration = function Calibration({ notify }) {
  const { Button, Card, SearchField } = MD;
  const cards = [
    { title: 'Flow Dynamics', desc: 'Calibrate pressure advance for sharp corners', icon: 'water_drop' },
    { title: 'Flow Rate', desc: 'Tune extrusion multiplier for accurate walls', icon: 'opacity' },
    { title: 'Max Volumetric Speed', desc: 'Find the fastest reliable flow for a filament', icon: 'speed' },
    { title: 'Temperature Tower', desc: 'Find the ideal nozzle temperature', icon: 'thermostat' },
    { title: 'Retraction Test', desc: 'Reduce stringing and oozing', icon: 'undo' },
    { title: 'Vertical Fine Tuning', desc: 'Correct Z-offset and first layer', icon: 'height' },
  ];
  return (
    <div style={{ position: 'absolute', inset: 0, overflow: 'auto', background: 'var(--md-surface)', padding: '28px 32px' }}>
      <div style={{ maxWidth: 1080, margin: '0 auto' }}>
        <div style={{ display: 'flex', alignItems: 'center', marginBottom: 6 }}>
          <div style={{ fontSize: 20, fontWeight: 700 }}>Calibration</div>
          <div style={{ flex: 1, maxWidth: 340, margin: '0 16px' }}><SearchField placeholder="Search calibrations" /></div>
          <div style={{ flex: 1 }} />
          <Button variant="outlined" icon="history">History</Button>
        </div>
        <div style={{ fontSize: 13.5, color: 'var(--md-on-surface-variant)', marginBottom: 20 }}>Fine-tune your printer and filament for the best possible print quality.</div>
        <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fill,minmax(320px,1fr))', gap: 16 }}>
          {cards.map(c => (
            <Card key={c.title} interactive style={{ display: 'flex', gap: 14, alignItems: 'flex-start', padding: 18, textAlign: 'left' }}
              onClick={() => notify('Starting ' + c.title + ' calibration…', c.icon)}>
              <div style={{ width: 46, height: 46, flex: '0 0 auto', borderRadius: 14, background: 'var(--md-secondary-container)', color: 'var(--md-on-secondary-container)', display: 'flex', alignItems: 'center', justifyContent: 'center' }}><span data-icon style={{ fontSize: 24 }}>{c.icon}</span></div>
              <div style={{ flex: 1, minWidth: 0 }}>
                <div style={{ fontSize: 14.5, fontWeight: 600 }}>{c.title}</div>
                <div style={{ fontSize: 12, color: 'var(--md-on-surface-variant)', marginTop: 3, lineHeight: 1.4 }}>{c.desc}</div>
              </div>
              <span data-icon style={{ fontSize: 22, color: 'var(--md-primary)' }}>arrow_forward</span>
            </Card>
          ))}
        </div>
      </div>
    </div>
  );
};
