import assert from 'node:assert/strict';
import test from 'node:test';
import vm from 'node:vm';
import { readFile } from 'node:fs/promises';
const source = await readFile(new URL('../site/narration.js', import.meta.url), 'utf8');
const english = { voiceURI: 'voice-en-1', name: 'English voice', lang: 'en-CA', localService: true };
const cantonese = { voiceURI: 'voice-yue-1', name: 'Cantonese voice', lang: 'zh-HK', localService: true };
function environment({ supported = true, initialVoices = [english, cantonese], preferences = {} } = {}) {
  const state = { narratorEnabled: false, narratorPaused: false, narratorLanguage: 'en', narratorVoiceEn: '', narratorVoiceYue: '', narratorRate: 1, narratorPitch: 1, ...preferences };
  let voices = initialVoices;
  const listeners = new Map();
  const subscriptions = [];
  const spoken = [];
  let cancelled = 0;
  const context = vm.createContext({
    Date, SpeechSynthesisUtterance: class { constructor(text) { this.text = text; } },
    speechSynthesis: supported ? { getVoices: () => voices, speak: utterance => spoken.push(utterance), cancel: () => { cancelled++; }, addEventListener: (key, handler) => listeners.set(key, handler), removeEventListener: key => listeners.delete(key) } : undefined,
    BambuSite: { get: key => state[key], subscribe: handler => subscriptions.push(handler), pair: (key, params) => ({ en: 'English: ' + key + (params?.name || ''), yue: '廣東話：' + key + (params?.name || '') }) },
    addEventListener: (key, handler) => listeners.set(key, handler)
  });
  vm.runInContext(source, context);
  return { api: context.BambuNarration, state, spoken, listeners, setVoices: value => { voices = value; listeners.get('voiceschanged')?.(); }, update: (key, value) => { state[key] = value; subscriptions.forEach(fn => fn([key])); }, cancelled: () => cancelled };
}
test('narration is opt-in and missing synthesis is honest', () => {
  const normal = environment();
  assert.equal(normal.api.narrate('error', 'failure'), false);
  assert.equal(normal.spoken.length, 0);
  const missing = environment({ supported: false, preferences: { narratorEnabled: true } });
  assert.equal(missing.api.selection('en').supported, false);
  assert.equal(missing.api.narrate('error', 'failure'), false);
});
test('late voice enumeration is re-read and Cantonese does not use Mandarin', () => {
  const env = environment({ initialVoices: [] });
  assert.equal(env.api.available('en').length, 0);
  env.setVoices([english, cantonese, { voiceURI: 'mandarin', lang: 'zh-CN', localService: true }]);
  assert.equal(env.api.available('en').length, 1);
  assert.equal(env.api.available('yue').length, 1);
  assert.ok(env.listeners.has('voiceschanged'));
});
test('voice choice uses stable URI and retains an absent choice while falling back', () => {
  const env = environment({ preferences: { narratorVoiceEn: 'gone-voice' } });
  assert.equal(env.api.selection('en').missing, true);
  assert.equal(env.api.selection('en').voice.voiceURI, english.voiceURI);
  assert.equal(env.state.narratorVoiceEn, 'gone-voice');
  env.setVoices([english, { ...english, voiceURI: 'gone-voice', name: english.name }]);
  assert.equal(env.api.selection('en').missing, false);
  assert.equal(env.api.selection('en').voice.voiceURI, 'gone-voice');
});
test('both tracks are strictly serialized and urgent events ignore category cooldown', () => {
  const env = environment({ preferences: { narratorEnabled: true, narratorLanguage: 'both' } });
  assert.equal(env.api.narrate('error', 'first'), true);
  assert.equal(env.spoken.length, 1);
  assert.equal(env.spoken[0].voice.voiceURI, english.voiceURI);
  env.api.narrate('error', 'second');
  assert.equal(env.spoken.length, 1);
  env.spoken[0].onend();
  assert.equal(env.spoken[1].voice.voiceURI, cantonese.voiceURI);
  env.spoken[1].onend();
  assert.match(env.spoken[2].text, /second/);
});
test('missing Cantonese voice leaves English available without a fake voice', () => {
  const env = environment({ initialVoices: [english], preferences: { narratorEnabled: true, narratorLanguage: 'both' } });
  env.api.narrate('error', 'notice');
  env.spoken[0].onend();
  assert.equal(env.spoken.length, 1);
  assert.equal(env.api.selection('yue').voice, null);
});
test('rate and pitch are bounded, with normal delivery for non-finite preferences', () => {
  const env = environment({ preferences: { narratorEnabled: true, narratorRate: 99, narratorPitch: -1 } });
  env.api.narrate('error', 'bounds');
  assert.equal(env.spoken[0].rate, 10); assert.equal(env.spoken[0].pitch, 0);
  env.spoken[0].onend(); env.state.narratorRate = 'invalid'; env.state.narratorPitch = Infinity;
  env.api.narrate('error', 'fallback');
  assert.equal(env.spoken[1].rate, 1); assert.equal(env.spoken[1].pitch, 1);
});
test('turning off or pausing cancels every queued line and stale completion cannot restart it', () => {
  const env = environment({ preferences: { narratorEnabled: true, narratorLanguage: 'both' } });
  env.api.narrate('error', 'notice');
  env.update('narratorPaused', true);
  assert.equal(env.api.queueLength(), 0);
  env.spoken[0].onend();
  assert.equal(env.spoken.length, 1); assert.equal(env.cancelled(), 1);
});
test('non-urgent cooldown suppresses repeated speech and replaces queued older copy', () => {
  const env = environment({ preferences: { narratorEnabled: true } });
  env.api.narrate('error', 'blocking');
  env.api.narrate('info', 'old');
  env.api.narrate('info', 'new');
  env.spoken[0].onend();
  assert.match(env.spoken[1].text, /new/);
  assert.equal(env.api.narrate('info', 'repeated'), false);
});
test('page teardown removes voice subscription and stops speech', () => {
  const env = environment();
  env.listeners.get('pagehide')();
  assert.equal(env.listeners.has('voiceschanged'), false);
  assert.equal(env.cancelled(), 1);
});
