const { useState: useDevState } = React;
window.Screens = window.Screens || {};

window.Screens.Device = function Device({ notify }) {
  const [speed, setSpeed] = useDevState('Standard');
  const [lamp, setLamp] = useDevState(true);
  const { Button, IconButton, SectionHeader, SegmentedControl, Switch, Card, ProgressBar } = MD;
  const filaments = window.KIT_FILAMENTS;
  const zBtn = (icon, label) => (
    <button key={label} style={{ height: 40, border: 'none', background: 'var(--md-sc-highest)', color: 'var(--md-on-surface)', borderRadius: 10, cursor: 'pointer', display: 'flex', alignItems: 'center', justifyContent: 'center', gap: 6, fontSize: 13 }}>
      <span data-icon style={{ fontSize: 18 }}>{icon}</span> {label}</button>
  );
  const arrow = (icon) => <button style={{ border: 'none', background: 'var(--md-sc-highest)', color: 'var(--md-on-surface)', borderRadius: 8, cursor: 'pointer' }}><span data-icon style={{ fontSize: 20 }}>{icon}</span></button>;
  return (
    <div data-scheme="device" style={{ position: 'absolute', inset: 0, overflow: 'auto', background: 'var(--md-surface-dim)', padding: 16 }}>
      <div style={{ display: 'flex', gap: 16, alignItems: 'flex-start', maxWidth: 1300, margin: '0 auto' }}>
        <div style={{ flex: 1.5, display: 'flex', flexDirection: 'column', gap: 16, minWidth: 0 }}>
          <div style={{ borderRadius: 'var(--radius)', overflow: 'hidden', border: '1px solid var(--md-outline-variant)', background: '#0c0e13' }}>
            <div style={{ position: 'relative', aspectRatio: '16/9', background: 'repeating-linear-gradient(45deg,#15171d 0 12px,#1b1e25 12px 24px)', display: 'flex', alignItems: 'center', justifyContent: 'center' }}>
              <span data-icon style={{ fontSize: 52, color: '#3a3f47' }}>videocam</span>
              <div style={{ position: 'absolute', left: 14, top: 14, display: 'flex', alignItems: 'center', gap: 6, padding: '4px 10px', background: 'rgba(0,0,0,.55)', borderRadius: 12, color: '#fff', fontSize: 11, fontWeight: 600 }}>
                <span style={{ width: 7, height: 7, borderRadius: '50%', background: 'var(--md-live)', animation: 'pulse 1.6s infinite' }} /> LIVE</div>
              <div style={{ position: 'absolute', right: 12, top: 12, display: 'flex', gap: 6 }}>
                {['hd', 'fullscreen'].map(i => <button key={i} style={{ width: 34, height: 34, border: 'none', background: 'rgba(0,0,0,.5)', color: '#fff', borderRadius: '50%', cursor: 'pointer' }}><span data-icon style={{ fontSize: 18 }}>{i}</span></button>)}
              </div>
              <div style={{ position: 'absolute', left: 14, bottom: 14, display: 'flex', gap: 8 }}>
                <span style={{ padding: '4px 10px', background: 'rgba(0,0,0,.55)', borderRadius: 10, color: '#fff', fontSize: 11.5, fontFamily: 'var(--md-font-mono)' }}>220°C</span>
                <span style={{ padding: '4px 10px', background: 'rgba(0,0,0,.55)', borderRadius: 10, color: '#fff', fontSize: 11.5, fontFamily: 'var(--md-font-mono)' }}>60°C</span>
              </div>
            </div>
          </div>
          <Card style={{ display: 'flex', flexDirection: 'column', gap: 14 }}>
            <div style={{ display: 'flex', gap: 14, alignItems: 'center' }}>
              <div style={{ width: 60, height: 60, borderRadius: 12, background: 'repeating-linear-gradient(45deg, var(--md-sc-high) 0 8px, var(--md-sc) 8px 16px)', display: 'flex', alignItems: 'center', justifyContent: 'center', color: 'var(--md-on-surface-variant)' }}><span data-icon style={{ fontSize: 30 }}>deployed_code</span></div>
              <div style={{ flex: 1, minWidth: 0 }}>
                <div style={{ fontSize: 15, fontWeight: 600 }}>3DBenchy.gcode.3mf</div>
                <div style={{ fontSize: 12.5, color: 'var(--md-on-surface-variant)', marginTop: 2 }}>Layer 124 / 180 · 42 min remaining</div>
              </div>
              <div style={{ fontFamily: 'var(--md-font-mono)', fontSize: 28, fontWeight: 600, color: 'var(--md-primary)' }}>68%</div>
            </div>
            <ProgressBar value={68} />
            <div style={{ display: 'flex', gap: 10 }}>
              <Button variant="tonal" size="lg" icon="pause" style={{ flex: 1 }} onClick={() => notify('Print paused', 'pause')}>Pause</Button>
              <Button variant="danger" size="lg" icon="stop" style={{ flex: 1 }} onClick={() => notify('Print stopped', 'stop')}>Stop</Button>
            </div>
          </Card>
        </div>
        <div style={{ flex: 1, minWidth: 344, display: 'flex', flexDirection: 'column', gap: 16 }}>
          <Card style={{ display: 'flex', flexDirection: 'column', gap: 12 }}>
            <SectionHeader icon="thermostat">Temperature</SectionHeader>
            {[['mode_heat', 'Nozzle', 220, 220], ['radio_button_checked', 'Bed', 60, 60], ['home_work', 'Chamber', 34, '—']].map(t => (
              <div key={t[1]} style={{ display: 'flex', alignItems: 'center', gap: 12, height: 40 }}>
                <span data-icon style={{ fontSize: 22, color: 'var(--md-primary)' }}>{t[0]}</span>
                <span style={{ flex: 1, fontSize: 13.5 }}>{t[1]}</span>
                <span style={{ fontFamily: 'var(--md-font-mono)', fontSize: 15, fontWeight: 500 }}>{t[2]}°</span>
                <span style={{ fontSize: 12, color: 'var(--md-on-surface-variant)', fontFamily: 'var(--md-font-mono)' }}>/ {t[3]}</span>
                <IconButton icon="edit" size={32} filled />
              </div>
            ))}
          </Card>
          <Card style={{ display: 'flex', flexDirection: 'column', gap: 12 }}>
            <SectionHeader icon="speed">Print options</SectionHeader>
            <SegmentedControl options={['Silent', 'Standard', 'Sport', 'Ludicrous'].map(m => ({ id: m, label: m }))} value={speed} onChange={m => { setSpeed(m); notify('Speed: ' + m, 'speed'); }} />
            <div style={{ display: 'flex', alignItems: 'center', gap: 12 }}><span data-icon style={{ fontSize: 20, color: 'var(--md-on-surface-variant)' }}>mode_fan</span><span style={{ width: 100, fontSize: 13 }}>Part cooling</span><input type="range" min={0} max={100} defaultValue={100} style={{ flex: 1, accentColor: 'var(--md-primary)' }} /></div>
            <div style={{ display: 'flex', alignItems: 'center', gap: 12 }}><span data-icon style={{ fontSize: 20, color: 'var(--md-on-surface-variant)' }}>air</span><span style={{ width: 100, fontSize: 13 }}>Aux fan</span><input type="range" min={0} max={100} defaultValue={40} style={{ flex: 1, accentColor: 'var(--md-primary)' }} /></div>
            <div style={{ display: 'flex', alignItems: 'center', gap: 12, height: 36 }}><span data-icon style={{ fontSize: 20, color: 'var(--md-on-surface-variant)' }}>lightbulb</span><span style={{ flex: 1, fontSize: 13 }}>Chamber light</span><Switch checked={lamp} onChange={setLamp} /></div>
          </Card>
          <Card style={{ display: 'flex', flexDirection: 'column', gap: 12 }}>
            <SectionHeader icon="inventory_2" trailing={<span style={{ fontSize: 11, color: 'var(--md-primary)' }}>Humidity: Dry</span>}>AMS</SectionHeader>
            <div style={{ display: 'flex', gap: 8 }}>
              {filaments.map(f => (
                <div key={f.slot} style={{ flex: 1, display: 'flex', flexDirection: 'column', alignItems: 'center', gap: 6, padding: '10px 4px', background: 'var(--md-sc-highest)', borderRadius: 12 }}>
                  <div style={{ width: 30, height: 30, borderRadius: 8, background: f.color, border: '1px solid rgba(0,0,0,.25)' }} />
                  <span style={{ fontSize: 10, color: 'var(--md-on-surface-variant)' }}>{f.type}</span>
                </div>
              ))}
            </div>
          </Card>
          <Card style={{ display: 'flex', gap: 16 }}>
            <div style={{ display: 'flex', flexDirection: 'column', gap: 6, alignItems: 'center' }}>
              <SectionHeader icon="control_camera" style={{ alignSelf: 'flex-start' }}>Move</SectionHeader>
              <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3,40px)', gridTemplateRows: 'repeat(3,40px)', gap: 4, marginTop: 4 }}>
                <div />{arrow('keyboard_arrow_up')}<div />
                {arrow('keyboard_arrow_left')}
                <button style={{ border: 'none', background: 'var(--md-secondary-container)', color: 'var(--md-on-secondary-container)', borderRadius: 8, cursor: 'pointer' }} onClick={() => notify('Homing axes…', 'home')}><span data-icon style={{ fontSize: 18 }}>home</span></button>
                {arrow('keyboard_arrow_right')}
                <div />{arrow('keyboard_arrow_down')}<div />
              </div>
            </div>
            <div style={{ flex: 1, display: 'flex', flexDirection: 'column', gap: 6 }}>
              <SectionHeader>Z axis</SectionHeader>
              {zBtn('keyboard_arrow_up', 'Z +10')}{zBtn('keyboard_arrow_down', 'Z −10')}{zBtn('vertical_align_bottom', 'Extrude')}
            </div>
          </Card>
        </div>
      </div>
    </div>
  );
};
