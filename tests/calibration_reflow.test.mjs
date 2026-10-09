import {readFileSync,writeFileSync,mkdtempSync} from 'node:fs';
import {execFileSync,spawnSync} from 'node:child_process';
import {tmpdir} from 'node:os';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {compileFixture} from './native_fixture_compiler.mjs';
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'..');
const revision=process.argv.find(x=>x.startsWith('--baseline='))?.slice(11);
const output=mkdtempSync(path.join(tmpdir(),'bambustudio-calibration-reflow-'));
function method(file,name) {
 const text=revision?execFileSync('git',['show',revision+':src/slic3r/GUI/'+file],{cwd:root,encoding:'utf8'}):readFileSync(path.join(root,'src/slic3r/GUI',file),'utf8');
 const start=text.indexOf('void '+name+'()');if(start<0)throw new Error('Production method missing');
 const tokens=/\/\*[\s\S]*?\*\/|\/\/[^\n]*|"(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*'|[{}]/g;tokens.lastIndex=text.indexOf('{',start);let depth=0;
 for(let match;(match=tokens.exec(text));){if(match[0]==='{')++depth;if(match[0]==='}'&&--depth===0)return text.slice(start,match.index+1);}
 throw new Error('Unterminated production method');
}
const methods=method('CalibrationWizardPage.cpp','CalibrationWizardPage::queue_instruction_reflow')+'\n'+method('CalibrationWizardPresetPage.cpp','CaliPresetTipsPanel::queue_tips_reflow');
const cpp=path.join(output,'fixture.cpp'),exe=path.join(output,'fixture.exe');
writeFileSync(cpp,readFileSync(path.join(root,'tests/calibration_reflow_fixture.cpp'),'utf8').replace('// PRODUCTION_METHODS',methods));
console.log('Actual adapter source: '+(revision??'current working source'));
console.log('Task-owned output: '+output);
const compiled=compileFixture({source:cpp,exe,cwd:output,include:root});
if(compiled.error)throw compiled.error;if(compiled.status!==0)process.exit(compiled.status??1);
const result=spawnSync(exe,[],{cwd:output,stdio:'inherit'});if(result.error)throw result.error;process.exit(result.status??1);
