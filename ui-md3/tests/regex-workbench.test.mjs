import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

// Source contract for the regex builder's workbench analysis: the engine
// identity the worker reports, the flag and escaping tables, the capability
// matrix with its engine probes, the structure tree, token annotation,
// compatibility warnings and backtracking-risk diagnostics. The behaviour
// itself is proven by tests/regex_analysis (pure model) and the
// [regex_workbench] cases of tests/bounded_regex (real worker); this file
// keeps the builder popover, protocol, build lists, catalogue list and
// documentation wired to them. Each check is run once against a mutated
// copy that removes one binding, which must turn it red.

const testDir = path.dirname(fileURLToPath(import.meta.url));
const repoDir = path.resolve(testDir, '..', '..');
const read = async (...parts) => (await readFile(path.join(repoDir, ...parts), 'utf8')).replace(/\r\n/g, '\n');
const code = (text) => text.replace(/\/\*[\s\S]*?\*\//g, '').replace(/^[ \t]*\/\/.*$/gm, '');
const fn = (source, signature) => {
  const start = source.indexOf(signature);
  assert.notEqual(start, -1, `${signature} must exist`);
  return source.slice(start, source.indexOf('\n}\n', start) + 2);
};

const sources = {
  popup: code(await read('src', 'slic3r', 'GUI', 'Widgets', 'RegexBuilderPopup.cpp')),
  header: code(await read('src', 'slic3r', 'GUI', 'Widgets', 'RegexBuilderPopup.hpp')),
  analysis: await read('src', 'slic3r', 'GUI', 'Widgets', 'RegexAnalysis.hpp'),
  protocol: code(await read('src', 'slic3r', 'GUI', 'Widgets', 'BoundedRegexProtocol.hpp')),
  client: code(await read('src', 'slic3r', 'GUI', 'Widgets', 'BoundedRegex.cpp')),
  clientHeader: code(await read('src', 'slic3r', 'GUI', 'Widgets', 'BoundedRegex.hpp')),
  guiList: await read('src', 'slic3r', 'CMakeLists.txt'),
  tests: await read('tests', 'CMakeLists.txt'),
  i18nList: await read('bbl', 'i18n', 'list.txt'),
  doc: await read('docs', 'features', 'windows', 'regex-builder.md'),
};

function popupContract({ popup, header }) {
  // Three tabs; Explain is built on demand and refreshed with every edit.
  assert.match(header, /enum Tab : int \{ TabBuild = 0, TabExplain = 1, TabReference = 2 \};/);
  assert.match(popup, /m_tab_explain = new Button\(this, _L\("Explain"\)\);/);
  assert.match(popup, /m_tab_explain->Bind\(wxEVT_BUTTON, \[this\]\(wxCommandEvent &\) \{ switchTab\(TabExplain\); \}\);/);
  const switchTab = fn(popup, 'void RegexBuilderPopup::switchTab(');
  assert.match(switchTab, /if \(tab == TabExplain && !m_explain_scroll\)\s*buildExplain\(\);/);
  assert.match(switchTab, /refreshAnalysis\(\);/);
  assert.match(fn(popup, 'void RegexBuilderPopup::evaluate('), /return;\n\s*refreshAnalysis\(\);/,
    'every pattern or flag change re-runs the analysis');

  // The live analysis uses the real model with the field's own flags.
  const refresh = fn(popup, 'void RegexBuilderPopup::refreshAnalysis(');
  assert.match(refresh, /RA::analyze\(pattern, options\)/);
  for (const flag of ['regex_mode     = m_regex_on', 'case_sensitive = m_case_on', 'multiline      = m_multiline_on',
    'whole_word     = m_word_on', 'code_unit_bits = engine.code_unit_bits'])
    assert.ok(refresh.includes(`options.${flag};`), `analysis option ${flag}`);
  assert.match(refresh, /engineInfo\(explain_visible\)/, 'the engine identity comes from the worker');
  assert.match(refresh, /BoundedRegex::validate\(pattern, engine_options\)/, 'the verdict is the engine\'s');
  assert.match(refresh, /verdict\.error_offset/, 'the verdict names the engine\'s error offset');
  for (const view of ['m_tree', 'm_tokens', 'm_compat', 'm_risks'])
    assert.match(refresh, new RegExp(`${view}->ChangeValue\\(`), `${view} is refreshed`);
  assert.match(refresh, /RA::tree_lines\(analysis\)/);
  assert.match(refresh, /analysis\.tokens/);
  assert.match(refresh, /analysis\.compatibility/);
  assert.match(refresh, /analysis\.risks/);
  assert.match(refresh, /RA::flag_rows\(\)/);
  assert.match(fn(popup, 'const RA::EngineInfo &engineInfo('), /BoundedRegex::describe_engine\(\)/);

  // Read-only, named, keyboard-reachable views on the kit editor.
  const explain = fn(popup, 'void RegexBuilderPopup::buildExplain(');
  assert.match(explain, /new TextAreaEditor\(m_explain_scroll[^;]*wxTE_MULTILINE \| wxTE_READONLY/);
  for (const name of ['Pattern structure', 'Token annotations', 'Compatibility notes', 'Backtracking risk findings'])
    assert.ok(explain.includes(`view(_L("${name}")`), `${name} view`);
  assert.match(explain, /m_use_example->Bind\(wxEVT_BUTTON, \[this\]\(wxCommandEvent &\) \{ useRiskExample\(\); \}\);/);

  // Reference: flag table, escaping rules and the capability matrix with its check.
  const reference = fn(popup, 'void RegexBuilderPopup::buildReference(');
  assert.match(reference, /for \(const RA::FlagRow &row : RA::flag_rows\(\)\)/);
  assert.match(reference, /for \(const RA::EscapeRule &rule : RA::escape_rules\(\)\)/);
  assert.match(reference, /for \(const RA::Capability &row : RA::capabilities\(\)\)/);
  assert.match(reference, /RA::Support::Unsupported/, 'unsupported constructs stay listed');
  assert.match(reference, /check->Bind\(wxEVT_BUTTON, \[this\]\(wxCommandEvent &\) \{ checkCapabilities\(\); \}\);/);
  const check = fn(popup, 'void RegexBuilderPopup::checkCapabilities(');
  assert.match(check, /observeProbe\(row\)/);
  assert.match(check, /RA::confirms\(row, seen\)/);
  assert.match(fn(popup, 'RA::ProbeObservation observeProbe('), /BoundedRegex::search\(row\.probe, subject\)/);
}

function protocolContract({ protocol, client, clientHeader }) {
  assert.match(protocol, /enum class Mode : std::uint32_t \{[^}]*Describe = 4 \};/);
  assert.match(protocol, /inline constexpr std::uint32_t kVersion\s*= 2;/);
  assert.match(protocol, /mode > static_cast<std::uint32_t>\(Mode::Describe\)/);
  const descriptor = fn(protocol, 'inline std::string engine_descriptor(');
  for (const part of ['BOOST_VERSION', 'BOOST_REGEX_MAX_STATE_COUNT', 'sizeof(wchar_t) * CHAR_BIT', 'BOOST_REGEX_USE_WIN32_LOCALE'])
    assert.ok(descriptor.includes(part), `descriptor reports ${part}`);
  assert.match(protocol, /if \(request\.mode == Mode::Describe\) \{[\s\S]*?described\.diagnostic = engine_descriptor\(\);/);
  assert.match(protocol, /m_compile_result\.error_offset = std::min<std::size_t>\(\s*static_cast<std::size_t>\(error\.position\(\)\)/);
  assert.match(protocol, /append_u32\(payload, result\.error_offset == kNoErrorOffset/);
  assert.match(protocol, /result\.error_offset = error_offset == kNoOffsetWire/);
  assert.match(clientHeader, /std::size_t\s+error_offset = kNoErrorOffset;/);
  assert.match(clientHeader, /Result describe_engine\(const Options &options = \{\}\);/);
  assert.match(fn(client, 'Result describe_engine('), /execute\(Protocol::Mode::Describe, \{\}, \{\}, 0, options\)/);
}

function registrationContract({ analysis, guiList, tests, i18nList }) {
  assert.match(guiList, /^\s*GUI\/Widgets\/RegexAnalysis\.hpp\s*$/m, 'the model is part of libslic3r_gui');
  assert.match(tests, /^add_subdirectory\(regex_analysis\)$/m, 'the pure model tests are built');
  assert.match(i18nList, /^src\/slic3r\/GUI\/Widgets\/RegexAnalysis\.hpp$/m, 'model messages are extracted');
  assert.match(analysis, /#pragma push_macro\("L"\)[\s\S]*#define L\(s\) s[\s\S]*#pragma pop_macro\("L"\)/,
    'model text is marked for extraction without leaking the macro');
  assert.doesNotMatch(analysis, /#include <wx\/|#include <boost\//, 'the model stays free of wx and Boost');
}

function documentationContract({ doc }) {
  for (const phrase of ['Explain tab', 'capability matrix', 'Check against the engine', 'token-by-token',
    'structure tree', 'backtracking', 'Describe', 'error offset', 'regex_analysis_tests', '[regex_workbench]'])
    assert.ok(doc.includes(phrase), `the article documents ${phrase}`);
}

const contracts = [
  ['builder popover', popupContract,
    (s) => ({ ...s, popup: s.popup.replace('        return;\n    refreshAnalysis();\n', '        return;\n') })],
  ['worker protocol', protocolContract, (s) => ({ ...s, protocol: s.protocol.replace('Describe = 4', 'Unused = 4') })],
  ['build and catalogue registration', registrationContract,
    (s) => ({ ...s, tests: s.tests.replace('add_subdirectory(regex_analysis)\n', '') })],
  ['feature article', documentationContract, (s) => ({ ...s, doc: s.doc.replace(/capability matrix/g, 'list') })],
];

for (const [name, contract, mutate] of contracts) {
  test(`regex workbench ${name} is wired`, () => contract(sources));
  test(`regex workbench ${name} check turns red when a binding is removed`, () => {
    const broken = mutate(sources);
    assert.notDeepEqual(broken, sources, 'the mutation must change the source');
    assert.throws(() => contract(broken));
  });
}
