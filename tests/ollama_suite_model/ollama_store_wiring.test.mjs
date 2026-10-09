// Source contract for the Model Store's filters, grouping, sorting and
// pre-selection explanations. The query model is covered by store_query_tests.cpp.
import {test} from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const strip = (source) => source.replace(/\/\*[\s\S]*?\*\//g, '').replace(/\/\/[^\n]*/g, '');
const dialog = strip(readFileSync('src/slic3r/GUI/OllamaSuite/OllamaSuiteDialog.cpp', 'utf8'));
const text = strip(readFileSync('src/slic3r/GUI/OllamaSuite/OllamaSuiteText.cpp', 'utf8'));

test('every required facet has a picker that re-queries the store', () => {
  for (const [member, facet] of [['m_filter_state', 'StateFilter'], ['m_filter_family', 'FamilyFilter'], ['m_filter_variant', 'VariantFilter'],
    ['m_filter_capability', 'CapabilityFilter'], ['m_filter_quantization', 'QuantizationFilter'], ['m_filter_size', 'SizeFilter'],
    ['m_filter_fit', 'FitFilter'], ['m_store_group', 'Grouping'], ['m_store_sort', 'SortOrder']])
    assert.match(dialog, new RegExp(member + '\\s*=\\s*store_picker\\s*\\([^;]*StoreUi::' + facet + '\\b'), member + ' is a named picker');
  assert.match(dialog, /picker->Bind\s*\(\s*wxEVT_COMBOBOX[\s\S]{0,120}?render_models\s*\(\s*\)/, 'a picker change re-renders the store');
  assert.match(dialog, /picker->SetName\s*\(/, 'pickers carry accessible names');
  assert.match(dialog, /StoreUi::ClearFilters/, 'every filter can be cleared in one action');
});

test('the store is queried with per-entry verdicts beside the regex search', () => {
  assert.match(dialog, /query_store\s*\(\s*entries\s*,\s*query\s*,/);
  assert.match(dialog, /assess\s*\(\s*m\s*,\s*m_state\.hardware\s*,\s*m_state\.fit_settings\s*\)\.verdict/, 'fit filters use current measurements');
  assert.match(dialog, /m_model_search->SetOnRegexToggle/, 'plain text search keeps its adjacent regex builder');
  assert.match(dialog, /m_query_summary->SetLabel\s*\(\s*OllamaText::query_summary/, 'active filters are explained');
});

test('a disabled variant picker names its unmet condition', () => {
  assert.match(dialog, /m_filter_variant->Enable\s*\(\s*family\.has_value\s*\(\s*\)\s*\)/);
  assert.match(dialog, /m_filter_variant->SetToolTip\s*\([^;]*StoreUi::ChooseFamilyFirst/);
});

test('rows are explained before inspection, and headings explain their group', () => {
  assert.match(dialog, /OllamaText::model_explanation\s*\(\s*m\s*,/);
  assert.match(dialog, /OllamaText::group_explanation\s*\(/);
  assert.match(dialog, /Operation::Show\s*,\s*\{\s*\{\s*"model"\s*,\s*models\s*\[\s*i\s*\]\.name/, 'installed models get verified metadata on refresh');
});

test('the store never curates or infers from model names', () => {
  for (const source of [dialog, text])
    assert.doesNotMatch(source, /"(llama|qwen|gemma|mistral|phi|deepseek|llava|gpt)[^"]*"/i, 'no model names are hard-coded');
  assert.doesNotMatch(text, /model\.name[^;]*find\s*\(\s*"/, 'explanations never search a model name for meaning');
});
