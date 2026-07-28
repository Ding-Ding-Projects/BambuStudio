import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

const testDir = path.dirname(fileURLToPath(import.meta.url));
const uiDir = path.resolve(testDir, '..');
const repoDir = path.resolve(uiDir, '..');

const landing = await readFile(path.join(uiDir, 'landing.html'), 'utf8');
const siteCss = await readFile(path.join(uiDir, 'site', 'site.css'), 'utf8');
const tabsJs = await readFile(path.join(uiDir, 'site', 'tabs.js'), 'utf8');
// "must not contain" assertions run against code only: the comments explain why
// a construct is banned, and naming it there must not fail the test.
const stripComments = (source) => source
  .replace(/\/\*[\s\S]*?\*\//g, '')
  .replace(/^[ \t]*\/\/.*$/gm, '');
const siteCssCode = stripComments(siteCss);
const tabsJsCode = stripComments(tabsJs);
const smartHome = await readFile(
  path.join(repoDir, 'src', 'slic3r', 'GUI', 'SmartHomeDialog.cpp'),
  'utf8'
);
const smartHomeHeader = await readFile(
  path.join(repoDir, 'src', 'slic3r', 'GUI', 'SmartHomeDialog.hpp'),
  'utf8'
);
const msgDialog = await readFile(
  path.join(repoDir, 'src', 'slic3r', 'GUI', 'MsgDialog.cpp'),
  'utf8'
);
const msgDialogHeader = await readFile(
  path.join(repoDir, 'src', 'slic3r', 'GUI', 'MsgDialog.hpp'),
  'utf8'
);

test('the site never truncates or side-scrolls its way out of a clipping bug', () => {
  // The runtime gate fails any element whose content is wider than its box, so
  // an ellipsis or a horizontal scroller would trade a visible clip for a
  // hidden one. Neither is allowed anywhere in the stylesheet.
  assert.doesNotMatch(siteCssCode, /text-overflow\s*:\s*ellipsis/);
  assert.doesNotMatch(siteCssCode, /overflow(-x)?\s*:\s*(auto|scroll)/);
  // Author `display` rules outrank the UA sheet, so [hidden] needs enforcing.
  assert.match(siteCss, /\[hidden\]\s*\{\s*display:\s*none\s*!important;\s*\}/);
  // Long strings wrap instead of pushing the page sideways.
  assert.match(siteCss, /body\s*\{[\s\S]*?overflow-x:\s*hidden;/);
  assert.ok(
    (siteCss.match(/overflow-wrap:\s*anywhere/g) || []).length >= 25,
    'wrapping must be the default answer to long localized strings'
  );
});

test('every header target keeps a 44px floor, including the tab strip', () => {
  assert.match(siteCss, /\.iconbtn\s*\{\s*width:\s*44px;\s*height:\s*44px;\s*flex:\s*0 0 44px;/);
  assert.match(siteCss, /\.language-select\s*\{\s*height:\s*44px;\s*min-height:\s*44px;/);
  assert.match(siteCss, /\.btn\s*\{[\s\S]*?min-height:\s*44px;\s*min-width:\s*44px;/);
  assert.match(siteCss, /\.tab\s*\{[\s\S]*?min-height:\s*44px;\s*min-width:\s*44px;/);
  assert.match(siteCss, /\.menuitem,\s*\.tabsearch-item\s*\{[\s\S]*?min-height:\s*44px;/);
  // The calendar keeps a 44px touch height while giving up horizontal padding,
  // because seven columns still have to fit inside a phone-width panel.
  assert.match(siteCss, /\.cal-day\s*\{[\s\S]*?min-height:\s*44px;\s*min-width:\s*0;\s*padding:\s*0;/);
  assert.match(siteCss, /\.swatch\s*\{\s*width:\s*44px;\s*height:\s*44px;/);
});

test('the tab strip degrades through overflow instead of clipping', () => {
  // Wrapping is the CSS-only guarantee; the staged algorithm is the browser-like
  // behaviour layered on top of it.
  assert.match(siteCss, /\.tabstrip-tabs\s*\{\s*display:\s*flex;\s*flex-wrap:\s*wrap;/);
  assert.match(tabsJs, /function fits\(\)[\s\S]*?offsetTop/);
  assert.match(tabsJs, /overflowButton\.hidden = false;[\s\S]*?icons-only/);
  assert.match(tabsJs, /classList\.add\('overflowed'\)/);
  assert.match(tabsJs, /searchButton\.hidden = true;/, 'the last stage must free the search slot');
  // requestAnimationFrame never fires in a page nobody paints, which is exactly
  // how the deploy gate and headless capture load this site.
  assert.doesNotMatch(tabsJsCode, /requestAnimationFrame/);
  assert.match(tabsJs, /layoutPending = setTimeout/);
  assert.match(tabsJs, /doc\.fonts\.load\("400 20px 'Material Symbols Outlined'"\)/);
});

test('every adjustment surface carries its own regex-capable search', async () => {
  const views = await readFile(path.join(uiDir, 'site', 'views.js'), 'utf8');
  const settingsModule = await readFile(path.join(uiDir, 'site', 'settings.js'), 'utf8');
  const changelogModule = await readFile(path.join(uiDir, 'site', 'changelog.js'), 'utf8');
  const tabsModule = await readFile(path.join(uiDir, 'site', 'tabs.js'), 'utf8');
  // Four surfaces let the user adjust or find something, and each owns a search
  // bar built from the same component — not one shared bar on the Settings tab.
  assert.match(settingsModule, /labelKey: 'settings\.search'[\s\S]*?items: searchableItems/);
  assert.match(settingsModule, /function mountAppearanceSearch[\s\S]*?createSearchField/);
  assert.match(views, /class="you-search"/);
  assert.match(views, /mountAppearanceSearch\(panel\)/);
  assert.match(changelogModule, /labelKey: 'changelog\.search'/);
  assert.match(tabsModule, /labelKey: 'shell\.tabsearch'/);
  // All of them go through createSearchField, so plain text stays the default
  // and the builder is one button away from each.
  for (const [name, source] of [['settings', settingsModule], ['changelog', changelogModule],
    ['tabs', tabsModule]]) {
    assert.match(source, /BambuRegex\.createSearchField\(/, `${name} must use the shared field`);
  }
});

test('focus is never dropped on the floor by the chrome', () => {
  // The first tab stop on the page has to become a visible 44px target.
  assert.match(siteCss, /\.sr-only:focus,\s*\.sr-only:focus-visible\s*\{[\s\S]*?min-height:\s*44px;/);
  assert.match(siteCss, /\.sr-only:focus[\s\S]*?clip:\s*auto;/);
  assert.match(landing, /<main class="wrap" id="panels" tabindex="-1">/);
  // Closing a popover returns focus to whatever opened it.
  assert.match(tabsJs, /function closeMenus\(options\)[\s\S]*?menuOpener\.focus\(\)/);
  assert.match(tabsJs, /function onMenuKeydown[\s\S]*?ArrowDown/);
  // A tab sitting in the overflow menu is display:none; focusing it there would
  // send focus to <body>, so it is un-overflowed and re-laid-out first.
  assert.match(
    tabsJs,
    /elements\[id\]\.classList\.remove\('overflowed'\);\s*runLayout\(\);[\s\S]*?options\.focus\) elements\[id\]\.focus\(\)/
  );
  // The strip keeps the wrap's horizontal inset, so end tabs are not clipped.
  assert.match(siteCss, /\.tabstrip\s*\{[\s\S]*?padding-block:\s*var\(--el-tabstrip-spacing, 6px\);/);
});

test('the tab strip is a real tablist with roving focus and reachable actions', () => {
  assert.match(tabsJs, /setAttribute\('role', 'tablist'\)/);
  assert.match(tabsJs, /tab\.setAttribute\('role', 'tab'\)/);
  assert.match(tabsJs, /tab\.setAttribute\('aria-controls', 'panel-' \+ definition\.id\)/);
  assert.match(tabsJs, /panel\.setAttribute\('role', 'tabpanel'\)/);
  assert.match(tabsJs, /panel\.setAttribute\('aria-labelledby', tab\.id\)/);
  assert.match(tabsJs, /tab\.tabIndex = isActive \? 0 : -1/);
  assert.match(tabsJs, /event\.key === 'ArrowRight'/);
  assert.match(tabsJs, /event\.key === 'Home'/);
  assert.match(tabsJs, /event\.shiftKey && \(event\.key === 'ArrowRight'/);
  assert.match(tabsJs, /site\.set\('tabOrder'[\s\S]*?site\.set\('tabPinned'[\s\S]*?site\.set\('tabGroups'/);
});

test('bilingual mode renders each string once, and a tab keeps its name when the label goes', async () => {
  const core = await readFile(path.join(uiDir, 'site', 'core.js'), 'utf8');
  /*
   * text() composes "English / 廣東話" for callers writing into a textContent.
   * applyCopy renders the companion itself, so taking the composed form there
   * printed the Cantonese twice on every label — and the doubled label widths
   * pushed the tab strip into icons-only at 1280px.
   */
  assert.match(core, /var primary = mode === 'yue_HK' \? \(both\.yue \|\| both\.en\) : both\.en;/);
  assert.doesNotMatch(core, /var primary = text\(key, params\);/);
  // Icons-only hides the label and the icon is aria-hidden, so the name has to
  // live on the button or the tab has no accessible name at all.
  assert.match(tabsJs, /tab\.setAttribute\('data-copy-attr', 'aria-label:' \+ definition\.copy/);
  assert.match(siteCss, /\.tabstrip-tabs\.icons-only \.tab-label \{ display: none; \}/);
});

test('landing chrome keeps accessible names and one shared language selector', () => {
  assert.match(landing, /id="languageMode"[^>]*aria-label="Language mode"/);
  assert.match(landing, /id="themeToggle"[\s\S]{0,220}data-copy-attr="aria-label:shell\.theme\.toggle/);
  assert.match(landing, /id="notifyButton"[\s\S]{0,220}data-copy-attr="aria-label:shell\.notifications/);
  assert.match(landing, /id="launchTop"[^>]*data-copy-attr="aria-label:shell\.launch"/);
  assert.match(landing, /class="logo" aria-hidden="true"/);
  assert.match(landing, /id="themeIcon" aria-hidden="true"/);
  assert.match(landing, /<a class="sr-only" href="#panels"/, 'a skip link must come first');
  assert.match(landing, /<noscript>/, 'the site must still hand out its links without scripting');
});

const prototype = await readFile(path.join(uiDir, 'index.html'), 'utf8');
const searchFieldLogic = await readFile(path.join(uiDir, 'app', 'searchfield.logic.js'), 'utf8');
const consumers = Object.fromEntries(await Promise.all(
  ['app/main.logic.js', 'app/screens/home.logic.js', 'app/screens/prepare.logic.js',
    'app/screens/project.logic.js', 'app/screens/calibration.logic.js',
    'app/screens/filament.logic.js', 'app/screens/multi.logic.js',
    'app/screens/settings.logic.js']
    .map(async (name) => [name, await readFile(path.join(uiDir, name), 'utf8')])
));
const dialogsModule = await readFile(path.join(uiDir, 'app', 'dialogs.js'), 'utf8');
const appStyles = await readFile(path.join(uiDir, 'app', 'styles.css'), 'utf8');

test('prototype icons are decorative and never become a button’s accessible name', () => {
  // An icon-font ligature is read as literal text, and on an icon-only button
  // that text wins over the title that was meant to name it.
  const icons = prototype.match(/<span[^>]*\bdata-icon\b[^>]*>/g) || [];
  assert.ok(icons.length > 100, `expected the full icon set, found ${icons.length}`);
  const exposed = icons.filter((tag) => !/aria-hidden/.test(tag));
  assert.deepEqual(exposed, [], 'every decorative icon span must be aria-hidden');
});

test('prototype preference switches carry role, state and a name', () => {
  assert.match(prototype, /role="switch" aria-checked="\{\{ p\.on \}\}" aria-label="\{\{ p\.label \}\}"/);
});

test('prototype dialogs are real modals with a close control the runtime can find', () => {
  assert.equal((prototype.match(/role="dialog"/g) || []).length, 4);
  assert.equal((prototype.match(/aria-modal="true"/g) || []).length, 4);
  assert.ok((prototype.match(/data-dialog-close/g) || []).length >= 5);
  assert.match(prototype, /<script src="\.\/app\/dialogs\.js"><\/script>/);
  assert.match(dialogsModule, /event\.key === 'Escape'/);
  assert.match(dialogsModule, /sibling\.inert = true/);
  assert.match(dialogsModule, /opener\.focus\(\)/);
});

test('the prototype title bar cannot push its window controls off-screen', () => {
  assert.match(prototype, /<div class="titlebar"/);
  assert.match(prototype, /<div class="tb-controls">/);
  assert.match(prototype, /<div class="tb-menus">/);
  assert.match(appStyles, /\.titlebar \.tb-controls\{[^}]*flex:0 0 auto;[^}]*\}/);
  assert.match(appStyles, /\.titlebar \.tb-menus\{[^}]*flex:0 1 auto;/);
  for (const width of [1000, 860, 700, 560]) {
    assert.match(appStyles, new RegExp(`@media \\(max-width:${width}px\\)\\{ \\.titlebar`));
  }
  // Every element in the prototype carries an inline style="display:flex", and
  // an inline style beats a stylesheet rule without !important — so a collapse
  // rule that omits it looks perfect in the diff and does absolutely nothing.
  const collapseRules = appStyles.match(/@media \(max-width:\d+px\)\{ \.titlebar [^}]*\{[^}]*\}/g) || [];
  assert.equal(collapseRules.length, 4, 'four collapse breakpoints are expected');
  for (const rule of collapseRules) {
    assert.match(rule, /display:none !important/, `a collapse rule without !important is inert: ${rule}`);
  }
});

test('the title bar block closes every element it opens', () => {
  /*
   * The .tb-controls wrapper once shipped unclosed, swallowing the tab bar,
   * because its closing tag lived in a two-line replacement that never matched
   * a CRLF file. The diff looked right; the artifact did not.
   */
  const start = prototype.indexOf('<div class="titlebar"');
  const end = prototype.indexOf('<!-- ===== TAB BAR ===== -->');
  assert.ok(start !== -1 && end > start, 'the title bar block must be findable');
  const block = prototype.slice(start, end);
  const opened = (block.match(/<div\b/g) || []).length;
  const closed = (block.match(/<\/div>/g) || []).length;
  assert.equal(opened, closed, `title bar opens ${opened} divs and closes ${closed}`);
});

test('every prototype search field is wired, and plain text is the default', () => {
  const fields = prototype.match(/<dc-import name="SearchField"[^>]*>/g) || [];
  assert.equal(fields.length, 10, 'the prototype has ten search fields');
  const inert = fields.filter((tag) => !/on-query=/.test(tag));
  assert.deepEqual(inert, [], 'a search field that filters nothing must not ship');
  // The mode travels with the query, so a consumer can tell opt-in regex from
  // plain text instead of compiling everything it is handed.
  assert.match(searchFieldLogic, /onQuery\(v, \{ regex:this\.state\.regex, flags:this\.searchFlags\(\) \}\)/);
  // searchFlags() drops `g` — a global regex carries lastIndex between calls
  // and starts skipping rows — and returns '' verbatim when every chip is off,
  // which is what "case-sensitive" looks like.
  assert.match(searchFieldLogic, /searchFlags\(\)\{[\s\S]*?return \(f\.i\?'i':''\)/);
  assert.doesNotMatch(searchFieldLogic, /searchFlags\(\)\{[\s\S]*?'g'/);
  // No consumer may substitute 'i' for an explicitly empty flag string.
  for (const [name, source] of Object.entries(consumers)) {
    assert.doesNotMatch(source, /Flags\|\|'i'/, `${name} makes case-sensitive search impossible`);
    assert.doesNotMatch(source, /replace\('g',''\)\|\|'i'/, `${name} makes case-sensitive search impossible`);
  }
});

test('Smart Home keeps a fixed footer around a work-area-capped scrolling body', () => {
  assert.match(smartHome, /MD3Dialog::Options\{true, false\}/);
  assert.match(smartHome, /new wxScrolledWindow\([\s\S]*?wxVSCROLL \| wxTAB_TRAVERSAL/);
  assert.match(smartHome, /GetContentSizer\(\)->Add\(m_scroll, 1, wxEXPAND\)/);
  assert.match(smartHome, /wxDisplay\(display_index\)\.GetClientArea\(\)/);
  assert.match(smartHome, /std::min\(target\.GetHeight\(\), available\.GetHeight\(\)\)/);
  assert.match(smartHome, /CenterOnParent\(\);\s*constrain_to_work_area\(false\);/);
  assert.match(smartHome, /auto \*close = new Button\(this,[\s\S]*?GetFooterSizer\(\)->Add\(close/);
  assert.match(smartHomeHeader, /void on_dpi_changed\(const wxRect &suggested_rect\) override;/);
});

test('Smart Home reflows actions and all changing text at the viewport width', () => {
  assert.match(smartHome, /auto \*media = new wxWrapSizer\(wxHORIZONTAL\)/);
  assert.match(smartHome, /auto \*adds = new wxBoxSizer\(wxVERTICAL\)/);
  assert.match(smartHome, /void make_responsive_action\(Button &button\)/);
  assert.ok(
    (smartHome.match(/make_responsive_action\(\*/g) || []).length >= 8,
    'every text action must use the shared 44-DIP target and natural-width policy'
  );
  assert.match(smartHome, /button\.SetAllowShrink\(false\)/);
  assert.doesNotMatch(smartHome, /button\.SetAllowShrink\(true\)/);
  assert.doesNotMatch(
    smartHome,
    /SetMinSize\(FromDIP\(wxSize\((?:300|210|160|140|136|120|110|104|96),/
  );
  assert.doesNotMatch(smartHome, /wxSize\(FromDIP\(140\), -1\)/);
  assert.match(smartHome, /handover_actions->Add\(m_add_printers, 0, wxEXPAND\)/);
  assert.match(smartHome, /adds->Add\(add_speaker, 0, wxEXPAND/);
  assert.match(smartHome, /speakers_row->Add\(clear_speakers, 0, wxEXPAND/);
  assert.match(smartHome, /auto_wrap \? LB_AUTO_WRAP : 0/);
  assert.match(smartHome, /m_status = label\([\s\S]*?true, true\);/);
  assert.match(smartHome, /m_discovery_status\s*=\s*label\([\s\S]*?true, true\);/);
  assert.match(smartHome, /m_speakers_label\s*=\s*label\([\s\S]*?true, true\);/);
  assert.match(smartHome, /m_lights_label = label\([\s\S]*?true, true\);/);
  assert.doesNotMatch(
    smartHome,
    /m_(?:status|discovery_status|speakers_label|lights_label)->SetLabel\(/
  );
  assert.ok(
    (smartHome.match(/update_wrapped_label\(/g) || []).length >= 9,
    'every dynamic status/entity update must use the relayout helper'
  );
});

test('Smart Home keeps long Home Assistant entity names horizontally reachable', () => {
  assert.match(
    smartHome,
    /new wxListBox\([\s\S]*?wxLB_SINGLE\s*\|\s*wxLB_HSCROLL\)/,
    'the native entity list must expose long friendly names instead of clipping them',
  );
});

test('Smart Home names and enlarges every custom toggle target', () => {
  assert.match(
    smartHome,
    /auto add_toggle =[\s\S]*?store->SetMinSize\(FromDIP\(wxSize\(44, 44\)\)\);[\s\S]*?store->SetName\(text\);/
  );
  assert.match(
    smartHome,
    /m_discovery_toggle->SetMinSize\(FromDIP\(wxSize\(44, 44\)\)\);[\s\S]*?m_discovery_toggle->SetName\(discovery_toggle_copy\);/
  );
  assert.match(smartHome, /m_url->SetName\(_L\("Home Assistant URL"\)\);/);
  assert.match(smartHome, /m_token->SetName\(_L\("Token"\)\);/);
  assert.match(smartHome, /m_url->SetMinSize\(wxSize\(-1, FromDIP\(44\)\)\);/);
  assert.match(smartHome, /m_token->SetMinSize\(wxSize\(-1, FromDIP\(44\)\)\);/);
  assert.match(smartHome, /m_volume->SetName\(_L\("Volume"\)\);/);
});

test('Smart Home preserves saved entity lists and bounds native list rendering', () => {
  assert.doesNotMatch(smartHome, /normalize_config_list|joined_config_list/);
  assert.doesNotMatch(
    smartHome,
    /values\.resize\(kMaxConfiguredHomeAssistantEntities\)/
  );
  assert.match(
    smartHome,
    /std::string updated = cfg->get\(key\);[\s\S]*?updated\.back\(\) != ';'[\s\S]*?updated \+= value;[\s\S]*?cfg->set\(key, updated\);/
  );
  assert.match(
    smartHome,
    /index < values\.size\(\) && index < kMaxConfiguredHomeAssistantEntities/
  );
  assert.match(
    smartHome,
    /if \(values\.size\(\) > kMaxConfiguredHomeAssistantEntities\)\s*break;/
  );
  assert.match(
    smartHome,
    /Existing "\s*"entries were kept; only the first 32 valid entries are active\./
  );
  assert.match(smartHome, /kMaxConfiguredHomeAssistantSegments\s*=\s*256/);
  assert.match(smartHome, /kMaxConfiguredHomeAssistantScanBytes\s*=\s*64 \* 1024/);
  assert.match(smartHome, /kMaxConfiguredHomeAssistantValueBytes\s*=\s*256/);
  assert.match(
    smartHome,
    /std::find\(\s*raw\.begin\(\) \+ begin,\s*raw\.begin\(\) \+ scan_end,\s*';'\)/
  );
  assert.match(smartHome, /kMaxVisibleHomeAssistantEntities\s*=\s*256/);
  assert.match(
    smartHome,
    /m_list->Freeze\(\);\s*m_list->Set\(rows\);\s*m_list->Thaw\(\);/
  );
  assert.doesNotMatch(smartHome, /m_list->Append\(/);
  assert.match(
    smartHome,
    /Showing %d of %d matching speakers and lights\./
  );
  assert.match(
    smartHomeHeader,
    /Label\s+\*m_config_limit_status[\s\S]*?Label\s+\*m_results_status/
  );
});

test('message dialogs wrap, refit and stack actions inside the active work area', () => {
  assert.match(msgDialog, /wxDisplay\(display_index\)\.GetClientArea\(\)/);
  assert.match(
    msgDialog,
    /bounded_message_content_width\(parent, 68 \* em\)/
  );
  assert.match(
    msgDialog,
    /void MsgDialog::refit_to_work_area\(bool recenter\)[\s\S]*?available\.GetWidth\(\)[\s\S]*?SetSize\(target\)/
  );
  assert.match(
    msgDialog,
    /void MsgDialog::reflow_footer_for_width\(int available_width\)[\s\S]*?SetCols\(1\)[\s\S]*?SetRows\(1\)/
  );
  assert.match(
    msgDialog,
    /SetButtonLabel[\s\S]*?refit_to_work_area\(!IsShown\(\)\)/
  );
  assert.match(
    msgDialog,
    /show_dsa_button[\s\S]*?refit_to_work_area\(!IsShown\(\)\)/
  );
  assert.match(
    msgDialog,
    /m_footer_content_sizer = new wxBoxSizer\(wxVERTICAL\)/
  );
  assert.match(
    msgDialog,
    /m_dsa_row_sizer->Add\(m_dsa_sizer, 1, wxEXPAND\)/
  );
  assert.match(msgDialog, /m_text_dsa->SetLabel\(m_dsa_text\)/);
  assert.match(msgDialog, /m_text_dsa->Wrap\(text_budget\)/);
  assert.match(msgDialog, /MD3Dialog::on_dpi_changed\(suggested_rect\)/);
  assert.match(msgDialog, /btn->SetMinSize\(FromDIP\(wxSize\(44, 42\)\)\)/);
  assert.match(msgDialog, /btn->SetAllowShrink\(true\)/);
  assert.match(msgDialogHeader, /wxFlexGridSizer \*m_action_sizer/);
});
