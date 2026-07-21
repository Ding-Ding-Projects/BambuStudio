import React from 'react';
const TOKENS = [
  { t: '.', l: 'any' }, { t: '\\d', l: 'digit' }, { t: '\\w', l: 'word' }, { t: '\\s', l: 'space' },
  { t: '^', l: 'start' }, { t: '$', l: 'end' }, { t: '\\b', l: 'bound' }, { t: '+', l: '1 or +' },
  { t: '*', l: '0 or +' }, { t: '?', l: 'optional' }, { t: '()', l: 'group' }, { t: '[]', l: 'set' },
  { t: '|', l: 'or' }, { t: '\\.', l: 'literal .' }, { t: '{2}', l: 'repeat' }, { t: '(?:)', l: 'non-cap' },
];
export function SearchField({ placeholder = 'Search', onQuery, style }) {
  const [s, set] = React.useState({ value: '', pattern: '', open: false, regex: false, flags: { i: true, w: false, m: false, s: false } });
  const patch = p => set(st => ({ ...st, ...p }));
  const emit = v => { if (typeof onQuery === 'function') onQuery(v); };
  const flagStr = 'g' + (s.flags.i ? 'i' : '') + (s.flags.m ? 'm' : '') + (s.flags.s ? 's' : '');
  let ok = true; try { new RegExp(s.pattern, flagStr); } catch (e) { ok = false; }
  const valid = s.pattern === '' || ok;
  const flagChip = (k, label) => {
    const on = s.flags[k];
    return (
      <button key={k} onClick={() => patch({ flags: { ...s.flags, [k]: !on } })}
        style={{ height: 30, padding: '0 12px', borderRadius: 15, cursor: 'pointer', fontSize: 11.5, fontWeight: 500,
          background: on ? 'var(--md-primary)' : 'transparent', color: on ? 'var(--md-on-primary)' : 'var(--md-on-surface-variant)',
          border: on ? '1px solid var(--md-primary)' : '1px solid var(--md-outline)' }}>{label}</button>
    );
  };
  return (
    <div style={{ position: 'relative', width: '100%', fontFamily: 'var(--md-font)', ...style }}>
      <div style={{ display: 'flex', alignItems: 'center', gap: 4, height: 40, padding: '0 5px 0 14px', background: 'var(--md-sc-highest)',
        border: '1px solid ' + (s.open ? 'var(--md-primary)' : 'var(--md-outline)'), borderRadius: 22 }}>
        <span data-icon style={{ fontSize: 20, color: 'var(--md-on-surface-variant)' }}>search</span>
        <input value={s.value} placeholder={placeholder}
          onChange={e => { patch({ value: e.target.value, pattern: s.regex ? e.target.value : s.pattern }); emit(e.target.value); }}
          style={{ flex: 1, minWidth: 0, border: 'none', background: 'transparent', outline: 'none', color: 'var(--md-on-surface)',
            fontSize: 13.5, fontFamily: s.regex ? 'var(--md-font-mono)' : 'inherit' }} />
        {!!s.value && (
          <button title="Clear" onClick={() => { patch({ value: '', pattern: '' }); emit(''); }}
            style={{ width: 30, height: 30, border: 'none', background: 'transparent', color: 'var(--md-on-surface-variant)', borderRadius: '50%', cursor: 'pointer' }}>
            <span data-icon style={{ fontSize: 18 }}>close</span>
          </button>
        )}
        <button title="Use regular expression" onClick={() => patch({ regex: !s.regex })}
          style={{ minWidth: 32, height: 30, padding: '0 4px', border: 'none', borderRadius: 8, cursor: 'pointer',
            fontFamily: 'var(--md-font-mono)', fontSize: 12.5, fontWeight: 700,
            background: s.regex ? 'var(--md-primary)' : 'var(--md-sc-low)', color: s.regex ? 'var(--md-on-primary)' : 'var(--md-on-surface-variant)' }}>.*</button>
        <button title="Regex builder" onClick={() => patch({ open: !s.open })}
          style={{ width: 32, height: 30, border: 'none', borderRadius: 8, cursor: 'pointer', display: 'flex', alignItems: 'center', justifyContent: 'center',
            background: s.open ? 'var(--md-secondary-container)' : 'var(--md-sc-low)', color: s.open ? 'var(--md-on-secondary-container)' : 'var(--md-on-surface-variant)' }}>
          <span data-icon style={{ fontSize: 18 }}>tune</span>
        </button>
      </div>
      {s.open && (
        <div style={{ position: 'absolute', top: 47, right: 0, left: 0, zIndex: 80, background: 'var(--md-sc)', border: '1px solid var(--md-outline-variant)',
          borderRadius: 18, boxShadow: '0 12px 34px var(--md-shadow)', padding: 14, display: 'flex', flexDirection: 'column', gap: 12, animation: 'mdfade .16s ease' }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: 8 }}>
            <span data-icon style={{ fontSize: 18, color: 'var(--md-primary)' }}>regular_expression</span>
            <span style={{ fontWeight: 600, fontSize: 13 }}>Regex builder</span>
            <span style={{ flex: 1 }} />
            <span style={{ fontSize: 11, fontWeight: 600, color: valid ? 'var(--md-primary)' : 'var(--md-error)' }}>{s.pattern === '' ? '' : (valid ? 'valid' : 'invalid')}</span>
          </div>
          <div style={{ display: 'flex', alignItems: 'center', gap: 6, background: 'var(--md-sc-lowest)', border: '1px solid var(--md-outline-variant)',
            borderRadius: 10, padding: '9px 10px', fontFamily: 'var(--md-font-mono)', fontSize: 13 }}>
            <span style={{ color: 'var(--md-on-surface-variant)' }}>/</span>
            <input value={s.pattern} placeholder="pattern"
              onChange={e => { patch({ pattern: e.target.value, value: e.target.value, regex: true }); emit(e.target.value); }}
              style={{ flex: 1, minWidth: 0, border: 'none', background: 'transparent', outline: 'none', color: 'var(--md-on-surface)', fontFamily: 'inherit', fontSize: 13 }} />
            <span style={{ color: 'var(--md-on-surface-variant)' }}>/{flagStr}</span>
          </div>
          <div style={{ display: 'flex', gap: 6, flexWrap: 'wrap' }}>
            {flagChip('i', 'Ignore case')}{flagChip('w', 'Whole word')}{flagChip('m', 'Multiline')}{flagChip('s', 'Dotall')}
          </div>
          <div style={{ fontSize: 10.5, letterSpacing: .5, textTransform: 'uppercase', color: 'var(--md-on-surface-variant)' }}>Insert token</div>
          <div style={{ display: 'grid', gridTemplateColumns: 'repeat(6,1fr)', gap: 6 }}>
            {TOKENS.map(td => (
              <button key={td.t} title={td.l} onClick={() => { patch({ pattern: s.pattern + td.t, regex: true }); emit(s.pattern + td.t); }}
                style={{ height: 38, display: 'flex', flexDirection: 'column', alignItems: 'center', justifyContent: 'center', gap: 1,
                  border: '1px solid var(--md-outline-variant)', background: 'var(--md-sc-lowest)', borderRadius: 9, cursor: 'pointer' }}>
                <span style={{ fontFamily: 'var(--md-font-mono)', fontSize: 12.5, fontWeight: 600, color: 'var(--md-on-surface)' }}>{td.t}</span>
                <span style={{ fontSize: 8.5, color: 'var(--md-on-surface-variant)' }}>{td.l}</span>
              </button>
            ))}
          </div>
          <div style={{ display: 'flex', gap: 8 }}>
            <button onClick={() => patch({ pattern: '' })}
              style={{ height: 38, padding: '0 16px', border: '1px solid var(--md-outline)', background: 'transparent', color: 'var(--md-on-surface)', borderRadius: 19, cursor: 'pointer', fontSize: 12.5, fontWeight: 500 }}>Clear</button>
            <button onClick={() => { patch({ open: false, value: s.pattern, regex: true }); emit(s.pattern); }}
              style={{ flex: 1, height: 38, border: 'none', background: 'var(--md-primary)', color: 'var(--md-on-primary)', borderRadius: 19, cursor: 'pointer', fontSize: 12.5, fontWeight: 600 }}>Apply expression</button>
          </div>
        </div>
      )}
    </div>
  );
}
