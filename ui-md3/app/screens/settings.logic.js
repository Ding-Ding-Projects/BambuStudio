registerScreen({
  id: 'settings',
  mixin: {
  // Plain text is the default. A regular expression is compiled only when the
  // search field reports that the user turned its .* toggle on, so a typed "."
  // with the toggle off matches literal full stops. A pattern that does not
  // compile falls back to the substring search instead of matching nothing.
  setMatch(text){
    const q=this.state.settingsQuery; if(!q) return true;
    const hay=String(text==null?'':text);
    if(!this.state.settingsRegex) return hay.toLowerCase().includes(q.toLowerCase());
    try{ return new RegExp(q, (typeof this.state.settingsFlags==='string'?this.state.settingsFlags:'i')).test(hay); }
    catch(e){ return hay.toLowerCase().includes(q.toLowerCase()); }
  },
  // Each field is tested on its own so an anchored pattern such as ^Theme$
  // still matches a row; joining the fields into one string would break it.
  setMatchAny(fields){
    for(let i=0;i<fields.length;i++){ if(this.setMatch(fields[i])) return true; }
    return false;
  },
  setEmptyText(){
    const q=this.state.settingsQuery||'';
    if(this.state.settingsRegex){
      try{ new RegExp(q, (typeof this.state.settingsFlags==='string'?this.state.settingsFlags:'i')); }
      catch(e){ return 'No settings match “'+q+'” — that pattern is not valid, so it was searched as plain text.'; }
      return 'No settings match regular expression “'+q+'”.';
    }
    return 'No settings match “'+q+'”.';
  },
  render_prefs(){
    const p=this.state.prefs;
    const defs=[
      {k:'autoArrange', label:'Auto-arrange on import', desc:'Automatically arrange new objects on the plate'},
      {k:'autoCommit', label:'Auto-commit every edit to Git', desc:'Save each undoable change to the project repository'},
      {k:'autoSaveToast', label:'Show “auto-saved” notifications', desc:'Pop a snackbar each time an edit is committed'},
      {k:'bundleRepo', label:'Bundle version history into .3mf', desc:'Include the local Git repo when saving a project'},
      {k:'hints', label:'Show daily tips', desc:'Display slicing tips on launch'},
      {k:'telemetry', label:'Share anonymous usage data', desc:'Help improve Bambu Studio'},
    ];
    // `on` is bound to aria-checked in the template: a switch has to announce
    // its state, not leave it to the knob position and a colour.
    return defs.filter(d=>this.setMatchAny(['General', d.label, d.desc])).map(d=>{ const on=p[d.k]; return { label:d.label, desc:d.desc, on:String(!!on),
      onClick:()=>this.setState(st=>({ prefs:{...st.prefs, [d.k]:!st.prefs[d.k]} })),
      track:on?'var(--md-primary)':'transparent', trackBorder:on?'var(--md-primary)':'var(--md-outline)',
      knob:on?'var(--md-on-primary)':'var(--md-outline)', knobX:on?'22px':'4px', knobSize:on?'16px':'12px' };});
  }
  },
  vals: function(){
    const langLabels=(window.BambuI18n&&window.BambuI18n.modes ? window.BambuI18n.modes : []).map(m=>m.label);
    const accentNames=this.render_accents().map(a=>a.name);
    const showTheme=this.setMatchAny(['Appearance','Theme','Light','Dark']);
    const showDensity=this.setMatchAny(['Appearance','Density','Comfortable','Compact']);
    const showLanguage=this.setMatchAny(['Appearance','Language','Language mode','Choose how interface text is shown'].concat(langLabels));
    const showAccent=this.setMatchAny(['Appearance','Accent color','Custom color'].concat(accentNames));
    const prefs=this.render_prefs();
    const showAppearance=showTheme||showDensity||showLanguage||showAccent;
    const showGeneral=prefs.length>0;
    return {
      isSettings: this.state.view === 'settings',
      prefs: prefs,
      // The mode travels with the query; without it the field cannot say
      // whether its .* toggle was on, and plain text stops being the default.
      setSettingsQuery:(v,mode)=>this.setState({ settingsQuery:v,
        settingsRegex:!!(mode&&mode.regex), settingsFlags:(mode&&typeof mode.flags==='string'?mode.flags:'i') }),
      setShowAppearance: showAppearance,
      setShowTheme: showTheme,
      setShowDensity: showDensity,
      setShowLanguage: showLanguage,
      setShowAccent: showAccent,
      setShowGeneral: showGeneral,
      setShowDivider: showAppearance&&showGeneral,
      setEmpty: !showAppearance&&!showGeneral,
      setEmptyText: this.setEmptyText()
    };
  }
});
