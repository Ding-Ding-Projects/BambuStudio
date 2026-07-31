registerScreen({
  id: 'home',
  mixin: {
  render_recent(){
    return [
      {name:'3DBenchy_project', meta:'Today · X1 Carbon', image:'prepare.webp'},
      {name:'Enclosure_v3', meta:'Yesterday · P1S', image:'device.webp'},
      {name:'Gridfinity_bins', meta:'2 days ago · A1 mini', image:'preview.webp'},
      {name:'Phone_stand', meta:'Last week · X1C', image:'calibration.webp'},
    ];
  },
  // Plain text is the default; a regular expression is compiled only when the
  // search field says the user turned its .* toggle on. A pattern that does not
  // compile falls back to the substring match instead of emptying the grid.
  homeMatch(project){
    const q=this.state.homeQuery; if(!q) return true;
    const hay=project.name+' '+project.meta;
    if(!this.state.homeRegex) return hay.toLowerCase().includes(q.toLowerCase());
    try{ return new RegExp(q, (typeof this.state.homeFlags==='string'?this.state.homeFlags:'i')).test(hay); }
    catch(e){ return hay.toLowerCase().includes(q.toLowerCase()); }
  }
  },
  vals: function(){
    const q=this.state.homeQuery||'';
    const recent=this.render_recent().filter(r=>this.homeMatch(r));
    return {
    isHome: this.state.view === 'home',
    recent,
    homeQuery:q,
    homeEmpty: q!=='' && recent.length===0,
    setHomeQuery:(v,mode)=>this.setState({ homeQuery:v,
      homeRegex:!!(mode&&mode.regex), homeFlags:(mode&&typeof mode.flags==='string'?mode.flags:'i') })
  }; }
});
