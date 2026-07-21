const { useState: useSetState } = React;
window.Screens = window.Screens || {};
window.Screens.Settings = function Settings({ notify, theme, setTheme, density, setDensity, accent, setAccent, seeds }) {
  const { SegmentedControl, Switch, SearchField, NavItem } = MD;
  const [nav, setNav] = useSetState('Appearance');
  const [prefs, setPrefs] = useSetState({ autoArrange: true, autoCommit: true, autoSaveToast: false, bundleRepo: true, hints: false, telemetry: false });
  const navItems = [
    ['palette', 'Appearance'], ['settings', 'General'], ['tune', 'Presets'], ['wifi', 'Network'], ['account_tree', 'Version control'], ['info', 'About'],
  ];
  const prefDefs = [
    ['autoArrange', 'Auto-arrange on import', 'Automatically arrange new objects on the plate'],
    ['autoCommit', 'Auto-commit every edit to Git', 'Save each undoable change to the project repository'],
    ['autoSaveToast', 'Show “auto-saved” notifications', 'Pop a snackbar each time an edit is committed'],
    ['bundleRepo', 'Bundle version history into .3mf', 'Include the local Git repo when saving a project'],
    ['hints', 'Show daily tips', 'Display slicing tips on launch'],
    ['telemetry', 'Share anonymous usage data', 'Help improve Bambu Studio'],
  ];
  return (
    <div style={{ position: 'absolute', inset: 0, display: 'flex', background: 'var(--md-surface)' }}>
      <div style={{ width: 'var(--md-settings-nav-w)', flex: '0 0 auto', borderRight: '1px solid var(--md-outline-variant)', background: 'var(--md-sc-low)', padding: '16px 10px', display: 'flex', flexDirection: 'column', gap: 2 }}>
        {navItems.map(n => <NavItem key={n[1]} icon={n[0]} label={n[1]} selected={nav === n[1]} onClick={() => setNav(n[1])} />)}
      </div>
      <div style={{ flex: 1, minWidth: 0, overflowY: 'auto', padding: '28px 32px' }}>
        <div style={{ maxWidth: 720, margin: '0 auto', display: 'flex', flexDirection: 'column', gap: 26 }}>
          <SearchField placeholder="Search settings" />
          <div style={{ display: 'flex', flexDirection: 'column', gap: 16 }}>
            <div style={{ fontSize: 16, fontWeight: 700 }}>Appearance</div>
            <div style={{ display: 'flex', alignItems: 'center', gap: 16 }}>
              <div style={{ width: 150, fontSize: 13.5 }}>Theme</div>
              <SegmentedControl size="md" grow={false} value={theme} onChange={setTheme} options={[{ id: 'light', label: 'Light', icon: 'light_mode' }, { id: 'dark', label: 'Dark', icon: 'dark_mode' }]} />
            </div>
            <div style={{ display: 'flex', alignItems: 'center', gap: 16 }}>
              <div style={{ width: 150, fontSize: 13.5 }}>Density</div>
              <SegmentedControl size="md" grow={false} value={density} onChange={setDensity} options={[{ id: 'comfortable', label: 'Comfortable' }, { id: 'compact', label: 'Compact' }]} />
            </div>
            <div style={{ display: 'flex', alignItems: 'center', gap: 16 }}>
              <div style={{ width: 150, fontSize: 13.5 }}>Accent color</div>
              <div style={{ display: 'flex', gap: 10, alignItems: 'center', flexWrap: 'wrap' }}>
                {seeds.map(s => (
                  <button key={s[0]} title={s[1]} onClick={() => setAccent(s[0])}
                    style={{ width: 32, height: 32, borderRadius: '50%', cursor: 'pointer', background: s[0], border: accent === s[0] ? '2px solid var(--md-on-surface)' : '2px solid transparent', display: 'flex', alignItems: 'center', justifyContent: 'center', color: '#fff' }}>
                    <span data-icon style={{ fontSize: 17, opacity: accent === s[0] ? 1 : 0 }}>check</span>
                  </button>
                ))}
              </div>
            </div>
          </div>
          <div style={{ height: 1, background: 'var(--md-outline-variant)' }} />
          <div style={{ display: 'flex', flexDirection: 'column', gap: 6 }}>
            <div style={{ fontSize: 16, fontWeight: 700, marginBottom: 6 }}>General</div>
            {prefDefs.map(p => (
              <div key={p[0]} style={{ display: 'flex', alignItems: 'center', gap: 16, padding: '10px 0' }}>
                <div style={{ flex: 1 }}>
                  <div style={{ fontSize: 13.5, fontWeight: 500 }}>{p[1]}</div>
                  <div style={{ fontSize: 12, color: 'var(--md-on-surface-variant)', marginTop: 1 }}>{p[2]}</div>
                </div>
                <Switch checked={prefs[p[0]]} onChange={v => { setPrefs({ ...prefs, [p[0]]: v }); notify(p[1] + (v ? ' — on' : ' — off'), 'settings'); }} />
              </div>
            ))}
          </div>
        </div>
      </div>
    </div>
  );
};
