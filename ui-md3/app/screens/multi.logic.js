registerScreen({
  id: 'multi',
  mixin: {
    render_devices(){
      return [
        {name:'X1 Carbon', model:'Bambu Lab X1C', status:'Printing', pct:68, dot:'var(--md-primary)'},
        {name:'P1S — Studio', model:'Bambu Lab P1S', status:'Idle', pct:0, dot:'var(--md-outline)'},
        {name:'A1 mini', model:'Bambu Lab A1 mini', status:'Printing', pct:12, dot:'var(--md-primary)'},
        {name:'X1E — Lab', model:'Bambu Lab X1E', status:'Offline', pct:0, dot:'var(--md-error)'},
      ];
    },
    // Plain text is the default; a regular expression is compiled only when the
    // search field says the user turned its .* toggle on. An uncompilable
    // pattern falls back to substring so the farm never blanks out mid-keystroke.
    multiMatch(d){
      const q=this.state.multiQuery; if(!q) return true;
      const fields=[d.name, d.model, d.status];
      const plain=()=>fields.some(f=>String(f).toLowerCase().includes(q.toLowerCase()));
      if(!this.state.multiRegex) return plain();
      try{
        const re=new RegExp(q, this.state.multiFlags||'i');
        return fields.some(f=>re.test(String(f)));
      }catch(e){ return plain(); }
    },
    setMultiQuery(v, mode){
      this.setState({ multiQuery:v||'',
        multiRegex:!!(mode&&mode.regex), multiFlags:(mode&&mode.flags||'i').replace('g','')||'i' });
    }
  },
  vals: function(){
    const devices = this.render_devices().filter(d=>this.multiMatch(d));
    return {
      isMulti: this.state.view === 'multi',
      devices: devices,
      multiQuery: this.state.multiQuery || '',
      setMultiQuery: (v,mode)=>this.setMultiQuery(v,mode),
      multiEmpty: devices.length === 0
    };
  }
});
