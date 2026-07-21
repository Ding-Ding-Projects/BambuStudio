// Kit-local mirror of /components primitives (no module system in this standalone shell).
const { useState } = React;
const MD = window.MD = {};

MD.Button = function Button({ variant = 'filled', size = 'md', icon, iconFill, elevated, onClick, title, style, children }) {
  const [hov, setHov] = useState(false);
  const h = { sm: 36, md: 42, lg: 44 }[size] || 42;
  const pad = { sm: 16, md: 18, lg: 22 }[size] || 18;
  const v = {
    filled: { background: 'var(--md-primary)', color: 'var(--md-on-primary)', border: 'none', fontWeight: 600 },
    tonal: { background: 'var(--md-secondary-container)', color: 'var(--md-on-secondary-container)', border: 'none', fontWeight: 600 },
    outlined: { background: 'transparent', color: 'var(--md-on-surface)', border: '1px solid var(--md-outline)', fontWeight: 500 },
    text: { background: 'transparent', color: 'var(--md-primary)', border: 'none', fontWeight: 600 },
    danger: { background: 'transparent', color: 'var(--md-error)', border: '1px solid var(--md-error)', fontWeight: 600 },
  }[variant];
  const hover = hov ? (variant === 'filled' ? { filter: 'brightness(1.06)' } : variant === 'tonal' ? { filter: 'brightness(1.04)' } : { background: variant === 'text' ? 'var(--md-secondary-container)' : 'var(--md-sc-high)' }) : {};
  return <button title={title} onClick={onClick} onMouseEnter={() => setHov(true)} onMouseLeave={() => setHov(false)}
    style={{ display: 'inline-flex', alignItems: 'center', justifyContent: 'center', gap: 8, height: h, padding: '0 ' + pad + 'px', borderRadius: h / 2, cursor: 'pointer', fontFamily: 'var(--md-font)', fontSize: size === 'sm' ? 12.5 : size === 'lg' ? 14 : 13.5, boxShadow: elevated ? 'var(--md-elev-2)' : 'none', transition: '.15s', ...v, ...hover, ...style }}>
    {icon && <span data-icon style={{ fontSize: size === 'sm' ? 18 : 20, fontVariationSettings: iconFill ? "'FILL' 1" : undefined }}>{icon}</span>}{children}</button>;
};

MD.IconButton = function IconButton({ icon, size = 36, iconSize, title, danger, filled, shape = 'circle', onClick, style }) {
  const [hov, setHov] = useState(false);
  return <button title={title} onClick={onClick} onMouseEnter={() => setHov(true)} onMouseLeave={() => setHov(false)}
    style={{ width: size + (shape === 'square' ? 4 : 0), height: size, border: 'none', cursor: 'pointer', borderRadius: shape === 'circle' ? '50%' : 8,
      background: hov ? (danger ? 'var(--md-error)' : 'var(--md-sc-high)') : filled ? 'var(--md-sc-highest)' : 'transparent',
      color: hov && danger ? 'var(--md-on-error)' : 'var(--md-on-surface-variant)', display: 'inline-flex', alignItems: 'center', justifyContent: 'center', transition: '.15s', ...style }}>
    <span data-icon style={{ fontSize: iconSize || (size >= 40 ? 22 : size >= 34 ? 19 : 17) }}>{icon}</span></button>;
};

MD.Chip = function Chip({ label, icon, selected, tonal, mono, onClick, style }) {
  const [hov, setHov] = useState(false);
  const sel = selected
    ? tonal ? { background: 'var(--md-secondary-container)', color: 'var(--md-on-secondary-container)', border: '1px solid var(--md-primary)' }
            : { background: 'var(--md-primary)', color: 'var(--md-on-primary)', border: '1px solid var(--md-primary)' }
    : { background: hov ? 'var(--md-sc-high)' : 'transparent', color: 'var(--md-on-surface-variant)', border: '1px solid var(--md-outline)' };
  return <button onClick={onClick} onMouseEnter={() => setHov(true)} onMouseLeave={() => setHov(false)}
    style={{ display: 'inline-flex', alignItems: 'center', gap: 6, height: 30, padding: '0 13px', borderRadius: 15, cursor: 'pointer', fontSize: 12, fontWeight: 500, transition: '.15s', fontFamily: mono ? 'var(--md-font-mono)' : 'var(--md-font)', ...sel, ...style }}>
    {icon && <span data-icon style={{ fontSize: 16 }}>{icon}</span>}{label}</button>;
};

MD.SegmentedControl = function SegmentedControl({ options, value, onChange, size = 'sm', grow = true, style }) {
  const md = size === 'md';
  return <div style={{ display: 'flex', gap: md ? 6 : 4, padding: md ? 4 : 3, background: 'var(--md-sc-highest)', borderRadius: md ? 14 : 12, ...style }}>
    {options.map(o => { const on = o.id === value; return <button key={o.id} onClick={() => onChange && onChange(o.id)}
      style={{ flex: grow ? 1 : 'none', height: md ? 36 : 30, padding: '0 ' + (md ? 18 : 10) + 'px', border: 'none', borderRadius: md ? 11 : 9, cursor: 'pointer', fontSize: md ? 12.5 : 11.5, fontWeight: 500, display: 'flex', alignItems: 'center', justifyContent: 'center', gap: 6,
        background: on ? 'var(--md-primary)' : 'transparent', color: on ? 'var(--md-on-primary)' : 'var(--md-on-surface-variant)', transition: '.15s' }}>
      {o.icon && <span data-icon style={{ fontSize: 18 }}>{o.icon}</span>}{o.label}</button>; })}</div>;
};

MD.Switch = function Switch({ checked, onChange, style }) {
  return <button onClick={() => onChange && onChange(!checked)}
    style={{ width: 44, height: 24, borderRadius: 14, position: 'relative', cursor: 'pointer', padding: 0, flex: '0 0 auto', transition: '.15s',
      border: '2px solid ' + (checked ? 'var(--md-primary)' : 'var(--md-outline)'), background: checked ? 'var(--md-primary)' : 'transparent', ...style }}>
    <span style={{ position: 'absolute', top: '50%', transform: 'translateY(-50%)', transition: '.15s', borderRadius: '50%', left: checked ? 22 : 4, width: checked ? 16 : 12, height: checked ? 16 : 12, background: checked ? 'var(--md-on-primary)' : 'var(--md-outline)' }} /></button>;
};

MD.SectionHeader = function SectionHeader({ icon, trailing, style, children }) {
  return <div style={{ display: 'flex', alignItems: 'center', gap: 6, ...style }}>
    <div style={{ flex: 1, display: 'flex', alignItems: 'center', gap: 6, fontSize: 11, fontWeight: 600, letterSpacing: '.6px', color: 'var(--md-on-surface-variant)', textTransform: 'uppercase' }}>
      {icon && <span data-icon style={{ fontSize: 16 }}>{icon}</span>}{children}</div>{trailing}</div>;
};

MD.Badge = ({ style, children }) => <span style={{ fontSize: 11, fontWeight: 600, padding: '2px 8px', borderRadius: 7, background: 'var(--md-secondary-container)', color: 'var(--md-on-secondary-container)', ...style }}>{children}</span>;
MD.ProgressBar = ({ value, height = 8, style }) => <div style={{ height, background: 'var(--md-sc-highest)', borderRadius: 6, overflow: 'hidden', ...style }}><div style={{ width: value + '%', height: '100%', background: 'var(--md-primary)', borderRadius: 6, transition: 'width .3s' }} /></div>;

MD.SelectField = function SelectField({ value, filled, caption, onClick, style }) {
  const inner = filled ? { height: 34, padding: '0 8px 0 12px', background: 'var(--md-sc-highest)', border: 'none', borderRadius: 10, gap: 4 }
    : { height: 38, padding: '0 12px', background: 'transparent', border: '1px solid var(--md-outline)', borderRadius: 10, justifyContent: 'space-between', width: '100%' };
  return <div style={{ display: 'flex', flexDirection: 'column', gap: 3, ...style }}>
    {caption && <div style={{ fontSize: 10.5, color: 'var(--md-on-surface-variant)', paddingLeft: 4 }}>{caption}</div>}
    <button onClick={onClick} style={{ display: 'flex', alignItems: 'center', cursor: 'pointer', color: 'var(--md-on-surface)', fontSize: 12.5, fontFamily: 'var(--md-font)', ...inner }}>
      <span style={{ fontWeight: filled ? 400 : 500, overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>{value}</span>
      <span data-icon style={{ fontSize: filled ? 16 : 18, color: 'var(--md-on-surface-variant)' }}>expand_more</span></button></div>;
};

MD.ValueField = ({ value, unit, minWidth = 96, style }) => <div style={{ display: 'flex', alignItems: 'center', gap: 6, height: 34, padding: '0 4px 0 12px', background: 'var(--md-sc-highest)', borderRadius: 10, minWidth, justifyContent: 'flex-end', ...style }}>
  <span style={{ fontFamily: 'var(--md-font-mono)', fontSize: 12.5, fontWeight: 500, color: 'var(--md-on-surface)' }}>{value}</span>
  {unit && <span style={{ fontSize: 11, color: 'var(--md-on-surface-variant)', paddingRight: 8 }}>{unit}</span>}</div>;

MD.SearchField = function SearchField({ placeholder = 'Search', onQuery, style }) {
  const [value, setValue] = useState('');
  const [regex, setRegex] = useState(false);
  return <div style={{ position: 'relative', width: '100%', fontFamily: 'var(--md-font)', ...style }}>
    <div style={{ display: 'flex', alignItems: 'center', gap: 4, height: 40, padding: '0 5px 0 14px', background: 'var(--md-sc-highest)', border: '1px solid var(--md-outline)', borderRadius: 22 }}>
      <span data-icon style={{ fontSize: 20, color: 'var(--md-on-surface-variant)' }}>search</span>
      <input value={value} placeholder={placeholder} onChange={e => { setValue(e.target.value); onQuery && onQuery(e.target.value); }}
        style={{ flex: 1, minWidth: 0, border: 'none', background: 'transparent', outline: 'none', color: 'var(--md-on-surface)', fontSize: 13.5, fontFamily: regex ? 'var(--md-font-mono)' : 'inherit' }} />
      {!!value && <MD.IconButton icon="close" size={30} title="Clear" onClick={() => { setValue(''); onQuery && onQuery(''); }} />}
      <button title="Use regular expression" onClick={() => setRegex(!regex)}
        style={{ minWidth: 32, height: 30, padding: '0 4px', border: 'none', borderRadius: 8, cursor: 'pointer', fontFamily: 'var(--md-font-mono)', fontSize: 12.5, fontWeight: 700,
          background: regex ? 'var(--md-primary)' : 'var(--md-sc-low)', color: regex ? 'var(--md-on-primary)' : 'var(--md-on-surface-variant)' }}>.*</button>
    </div></div>;
};

MD.Card = function Card({ interactive, pad = true, radius, onClick, style, children }) {
  const [hov, setHov] = useState(false);
  return <div onClick={onClick} onMouseEnter={() => setHov(true)} onMouseLeave={() => setHov(false)}
    style={{ background: 'var(--md-sc-low)', borderRadius: radius || 'var(--radius)', cursor: interactive ? 'pointer' : 'default',
      border: '1px solid ' + (interactive && hov ? 'var(--md-primary)' : 'var(--md-outline-variant)'), padding: pad ? 'var(--pad)' : 0, transition: 'border-color .15s', ...style }}>{children}</div>;
};

// Shared viewport mock: build plate + isometric model
MD.Viewport = function Viewport({ children, model = true, modelStyle }) {
  return <div style={{ flex: 1, position: 'relative', overflow: 'hidden', background: 'radial-gradient(120% 100% at 50% 0%, var(--md-surface-bright), var(--md-surface-dim) 70%)' }}>
    <div style={{ position: 'absolute', inset: 0, display: 'flex', alignItems: 'center', justifyContent: 'center', perspective: 1400 }}>
      <div style={{ width: 520, height: 360, transform: 'rotateX(58deg) rotateZ(-38deg)', background: 'linear-gradient(var(--md-sc-high),var(--md-sc))', border: '1px solid var(--md-outline)', borderRadius: 6,
        backgroundImage: 'repeating-linear-gradient(0deg,transparent 0 25px, color-mix(in srgb, var(--md-outline) 34%, transparent) 25px 26px), repeating-linear-gradient(90deg,transparent 0 25px, color-mix(in srgb, var(--md-outline) 34%, transparent) 25px 26px)',
        boxShadow: '0 40px 60px rgba(0,0,0,.28)' }} />
    </div>
    {model && <div style={{ position: 'absolute', left: '50%', top: '44%', transform: 'translate(-50%,-50%)', width: 150, height: 164, filter: 'drop-shadow(0 20px 16px rgba(0,0,0,.35))', ...modelStyle }}>
      <div style={{ position: 'absolute', inset: 0, background: 'color-mix(in srgb, var(--md-primary) 84%, #fff)', clipPath: 'polygon(50% 2%, 98% 27%, 50% 52%, 2% 27%)' }} />
      <div style={{ position: 'absolute', inset: 0, background: 'color-mix(in srgb, var(--md-primary) 60%, #000 8%)', clipPath: 'polygon(2% 27%, 50% 52%, 50% 98%, 2% 73%)' }} />
      <div style={{ position: 'absolute', inset: 0, background: 'color-mix(in srgb, var(--md-primary) 42%, #000 22%)', clipPath: 'polygon(98% 27%, 98% 73%, 50% 98%, 50% 52%)' }} />
    </div>}
    {children}
  </div>;
};

MD.Checkbox = function Checkbox({ checked, label, onChange, style }) {
  return <button onClick={() => onChange && onChange(!checked)}
    style={{ display: 'inline-flex', alignItems: 'center', gap: 8, border: 'none', background: 'transparent', cursor: 'pointer',
      color: 'var(--md-on-surface)', fontSize: 12.5, fontWeight: 500, fontFamily: 'var(--md-font)', padding: '6px 8px', borderRadius: 8, ...style }}>
    <span data-icon style={{ fontSize: 20, color: checked ? 'var(--md-primary)' : 'var(--md-on-surface-variant)' }}>
      {checked ? 'check_box' : 'check_box_outline_blank'}
    </span>
    {label}
  </button>;
};

MD.NavItem = function NavItem({ icon, label, selected, onClick, style }) {
  const [hov, setHov] = useState(false);
  return <div onClick={onClick} onMouseEnter={() => setHov(true)} onMouseLeave={() => setHov(false)}
    style={{ display: 'flex', alignItems: 'center', gap: 10, height: 44, padding: '0 14px', borderRadius: 22, cursor: 'pointer',
      background: selected ? 'var(--md-secondary-container)' : hov ? 'var(--md-sc-high)' : 'transparent',
      color: selected ? 'var(--md-on-secondary-container)' : 'var(--md-on-surface-variant)',
      fontSize: 13.5, fontWeight: selected ? 600 : 400, fontFamily: 'var(--md-font)', ...style }}>
    <span data-icon style={{ fontSize: 20 }}>{icon}</span> {label}
  </div>;
};
