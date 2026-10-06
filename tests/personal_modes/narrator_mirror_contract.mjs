import { readFileSync } from 'node:fs';
import assert from 'node:assert/strict';
const file = new URL('../../src/slic3r/GUI/TtsNarrator.cpp', import.meta.url);
const source = readFileSync(file, 'utf8').replaceAll('\r\n', '\n');

function verify(text) {
    const begin = text.indexOf('void pump_queue() {');
    const end = text.indexOf('// --- printer state watch', begin);
    assert(begin >= 0 && end > begin, 'queue boundary must exist');
    const pump = text.slice(begin, end);
    const quiet = pump.indexOf('if (s_quiet || s_screen_reader)');
    const track = pump.indexOf('if (!next) return;');
    const mirror = pump.indexOf('HomeAssistant::speak_on_speakers(wxString(next->text));');
    const unavailable = pump.indexOf('if (!resolved.available)');
    assert(quiet >= 0 && track > quiet && mirror > track && unavailable > mirror,
        'external mirror survives missing local voices and follows quiet/queue admission');
    assert(!text.includes('PersonalVocabulary::display('), 'private display substitutions must not enter outgoing narration');
    assert(text.includes('ExternalMirrorStatus::PlaybackCompletionUnavailable'), 'external playback limit must be exposed');
}

verify(source);
assert.throws(() => verify(source.replace('HomeAssistant::speak_on_speakers(wxString(next->text));', '')), /external mirror/);
assert.throws(() => verify(source + '\nPersonalVocabulary::display(line);'), /private display/);
assert.throws(() => verify(source.replaceAll('ExternalMirrorStatus::PlaybackCompletionUnavailable', 'ExternalMirrorStatus::Unconfigured')), /playback limit/);
console.log('PASS narrator mirror source contract and 3 deliberate negative mutations');
