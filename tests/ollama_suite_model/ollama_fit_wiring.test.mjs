// Source contract for the local Ollama suite's hardware-fit wiring. The pure
// model is covered by ollama_suite_model_tests.cpp; this guards the places the
// native dialog and client feed it, which a model test cannot see.
import {test} from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const read = (path) => readFileSync(path, 'utf8');
const strip = (source) => source.replace(/\/\*[\s\S]*?\*\//g, '').replace(/\/\/[^\n]*/g, '');
const dialog = strip(read('src/slic3r/GUI/OllamaSuite/OllamaSuiteDialog.cpp'));
const client = strip(read('src/slic3r/GUI/OllamaSuite/OllamaClient.cpp'));

test('the dialog never discards measured destination space or context memory', () => {
  assert.doesNotMatch(dialog, /free_disk\s*\.\s*reset\s*\(/, 'free destination space must reach the verdict');
  assert.doesNotMatch(dialog, /\bfit\s*\([^;]*,\s*\{\s*\}\s*\)/, 'a verdict must not be computed without context memory');
  const verdicts = dialog.match(/\bassess\s*\(/g) ?? [];
  assert.ok(verdicts.length >= 4, 'inspection, registry metadata, recomputation and launch preflight all use assess()');
});

test('estimate settings are pickers that persist and recompute every verdict', () => {
  assert.match(dialog, /fit-settings\.json/, 'estimate settings persist in the suite state folder');
  assert.match(dialog, /m_fit_context\s*->\s*Bind\s*\(\s*wxEVT_COMBOBOX/, 'context picker recomputes');
  assert.match(dialog, /m_fit_cache\s*->\s*Bind\s*\(\s*wxEVT_COMBOBOX/, 'cache precision picker recomputes');
  assert.match(dialog, /_L\("Measure hardware again"\)/, 'hardware can be measured again from the Models section');
});

test('runtime refresh records the runtime\'s own backend evidence', () => {
  assert.match(dialog, /runtime_memory\s*\(\s*running\.value\s*\)/, 'GET /api/ps sizes are read');
  assert.match(dialog, /observe_backend\s*\(/, 'a loaded model becomes backend evidence');
  assert.match(dialog, /backend\.json/, 'backend evidence persists with the adapters and runtime version it belongs to');
  assert.match(dialog, /apply_backend\s*\(/, 'evidence is re-validated against current adapters before use');
});

test('the client measures adapters and the documented model destination', () => {
  assert.match(client, /CreateDXGIFactory1/, 'hardware graphics adapters are enumerated');
  assert.match(client, /QueryVideoMemoryInfo/, 'the operating system memory budget caps usable GPU memory');
  assert.match(client, /CheckInterfaceSupport/, 'the driver version is recorded');
  assert.match(client, /DXGI_ADAPTER_FLAG_SOFTWARE/, 'software adapters are not GPU evidence');
  assert.match(client, /L"OLLAMA_MODELS"/, 'the documented OLLAMA_MODELS setting is read');
  assert.match(client, /HKEY_CURRENT_USER/, 'the persisted user setting is read');
  assert.match(client, /probe_destination\s*\(/, 'candidates are proven against installed manifests');
  assert.doesNotMatch(client, /(nvidia|geforce|radeon|rtx|cuda)/i, 'no backend support is inferred from an adapter or model name');
});

test('new suite text is extracted for translation and the model test is registered', () => {
  assert.match(read('bbl/i18n/list.txt'), /^src\/slic3r\/GUI\/OllamaSuite\/OllamaSuiteText\.cpp$/m);
  assert.match(read('src/slic3r/CMakeLists.txt'), /GUI\/OllamaSuite\/OllamaSuiteText\.cpp/);
  assert.match(read('tests/CMakeLists.txt'), /^add_subdirectory\(ollama_suite_model\)$/m);
});
