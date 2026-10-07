import {test} from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {execFileSync} from 'node:child_process';
const path='src/slic3r/GUI/ConfigWizard.cpp';
const baseline='54810146717bbf4d531a17f4b6c471bd3300d190';
const current=readFileSync(path,'utf8').replaceAll('\r\n','\n');
const before=execFileSync('git',['show',baseline+':'+path],{encoding:'utf8'}).replaceAll('\r\n','\n');
function parts(s){const a=s.indexOf('void ConfigWizardIndex::on_paint('),b=s.indexOf('void ConfigWizardIndex::on_mouse_move',a);assert.ok(a>=0&&b>a);return [s.slice(0,a),s.slice(a,b),s.slice(b)];}
test('only the setup index paint method changes',()=>{const a=parts(before),b=parts(current);assert.equal(a[0],b[0]);assert.equal(a[2],b[2]);});
test('step labels and hitbox advance retain actual item geometry',()=>{const paint=parts(current)[1];assert.match(paint,/const int yinc = item_height\(\)/);assert.match(paint,/i < items.size\(\)/);assert.match(paint,/GetTextExtent\(item.label\)/);assert.match(paint,/yinc - text_size.y/);assert.match(paint,/y \+= yinc/);assert.match(paint,/index_width = std::max\(index_width, \(int\)x \+ text_size.x \+ FromDIP/);});
test('state paint uses semantic foreground and measured shapes',()=>{const paint=parts(current)[1];for(const role of ['SurfaceContainerLow','PrimaryContainer','SurfaceContainerHigh','OnPrimaryContainer','OnSurface','Outline'])assert.ok(paint.includes('MD3::Role::'+role),role);assert.match(paint,/dc.DrawRoundedRectangle/);assert.match(paint,/dc.DrawCircle/);assert.doesNotMatch(paint,/DrawBitmap\(bullet_/);assert.match(paint,/dc.DrawBitmap\(bg.bmp\(\)/);});
test('preservation check rejects changed navigation routing',()=>{assert.notEqual(parts(current)[2],parts(current.replace('pos.y / item_height()','pos.y / 1'))[2]);});
