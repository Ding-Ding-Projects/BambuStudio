registerScreen({
  id: 'project',
  mixin: {
  render_projectCats(){
    const cur=this.state.projectCat||'Model pictures';
    return [
      {label:'Model pictures', icon:'image', count:3},{label:'Bill of materials', icon:'receipt_long', count:1},
      {label:'Assembly guide', icon:'menu_book', count:2},{label:'Others', icon:'folder', count:4},
    ].map(c=>{const on=cur===c.label;return{...c,onClick:()=>this.setState({projectCat:c.label}),
      bg:on?'var(--md-secondary-container)':'transparent', fg:on?'var(--md-on-secondary-container)':'var(--md-on-surface-variant)'};});
  },
  render_projectFiles(){
    return [
      {name:'hull_front.png', type:'PNG', icon:'image'},{name:'hull_rear.png', type:'PNG', icon:'image'},
      {name:'cover.png', type:'PNG', icon:'image'},{name:'assembly_1.pdf', type:'PDF', icon:'picture_as_pdf'},
      {name:'assembly_2.pdf', type:'PDF', icon:'picture_as_pdf'},{name:'notes.txt', type:'TXT', icon:'description'},
    ];
  },
  // The mode rides along with the query so the field's .* toggle is the only
  // thing that turns a typed string into a pattern.
  setProjectQuery(v, mode){
    this.setState({projectSearch:{ query:v,
      regex:!!(mode&&mode.regex), flags:(mode&&typeof mode.flags==='string'?mode.flags:'i') }});
  },
  // Plain text is the default; a regular expression is compiled only when the
  // search field says the user turned its .* toggle on.
  projectFileMatch(name){
    const ps=this.state.projectSearch||{}; const q=ps.query; if(!q) return true;
    if(!ps.regex) return name.toLowerCase().includes(q.toLowerCase());
    try{ return new RegExp(q, ps.flags||'i').test(name); }
    catch(e){ return name.toLowerCase().includes(q.toLowerCase()); }
  }
  },
  vals: function(){
    const files = this.render_projectFiles().filter(f=>this.projectFileMatch(f.name));
    const ps = this.state.projectSearch||{};
    return {
    isProject: this.state.view === 'project',
    projectCats: this.render_projectCats(),
    projectFiles: files,
    projectQuery: ps.query||'',
    projectEmpty: files.length === 0,
    setProjectQuery: (v, mode)=>this.setProjectQuery(v, mode)
  }; }
});
