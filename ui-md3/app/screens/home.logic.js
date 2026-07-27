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
  }
  },
  vals: function(){ return {
    isHome: this.state.view === 'home',
    recent:this.render_recent()
  }; }
});
