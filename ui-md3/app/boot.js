(function bootBambuStudio(){
  var shell = document.querySelector('template[data-shell]');
  var assembledString = shell.innerHTML;

  document.querySelectorAll('template[data-screen]').forEach(function(screen){
    var marker = '<!--SCREEN:' + screen.getAttribute('data-screen') + '-->';
    assembledString = assembledString.split(marker).join(screen.innerHTML);
  });

  assembledString = assembledString.replace(/<!--SCREEN:[a-z0-9_-]+-->/gi, '');

  DC.register('SearchField', document.querySelector('template[data-component="SearchField"]'), SearchField);
  DC.register('main', assembledString, Main);

  var qp = Object.fromEntries(new URLSearchParams(window.location.search));
  var language = window.BambuI18n
    ? window.BambuI18n.initialize({search:window.location.search})
    : 'en';
  // Query overrides are validated against the design's own enums (the
  // data-props of design-source/Bambu Studio.dc.html) and fall back to its
  // defaults. An unknown view used to render an empty body and an unknown
  // theme dropped every colour token.
  var pick = function(value, options, fallback){
    return options.indexOf(value) === -1 ? fallback : value;
  };
  var accent = /^#?([0-9a-f]{3}|[0-9a-f]{6})$/i.test(qp.accent || '')
    ? (qp.accent.charAt(0) === '#' ? qp.accent : '#' + qp.accent)
    : '#22c55e';
  DC.mount('main', document.getElementById('app'), {
    theme:pick(qp.theme, ['light','dark'], 'dark'),
    density:pick(qp.density, ['comfortable','compact'], 'comfortable'),
    accent:accent,
    view:pick(qp.view, ['home','prepare','preview','device','multi','project','calibration','filament','settings'], 'prepare'),
    language:language
  });
})();
