import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import vm from 'node:vm';
import test from 'node:test';

const read = path => readFileSync(fileURLToPath(new URL('../../' + path, import.meta.url)), 'utf8').replace(/\r\n/g, '\n');
const vendor = read('src/imgui/imgui.cpp');
const wrapper = read('src/slic3r/GUI/ImGuiWrapper.cpp');

test('tooltip override targets the rounding selected by the vendored tooltip flags', () => {
    const begin = vendor.slice(vendor.indexOf('void ImGui::BeginTooltip()'), vendor.indexOf('void ImGui::BeginTooltip2'));
    assert.match(begin, /BeginTooltipEx\(ImGuiWindowFlags_None, ImGuiTooltipFlags_None\)/);
    const tooltip = vendor.slice(vendor.indexOf('void ImGui::BeginTooltipEx('), vendor.indexOf('void  ImGui::BeginTooltipEx2('));
    const flagExpression = tooltip.match(/ImGuiWindowFlags flags = ([^;]+);/)[1];
    assert.match(tooltip, /Begin\(window_name, NULL, flags \| extra_flags\)/);
    const selection = vendor.match(/window->WindowRounding = ([^;]+);/)[1];
    // Evaluate the actual C++ bitwise/conditional expression, also valid JavaScript,
    // with distinct symbolic values for each style member. Do not reproduce its decision.
    const names = [...new Set((flagExpression + selection).match(/ImGuiWindowFlags_\w+/g))];
    const context = Object.fromEntries(names.map((name, index) => [name, 2 ** index]));
    assert.match(flagExpression, /^[\w\s|]+$/);
    assert.match(selection, /^[\w\s.()&!?:]+$/);
    context.flags = vm.runInNewContext(flagExpression, context, {timeout: 100});
    context.style = {ChildRounding: 'ChildRounding', PopupRounding: 'PopupRounding', WindowRounding: 'WindowRounding'};
    const selected = vm.runInNewContext(selection, context, {timeout: 100});
    const body = wrapper.slice(wrapper.indexOf('void ImGuiWrapper::tooltip(const char *label, float wrap_width)'), wrapper.indexOf('ImGuiID ImGuiWrapper::tooltip_source_id()'));
    const beforeBegin = body.slice(0, body.indexOf('ImGui::BeginTooltip();'));
    const pushed = beforeBegin.match(/PushStyleVar\(ImGuiStyleVar_(\w+),/)[1];
    assert.equal(pushed, selected, 'the tooltip must override the style member actually selected by this vendored ImGui');
    assert.equal((body.match(/ImGui::PushStyleVar\(/g) || []).length, 1);
    assert.match(body, /ImGui::EndTooltip\(\);\s*ImGui::PopStyleVar\(\);/);
    assert.equal((body.match(/ImGui::PopStyleVar\(/g) || []).length, 1);
});
