const { useState: useProjState } = React;
window.Screens = window.Screens || {};
window.Screens.Project = function Project({ notify, openHistory }) {
  const [cat, setCat] = useProjState('Model pictures');
  const { Button, SearchField } = MD;
  const cats = [
    { label: 'Model pictures', icon: 'image', count: 3 }, { label: 'Bill of materials', icon: 'receipt_long', count: 1 },
    { label: 'Assembly guide', icon: 'menu_book', count: 2 }, { label: 'Others', icon: 'folder', count: 4 },
  ];
  const files = [
    { name: 'hull_front.png', type: 'PNG', icon: 'image' }, { name: 'hull_rear.png', type: 'PNG', icon: 'image' },
    { name: 'cover.png', type: 'PNG', icon: 'image' }, { name: 'assembly_1.pdf', type: 'PDF', icon: 'picture_as_pdf' },
    { name: 'assembly_2.pdf', type: 'PDF', icon: 'picture_as_pdf' }, { name: 'notes.txt', type: 'TXT', icon: 'description' },
  ];
  const [hovFile, setHovFile] = useProjState(null);
  return (
    <div style={{ position: 'absolute', inset: 0, display: 'flex', background: 'var(--md-surface)' }}>
      <div style={{ width: 250, flex: '0 0 auto', borderRight: '1px solid var(--md-outline-variant)', background: 'var(--md-sc-low)', padding: 'var(--pad)', display: 'flex', flexDirection: 'column', gap: 12, overflowY: 'auto' }}>
        <div style={{ display: 'flex', flexDirection: 'column', gap: 8 }}>
          <div style={{ height: 120, borderRadius: 'var(--radius)', background: 'repeating-linear-gradient(45deg, var(--md-sc-high) 0 10px, var(--md-sc) 10px 20px)', display: 'flex', alignItems: 'center', justifyContent: 'center', color: 'var(--md-on-surface-variant)' }}><span data-icon style={{ fontSize: 40 }}>deployed_code</span></div>
          <div style={{ fontSize: 14, fontWeight: 600 }}>3DBenchy_project</div>
          <div style={{ fontSize: 11.5, color: 'var(--md-on-surface-variant)' }}>by you · 3 objects</div>
        </div>
        <div style={{ height: 1, background: 'var(--md-outline-variant)' }} />
        {cats.map(c => {
          const on = cat === c.label;
          return (
            <button key={c.label} onClick={() => setCat(c.label)}
              style={{ display: 'flex', alignItems: 'center', gap: 10, height: 42, padding: '0 12px', border: 'none', borderRadius: 12, cursor: 'pointer', fontSize: 13, textAlign: 'left',
                background: on ? 'var(--md-secondary-container)' : 'transparent', color: on ? 'var(--md-on-secondary-container)' : 'var(--md-on-surface-variant)' }}>
              <span data-icon style={{ fontSize: 20 }}>{c.icon}</span><span style={{ flex: 1 }}>{c.label}</span><span style={{ fontSize: 11, opacity: .7 }}>{c.count}</span>
            </button>
          );
        })}
        <div style={{ height: 1, background: 'var(--md-outline-variant)' }} />
        <div style={{ padding: 14, background: 'var(--md-secondary-container)', color: 'var(--md-on-secondary-container)', borderRadius: 'var(--radius)', display: 'flex', flexDirection: 'column', gap: 8 }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: 6, fontSize: 11, fontWeight: 700, letterSpacing: '.4px', textTransform: 'uppercase' }}><span data-icon style={{ fontSize: 16 }}>account_tree</span> Version control</div>
          <div style={{ display: 'flex', alignItems: 'center', gap: 8, fontSize: 12, fontFamily: 'var(--md-font-mono)' }}><span data-icon style={{ fontSize: 15 }}>commit</span> main · #a1f9c02</div>
          <div style={{ fontSize: 11.5, opacity: .85 }}>6 commits · auto-saved. Bundled into the .3mf on save.</div>
          <button onClick={openHistory} style={{ height: 34, border: 'none', borderRadius: 17, background: 'var(--md-primary)', color: 'var(--md-on-primary)', fontSize: 12, fontWeight: 600, cursor: 'pointer' }}>View history</button>
        </div>
      </div>
      <div style={{ flex: 1, minWidth: 0, display: 'flex', flexDirection: 'column', overflow: 'hidden' }}>
        <div style={{ padding: 'var(--pad)', display: 'flex', gap: 14, alignItems: 'center', borderBottom: '1px solid var(--md-outline-variant)' }}>
          <div style={{ fontSize: 18, fontWeight: 700, flex: '0 0 auto' }}>{cat}</div>
          <div style={{ flex: 1, maxWidth: 360 }}><SearchField placeholder="Search files" /></div>
          <div style={{ flex: 1 }} />
          <Button variant="filled" icon="add" onClick={() => notify('Add file', 'add')}>Add file</Button>
        </div>
        <div style={{ flex: 1, overflowY: 'auto', padding: 'var(--pad)' }}>
          <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fill,minmax(180px,1fr))', gap: 16 }}>
            {files.map(f => (
              <div key={f.name} onMouseEnter={() => setHovFile(f.name)} onMouseLeave={() => setHovFile(null)}
                style={{ border: '1px solid ' + (hovFile === f.name ? 'var(--md-primary)' : 'var(--md-outline-variant)'), background: 'var(--md-sc-low)', borderRadius: 'var(--radius)', overflow: 'hidden', cursor: 'pointer', transition: 'border-color .15s' }}>
                <div style={{ height: 130, background: 'repeating-linear-gradient(45deg, var(--md-sc-high) 0 10px, var(--md-sc) 10px 20px)', display: 'flex', alignItems: 'center', justifyContent: 'center', color: 'var(--md-on-surface-variant)' }}><span data-icon style={{ fontSize: 38 }}>{f.icon}</span></div>
                <div style={{ padding: '10px 12px', display: 'flex', alignItems: 'center', gap: 8 }}>
                  <span style={{ flex: 1, fontSize: 12.5, fontWeight: 500, overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>{f.name}</span>
                  <span style={{ fontSize: 10, fontWeight: 600, padding: '2px 7px', borderRadius: 7, background: 'var(--md-secondary-container)', color: 'var(--md-on-secondary-container)' }}>{f.type}</span>
                </div>
              </div>
            ))}
          </div>
        </div>
      </div>
    </div>
  );
};
