registerScreen({
  id: 'filament',
  mixin: {},
  vals: function(){
    // Plain text is the default; a regular expression is compiled only when the
    // search field says the user turned its .* toggle on, and a pattern that
    // does not compile falls back to the substring match.
    const fs = this.state.filamentSearch || { q:'', regex:false, flags:'i' };
    const q = fs.q || '';
    const re = (function(){
      if(!q || !fs.regex) return null;
      try{ return new RegExp(q, typeof fs.flags==='string' ? fs.flags : 'i'); }
      catch(e){ return null; }
    })();
    const needle = q.toLowerCase();
    const hit = (f)=>[f.name, f.vendor, f.type, f.nozzle, f.bed]
      .some(v=>re ? re.test(String(v)) : String(v).toLowerCase().includes(needle));
    const rows = q ? this.render_filRows().filter(hit) : this.render_filRows();
    return {
      isFilament: this.state.view === 'filament',
      filRows: rows,
      filEmpty: q !== '' && rows.length === 0,
      filEmptyNote: 'Searched “' + q + '” · ' + (re ? 'regular expression' : 'plain text'),
      setFilQuery:(v,mode)=>this.setState({filamentSearch:{ q:v, ...this.searchMode(mode) }})
    };
  }
});
