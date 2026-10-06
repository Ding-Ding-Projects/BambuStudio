import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

// Deterministic source-native boards, never production screenshot evidence.
const directory = path.dirname(fileURLToPath(import.meta.url));
const manifest = JSON.parse(fs.readFileSync(path.join(directory, 'manifest.json'), 'utf8'));
const check = process.argv.includes('--check');
const escape = value => String(value).replaceAll('&', '&amp;').replaceAll('<', '&lt;').replaceAll('>', '&gt;').replaceAll('"', '&quot;');
const outputs = [];
for (const surface of manifest.surfaces) for (const theme of manifest.themes) {
  const t = { ...manifest.tokens[theme] };
  const dark = theme === 'dark';
  if (surface.id === 'preview') Object.assign(t, dark ? { primary:'#ad98ff', accent:'#563bc2', onAccent:'#e8ddff' } : { primary:'#7050e8', accent:'#e8ddff', onAccent:'#23005c' });
  if (['print','monitor','printer-web','farm','ink-web'].includes(surface.id)) Object.assign(t, dark ? { primary:'#5eead4', accent:'#005047', onAccent:'#83f5e3' } : { primary:'#0f766e', accent:'#9cf2e7', onAccent:'#00201d' });
  const parts = [];
  const rect = (x,y,w,h,fill,r=0,stroke='none') => parts.push(`<rect x="${x}" y="${y}" width="${w}" height="${h}" rx="${r}" fill="${fill}" stroke="${stroke}"/>`);
  const line = (x1,y1,x2,y2,color=t.edge) => parts.push(`<path d="M${x1} ${y1}L${x2} ${y2}" fill="none" stroke="${color}"/>`);
  const text = (x,y,label,size=14,color=t.text,weight=400) => parts.push(`<text x="${x}" y="${y}" font-size="${size}" fill="${color}" font-weight="${weight}">${escape(label)}</text>`);
  const card = (x,y,w,h,label) => { rect(x,y,w,h,t.lowest,16,t.edge); text(x+20,y+31,label,16,t.text,650); };
  const field = (x,y,w,label,value) => { text(x,y,label,12.5,t.secondary); rect(x,y+10,w,40,t.surface,10,t.outline); text(x+12,y+35,value); };
  const pill = (x,y,w,label,selected=false) => { rect(x,y,w,36,selected?t.accent:t.low,18); text(x+14,y+23,label,13,selected?t.onAccent:t.secondary,selected?600:400); };
  const button = (x,y,w,label) => { rect(x,y,w,40,t.primary,20); text(x+18,y+25,label,14,t.onPrimary,600); };
  rect(0,0,1200,800,t.surface);
  rect(0,0,1200,46,t.lowest);
  rect(18,12,22,22,t.primary,7); text(49,29,'Bambu Studio',15,t.text,650);
  text(206,28,'Studio Atlas / source reference',12,t.secondary);
  text(1008,28,'Version unavailable',11.5,t.secondary);
  text(1160,28,'×',20,t.secondary);
  line(0,46,1200,46);
  rect(12,53,498,40,t.low,16,t.edge);
  ['Prepare','Preview','Print','Monitor'].forEach((name,i) => {
    const selected = surface.id === name.toLowerCase();
    const x=18+i*122;
    if(selected) rect(x,56,116,34,t.accent,17);
    text(x+16,78,name,14,selected?t.onAccent:t.secondary,selected?650:400);
    if(selected) rect(x+48,86,20,3,t.primary,1.5);
  });
  text(540,79,'Home',13,t.secondary); text(606,79,'Project',13,t.secondary); text(684,79,'Multi-device',13,t.secondary);
  text(796,79,'Filament',13,t.secondary); text(887,79,'Calibration',13,t.secondary); text(996,79,'Preferences',13,t.secondary); text(1114,79,'More',13,t.secondary);
  text(24,134,surface.title,24,t.text,700);
  text(24,157,'Static design reference. Illustrative controls are not interactive.',12.5,t.secondary);
  pill(982,114,194,theme === 'light' ? 'Light / comfortable' : 'Dark / comfortable');
  const canvas = surface.layout === 'canvas';
  if(canvas) {
    card(24,180,760,510,surface.id==='preview'?'Toolpath canvas':surface.id==='model-creator'?'Accepted model preview':'Build plate');
    rect(42,230,724,406,t.low,12);
    for(let i=0;i<9;i++) { line(125+i*57,300,75+i*69,560); line(126,300+i*29,680,300+i*29); }
    parts.push(`<path d="M275 415L405 348L522 410L392 478Z" fill="${t.accent}" stroke="${t.primary}"/><path d="M275 415L275 490L392 556L392 478Z" fill="${t.container}" stroke="${t.primary}"/><path d="M392 478L522 410L522 488L392 556Z" fill="${t.high}" stroke="${t.primary}"/>`);
    text(62,615,'Illustrative geometry',12,t.secondary);
    ['Select','Move','Scale','Rotate'].forEach((label,i)=>pill(62+i*112,647,102,label));
    card(800,180,376,510,surface.sections[0]);
    surface.sections.slice(1,4).forEach((label,i)=>field(820,256+i*105,336,label,i===0?'Use existing selection':'Current project value'));
    text(820,615,'Selection and actions stay unchanged',12,t.secondary);
    button(820,634,336,surface.sections.at(-1));
  } else if(surface.layout === 'print') {
    card(24,180,724,146,'1  Plate readiness');
    pill(44,230,104,'Review',true); text(164,251,'Current plate and slice generation',15,t.text,600);
    text(44,297,'A stale or missing slice keeps print setup unavailable.',14,t.secondary);
    card(24,342,724,202,'2  Destination and mapping');
    field(44,408,320,'Printer','Existing printer selection'); field(388,408,340,'Material mapping','Existing AMS / spool mapping');
    text(44,515,'Compatibility and availability remain authoritative.',14,t.secondary);
    card(24,560,724,130,'3  Continue deliberately');
    text(44,615,'The existing setup dialog keeps final print confirmation.',14,t.secondary);
    text(44,650,'Opening this workspace never submits a job.',14,t.secondary);
    card(764,180,412,510,'Print summary');
    ['Plate','Printer','Material','Estimated duration'].forEach((label,i)=>{ text(784,254+i*63,label,13,t.secondary); text(784,279+i*63,'Available from current project',14); });
    button(784,622,372,'Review print setup');
  } else if(surface.layout === 'monitor') {
    card(24,180,736,510,'Live camera'); rect(44,234,696,344,t.low,12);
    text(235,392,'Camera state belongs here',20,t.text,600); text(217,421,'No captured printer image is used in this reference.',12,t.secondary);
    pill(44,602,124,'Connection'); pill(180,602,104,'Play / stop'); text(44,665,'No physical action is represented as executed.',13,t.secondary);
    card(776,180,400,510,'Printer activity');
    surface.sections.filter(x=>x!=='Live camera').slice(0,4).forEach((label,i)=>{ text(796,253+i*89,label,15,t.text,600); rect(796,270+i*89,360,8,t.low,4); text(796,301+i*89,'Actual telemetry or explicit unavailable state',12,t.secondary); });
  } else if(surface.layout === 'overlay') {
    rect(24,180,1152,510,t.low,16,t.edge);
    card(224,202,752,466,surface.title);
    field(248,274,704,'Local search','Plain text search with adjacent pattern builder');
    surface.sections.slice(1).forEach((label,i)=> { rect(248,343+i*59,704,47,i===0?t.accent:t.surface,10); text(265,372+i*59,label,14,i===0?t.onAccent:t.text); });
    text(248,643,'Escape closes and returns focus to the originating control.',12,t.secondary);
  } else if(surface.layout === 'steps') {
    card(24,180,292,510,'Steps');
    surface.sections.forEach((label,i)=>{ pill(40,232+i*76,260,`${i+1}  ${label}`,i===0); });
    card(332,180,844,510,surface.sections[0]);
    text(356,265,'Existing inputs, validation and decisions remain available.',16);
    field(356,322,792,surface.sections[1]||'Selection','Choose from the existing values');
    field(356,426,792,'Supporting explanation','Actual defaults and validation, never a decorative field');
    pill(356,622,116,'Cancel'); button(908,622,240,'Continue');
  } else if(surface.layout === 'reader') {
    card(24,180,288,510,surface.sections[0]); field(44,253,248,'Local search','Search with builder');
    surface.sections.slice(1).forEach((label,i)=>pill(44,328+i*65,248,label,i===0));
    card(328,180,848,510,surface.sections[1]||'Article');
    text(352,265,'A readable article and its real navigation',22,t.text,650);
    text(352,309,'Existing content, headings, links and exports stay accessible.',15,t.secondary);
    for(let i=0;i<6;i++) rect(352,349+i*30,i===5?480:778,8,t.low,4);
    text(352,590,'Illustrative typesetting; no article content is fabricated.',13,t.secondary);
    pill(352,622,160,'Related features'); pill(524,622,148,'Copy / export');
  } else {
    card(24,180,288,510,surface.layout==='settings'?'Sections':'Browse');
    surface.sections.forEach((label,i)=>pill(40,232+i*72,256,label,i===0));
    card(328,180,848,510,surface.sections[0]);
    field(352,255,800,'Local search','Plain text search with adjacent pattern builder');
    surface.sections.slice(1).forEach((label,i)=>{ rect(352,333+i*74,800,60,t.surface,12,t.edge); text(368,357+i*74,label,15,t.text,600); text(368,380+i*74,'Real values, actions, state and explanation',12,t.secondary); });
    text(352,665,'Lists, fields and actions retain their existing behavior.',12,t.secondary);
  }
  line(24,710,1176,710);
  text(24,735,`Reference: ${surface.id}/${theme}  |  1200 × 800 DIP  |  No runtime verification`,12,t.secondary);
  text(24,759,`State inventory: ${surface.states.length} explicit states in manifest.json; each still needs real interaction and capture proof.`,12,t.secondary);
  text(24,782,'Material roles • Opaque surfaces • Local search and pattern builder • Visible focus • Reduced motion',11.5,t.secondary);
  const svg=`<svg xmlns="http://www.w3.org/2000/svg" width="1200" height="800" viewBox="0 0 1200 800" role="img" aria-labelledby="title desc"><title id="title">${escape(surface.title)}: ${theme} static design reference</title><desc id="desc">Source-native Studio Atlas layout specification. Not a screenshot or a working application. ${escape(surface.preserve)}</desc><g font-family="Roboto, Arial, sans-serif">${parts.join('')}</g></svg>\n`;
  const relative=`references/${surface.id}-${theme}.svg`;
  const destination=path.join(directory,relative);
  if(check) {
    if(!fs.existsSync(destination)||fs.readFileSync(destination,'utf8').replace(/\r\n/g,'\n')!==svg) throw new Error(`Stale or missing reference: ${relative}`);
  } else { fs.mkdirSync(path.dirname(destination),{recursive:true}); fs.writeFileSync(destination,svg); }
  outputs.push(relative);
}
console.log(`${check?'Verified':'Generated'} ${outputs.length} deterministic static reference boards; no runtime evidence produced.`);
