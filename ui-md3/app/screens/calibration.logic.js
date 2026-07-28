registerScreen({
  id: 'calibration',
  mixin: {
    render_cali(){
      // Plain text is the default; a regular expression is compiled only when
      // the search field says the user turned its .* toggle on. A typed "."
      // with that toggle off matches literal full stops, nothing else.
      const q = this.state.caliQuery || '';
      const caliMatch = (hay)=>{
        if(!q) return true;
        const s = String(hay);
        if(!this.state.caliRegex) return s.toLowerCase().includes(q.toLowerCase());
        try{ return new RegExp(q, this.state.caliFlags||'i').test(s); }
        catch(e){ return s.toLowerCase().includes(q.toLowerCase()); }
      };
      return [
        {title:'Flow Dynamics', desc:'Calibrate pressure advance for sharp corners', icon:'water_drop'},
        {title:'Flow Rate', desc:'Tune extrusion multiplier for accurate walls', icon:'opacity'},
        {title:'Max Volumetric Speed', desc:'Find the fastest reliable flow for a filament', icon:'speed'},
        {title:'Temperature Tower', desc:'Find the ideal nozzle temperature', icon:'thermostat'},
        {title:'Retraction Test', desc:'Reduce stringing and oozing', icon:'undo'},
        {title:'Vertical Fine Tuning', desc:'Correct Z-offset and first layer', icon:'height'},
      ].filter(c=>caliMatch(c.title)||caliMatch(c.desc))
       .map(c=>({ ...c, onClick:()=>this.notify('Starting '+c.title+' calibration…', {icon:c.icon}) }));
    }
  },
  vals: function(){
    const caliCards = this.render_cali();
    return {
    isCalibration:this.state.view === 'calibration',
    caliCards:caliCards,
    caliQuery:this.state.caliQuery || '',
    caliEmpty:caliCards.length === 0,
    setCaliQuery:(v,mode)=>this.setState({ caliQuery:v,
      caliRegex:!!(mode&&mode.regex), caliFlags:(mode&&mode.flags||'i').replace('g','')||'i' })
  }; }
});
