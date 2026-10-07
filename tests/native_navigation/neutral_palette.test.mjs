import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import fs from 'node:fs';
import { fileURLToPath } from 'node:url';
import path from 'node:path';
import test from 'node:test';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const read = file => fs.readFileSync(path.join(root, file), 'utf8').replaceAll('\r\n', '\n');
const tokens = read('src/slic3r/GUI/Widgets/MD3Tokens.hpp');
const stateHeader = read('src/slic3r/GUI/Widgets/StateColor.hpp');
const stateSource = read('src/slic3r/GUI/Widgets/StateColor.cpp');
const uncomments = source => source.replace(/\/\*[\s\S]*?\*\//g, '').replace(/\/\/[^\n]*/g, '');
const block = (source, start, end) => {
    const begin = source.indexOf(start);
    assert.notEqual(begin, -1, start);
    const finish = source.indexOf(end, begin + start.length);
    assert.notEqual(finish, -1, end);
    return source.slice(begin + start.length, finish);
};

function palette(tokenSource = tokens, header = stateHeader, source = stateSource) {
    const values = new Map();
    const resolve = value => {
        if (/^"#[0-9a-f]{6}"$/i.test(value)) return value.slice(1, -1).toLowerCase();
        assert(values.has(value), `Unknown colour expression ${value}`);
        return values.get(value);
    };
    for (const namespace of ['Light', 'Dark', 'Brand', 'Preview', 'Device']) {
        const body = block(tokenSource, `namespace ${namespace} {`, `} // namespace ${namespace}`);
        for (const match of uncomments(body).matchAll(/inline const wxColour ([A-Za-z0-9_]+)\{("#[0-9a-f]{6}")\};/gi))
            values.set(`MD3::${namespace}::${match[1]}`, resolve(match[2]));
    }
    const aliases = block(header, 'namespace ThemeColor {', '} // namespace ThemeColor');
    for (const match of uncomments(aliases).matchAll(/inline const wxColour ([A-Za-z0-9_]+)\{([^}]+)\};/g))
        values.set(`ThemeColor::${match[1]}`, resolve(match[2].trim()));
    const mapping = new Map();
    const rows = uncomments(block(source, 'gDarkColors{', '\n};'));
    const entries = [...rows.matchAll(/\{\s*([^,{}]+),\s*([^{}]+)\}/g)];
    assert(entries.length >= 60, 'Compatibility rows must not silently disappear');
    for (const [, keyExpression, valueExpression] of entries) {
        const key = resolve(keyExpression.trim()), value = resolve(valueExpression.trim());
        if (mapping.has(key)) assert.equal(mapping.get(key), value, `Conflicting duplicate key ${key}`);
        mapping.set(key, value);
    }
    return { values, mapping, colour: (theme, name) => values.get(`MD3::${theme}::${name}`) };
}

const data = palette();
const surfaces = ['surface', 'surfaceDim', 'surfaceBright', 'scLowest', 'scLow', 'sc', 'scHigh', 'scHighest'];
const neutrals = [...surfaces, 'onSurface', 'onSurfaceVariant', 'outline', 'outlineVariant'];
const luminance = hex => {
    const values = hex.slice(1).match(/../g).map(v => parseInt(v, 16) / 255)
        .map(v => v <= 0.04045 ? v / 12.92 : ((v + 0.055) / 1.055) ** 2.4);
    return values[0] * 0.2126 + values[1] * 0.7152 + values[2] * 0.0722;
};
const contrast = (a, b) => (Math.max(luminance(a), luminance(b)) + 0.05) / (Math.min(luminance(a), luminance(b)) + 0.05);

function checkContrast(subject) {
    for (const theme of ['Light', 'Dark']) {
        for (const surface of surfaces) {
            for (const text of ['onSurface', 'onSurfaceVariant'])
                assert(contrast(subject.colour(theme, text), subject.colour(theme, surface)) >= 4.5, `${theme} ${text}/${surface}`);
            assert(contrast(subject.colour(theme, 'outline'), subject.colour(theme, surface)) >= 3, `${theme} outline/${surface}`);
        }
    }
}

function checkIdempotence(subject) {
    for (const [key, value] of subject.mapping)
        assert.equal(subject.mapping.get(value) ?? value, value, `Second dark remap changes ${key}`);
    for (const name of neutrals)
        assert.equal(subject.mapping.get(subject.colour('Dark', name)) ?? subject.colour('Dark', name), subject.colour('Dark', name));
}

test('neutral foreground and essential outline contrast covers both themes and every surface', () => checkContrast(data));
test('dark remapping is idempotent for every compatibility entry and neutral role', () => checkIdempotence(data));
test('every current neutral light role maps to its matching dark role', () => {
    for (const name of neutrals)
        assert.equal(data.mapping.get(data.colour('Light', name)), data.colour('Dark', name), name);
});
test('neutral container hierarchy remains monotonic in both themes', () => {
    const ramp = ['scLowest', 'scLow', 'sc', 'scHigh', 'scHighest'];
    for (let index = 1; index < ramp.length; ++index) {
        assert(luminance(data.colour('Light', ramp[index - 1])) > luminance(data.colour('Light', ramp[index])));
        assert(luminance(data.colour('Dark', ramp[index - 1])) < luminance(data.colour('Dark', ramp[index])));
    }
});
test('historical raw neutral keys retain compatibility with current dark defaults', () => {
    const old = { '#faf8fd': 'surface', '#dad9e0': 'surfaceDim', '#1a1b1f': 'onSurface', '#44464e': 'onSurfaceVariant',
        '#f4f2f9': 'scLow', '#eeedf3': 'sc', '#e8e7ee': 'scHigh', '#e2e1e9': 'scHighest', '#c5c6d0': 'outlineVariant', '#75777f': 'outline' };
    for (const [key, role] of Object.entries(old)) assert.equal(data.mapping.get(key), data.colour('Dark', role));
});
test('light restoration explicitly prefers every current semantic neutral', () => {
    const body = block(stateSource, 'wxColour StateColor::lightModeColorFor', '\nwxColour StateColor::darkModeColorFor');
    const roles = ['Surface', 'SurfaceDim', 'SurfaceBright', 'SurfaceContainerLowest', 'SurfaceContainerLow', 'SurfaceContainer',
        'SurfaceContainerHigh', 'SurfaceContainerHighest', 'OnSurface', 'OnSurfaceVariant', 'Outline', 'OutlineVariant'];
    const registered = [...body.matchAll(/MD3::Role::([A-Za-z]+)/g)].map(match => match[1]);
    assert.deepEqual(registered, roles);
    assert(body.includes('result[MD3::resolve(role, true)] = MD3::resolve(role, false);'));
});
test('unknown custom RGB values remain outside the compatibility table', () => {
    for (const value of ['#123456', '#fedcba', '#8429a0', '#012345']) assert(!data.mapping.has(value));
    const body = block(stateSource, 'inline wxColour darkModeColorFor2', '\nstd::map<wxColour, wxColour> revert');
    assert(body.includes('if (!gDarkMode)'));
    assert(body.includes('if (iter != gDarkColors.end()) return iter->second;'));
    assert(body.trim().endsWith('return color;\n}'));
});
test('accent override, contextual resolution and seed generation remain byte-identical to the baseline', () => {
    // This narrow source fingerprint is intentionally not a runtime accent test.
    // It protects the no-behaviour-change boundary of this palette-only unit.
    const portions = [tokens.slice(tokens.indexOf('namespace detail {'), tokens.indexOf('struct DensityMetrics')),
        tokens.slice(tokens.indexOf('// Live accent generator'))];
    assert.equal(createHash('sha256').update(portions.join('\n')).digest('hex'),
        'dbed921ac70ceefcabd39aae5f47215afbef244b3a15512f44dc0593a95eaabe');
});
test('contrast and dark-remap checks reject deliberate invalid palette mutations', () => {
    assert.throws(() => checkContrast(palette(tokens.replace('onSurfaceVariant{"#b9c8da"}', 'onSurfaceVariant{"#344557"}'))));
    assert.throws(() => checkIdempotence(palette(tokens.replace('onSurface{"#e8eff8"}', 'onSurface{"#e8e7ee"}'))));
    checkContrast(data);
    checkIdempotence(data);
});
