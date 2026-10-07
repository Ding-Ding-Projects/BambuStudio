import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {createHash} from 'node:crypto';
import {fileURLToPath} from 'node:url';
import test from 'node:test';
const read = file => readFileSync(fileURLToPath(new URL('../src/slic3r/GUI/Gizmos/' + file, import.meta.url)), 'utf8').replace(/\r\n/g, '\n');
const source = read('GLGizmoBase.cpp');
function extract(s,name){const start=s.indexOf(name);if(start<0)throw Error(name);let open=s.indexOf('{',start),d=1,i=open+1;for(;d;i++){if(s[i]==='{')d++;else if(s[i]==='}')d--;}return {start,end:i,text:s.slice(start,i)};}
const begin = extract(source, 'bool GLGizmoBase::GizmoImguiBegin(').text;
const end = extract(source, 'void GLGizmoBase::GizmoImguiEnd(').text;
test('shared inspector framing preserves caller flags, return value and balanced style lifetime', () => {
 assert.match(begin, /const bool visible = m_imgui->begin\(name, flags\);/);
 assert.match(begin, /return visible;/);
 assert.match(end, /last_input_window_width = ImGui::GetWindowWidth\(\);\s*m_imgui->end\(\);\s*ImGui::PopStyleVar\(\);\s*ImGui::PopStyleColor\(2\);/);
 assert.equal((begin.match(/ImGui::PushStyleColor\(/g)||[]).length,2);
 assert.equal((begin.match(/ImGui::PushStyleVar\(/g)||[]).length,1);
 assert.doesNotMatch(begin + end, /ImGui::(?:SetCursor\w*|SetNextWindow\w*|SetWindow\w*|ItemSize|Button|InvisibleButton|Text|Dummy)\s*\(|set_requires_extra_frame|request_extra_frame/);
});
test('all twelve existing tool panels retain the paired framing route', () => {
 for(const name of ['AdvancedCut','Assembly','BrimEars','FdmSupports','Flatten','FuzzySkin','Measure','MmuSegmentation','MeshBoolean','Seam','SVG','Text']) {
  const file=read('GLGizmo'+name+'.cpp');
  assert.equal((file.match(/GizmoImguiBegin\(/g)||[]).length,1,name);
  assert.equal((file.match(/GizmoImguiEnd\(/g)||[]).length,1,name);
 }
});
test('base tool behavior outside the two framing wrappers remains unchanged', () => {
 let preserved=source.replace('#include "slic3r/GUI/Widgets/MD3Tokens.hpp"\n','');
 for(const signature of ['bool GLGizmoBase::GizmoImguiBegin(', 'void GLGizmoBase::GizmoImguiEnd(']) {
  const body=extract(preserved,signature);preserved=preserved.slice(0,body.start)+signature+preserved.slice(body.end);
 }
 assert.equal(createHash('sha256').update(preserved).digest('hex'),'94c451c9a0e584d3e49fb2c0ff9bd50d4ca000d389ca0377f3312692180ad39e');
});
