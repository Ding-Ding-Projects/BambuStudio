// Funny-level step of the first-run guide: copy and routing only, no DOM, so
// the same functions drive the page (12.js) and the tests. Every word comes
// from the web text catalog (../../data/text.js); this file only chooses keys,
// fills the {placeholders} and composes the language modes.
//
// The level ranges mirror I18N::FUNNY_LEVEL_MIN/MAX/DEFAULT in
// src/slic3r/GUI/LanguageMode.hpp. The application sends the live values; these
// are the fallback when the page runs without the application.
var FUNNY_DISCLOSURE_MIN_LEVEL = 1;
var FUNNY_DISCLOSURE_MAX_LEVEL = 5;
var FUNNY_DISCLOSURE_DEFAULT_LEVEL = 5;

// Title, the three facts, and the per-language controls.
var FUNNY_DISCLOSURE_KEYS = {
	title: 't300',
	affects: 't304',
	defaults: 't305',
	changeHere: 't306',
	changeLater: 't307',
	englishLabel: 't308',
	cantoneseLabel: 't309',
	levelValue: 't310',
	resetEnglish: 't311',
	resetCantonese: 't312',
	sample: 't313'
};

// The opening line is itself styled by the funny level, like every other
// message: levels 1-2, level 3, levels 4-5. The facts above never are.
var FUNNY_DISCLOSURE_INTRO_LADDER = ['t301', 't302', 't303'];

var FUNNY_DISCLOSURE_BILINGUAL = 'bilingual_en_yue_HK';

function FunnyDisclosureClampLevel(level)
{
	let n = parseInt(level, 10);
	if (isNaN(n))
		return FUNNY_DISCLOSURE_DEFAULT_LEVEL;
	return Math.max(FUNNY_DISCLOSURE_MIN_LEVEL, Math.min(FUNNY_DISCLOSURE_MAX_LEVEL, n));
}

// Same expansion as funny_ladder_index() in LanguageMode.cpp and the site's
// copy.js: 5 entries map one to one, 3 entries cover 1-2 / 3 / 4-5, 2 entries
// cover 1-2 / 3-5, and 1 entry is the same at every level.
function FunnyDisclosureLadderIndex(size, level)
{
	let n = FunnyDisclosureClampLevel(level);
	if (size === 5)
		return n - 1;
	if (size === 3)
		return n <= 2 ? 0 : (n === 3 ? 1 : 2);
	if (size === 2)
		return n <= 2 ? 0 : 1;
	return 0;
}

function FunnyDisclosureFill(text, values)
{
	let out = String(text);
	for (let name in values) {
		if (Object.prototype.hasOwnProperty.call(values, name))
			out = out.split('{' + name + '}').join(String(values[name]));
	}
	return out;
}

// Text for one language, falling back to English when the catalog has no
// real entry for it.
function FunnyDisclosureCatalogText(key, strLang)
{
	if (LangText.hasOwnProperty(strLang) && LangText[strLang].hasOwnProperty(key) && LangText[strLang][key] !== '')
		return LangText[strLang][key];
	return LangText['en'].hasOwnProperty(key) ? LangText['en'][key] : '';
}

// One catalog key in the current language mode, as HTML, with placeholders filled.
function FunnyDisclosureText(key, strLang, values)
{
	return FunnyDisclosureFill(GetLocalizedTextByKey(key, strLang) || '', values || {});
}

// One catalog key as plain text in the current language mode (accessible
// values, the window title), with placeholders filled.
function FunnyDisclosurePlainText(key, values)
{
	return FunnyDisclosureFill(GetCurrentPlainTextByKey(key), values || {});
}

// A ladder line where each language follows its own level: English text at the
// English level, Cantonese text at the Cantonese level. Languages other than
// English and Cantonese read the English ladder at the English level.
function FunnyDisclosureLadderText(keys, strLang, englishLevel, cantoneseLevel)
{
	let englishKey = keys[FunnyDisclosureLadderIndex(keys.length, englishLevel)];
	let cantoneseKey = keys[FunnyDisclosureLadderIndex(keys.length, cantoneseLevel)];
	let english = FunnyDisclosureCatalogText(englishKey, 'en');

	if (strLang === FUNNY_DISCLOSURE_BILINGUAL) {
		let cantonese = LangText['yue_HK'].hasOwnProperty(cantoneseKey) ? LangText['yue_HK'][cantoneseKey] : '';
		// Same fallback rule as the rest of the guide: no real Cantonese line,
		// no annotation.
		if (cantonese === '' || cantonese === english)
			return english;
		return '<span lang="en">' + english + '</span>' + BuildBilingualSecondarySpan(cantonese, '');
	}
	if (strLang === 'yue_HK')
		return FunnyDisclosureCatalogText(cantoneseKey, 'yue_HK');
	return FunnyDisclosureCatalogText(englishKey, strLang);
}

// Normalizes the application's response_funny_disclosure payload. A missing
// or partial payload (the page opened outside the application) keeps the
// facts, reports the shipped defaults, and hides the controls.
function FunnyDisclosureState(payload)
{
	let fromHost = !!payload && typeof payload === 'object';
	let state = {
		available: !fromHost || payload['available'] !== false,
		controls: false,
		defaultLevel: FUNNY_DISCLOSURE_DEFAULT_LEVEL,
		english: { level: FUNNY_DISCLOSURE_DEFAULT_LEVEL, sample: '' },
		cantonese: { level: FUNNY_DISCLOSURE_DEFAULT_LEVEL, sample: '' }
	};
	if (!fromHost || !state.available)
		return state;

	let en = payload['en'];
	let yue = payload['yue'];
	if (en && yue && typeof en === 'object' && typeof yue === 'object') {
		state.controls = true;
		state.defaultLevel = FunnyDisclosureClampLevel(payload['default']);
		state.english = { level: FunnyDisclosureClampLevel(en['level']), sample: String(en['sample'] || '') };
		state.cantonese = { level: FunnyDisclosureClampLevel(yue['level']), sample: String(yue['sample'] || '') };
	}
	return state;
}

// Everything the step shows in the current language mode (English, Cantonese
// or bilingual; any other guide language falls back to English) for the given
// state. The facts never depend on the current levels; only the opening line does.
function BuildFunnyDisclosureCopy(state)
{
	let strLang = GetCurrentWebLang();
	let k = FUNNY_DISCLOSURE_KEYS;
	let values = { 'default': state.defaultLevel };
	let row = function (side, labelKey, resetKey) {
		let level = state[side].level;
		return {
			label: FunnyDisclosureText(labelKey, strLang),
			labelPlain: FunnyDisclosurePlainText(labelKey),
			level: level,
			value: FunnyDisclosureText(k.levelValue, strLang, { level: level }),
			valuePlain: FunnyDisclosurePlainText(k.levelValue, { level: level }),
			reset: FunnyDisclosureText(resetKey, strLang, values),
			sampleLabel: FunnyDisclosureText(k.sample, strLang),
			sample: state[side].sample
		};
	};
	return {
		lang: strLang,
		title: FunnyDisclosureText(k.title, strLang),
		titlePlain: FunnyDisclosurePlainText(k.title),
		intro: FunnyDisclosureLadderText(FUNNY_DISCLOSURE_INTRO_LADDER, strLang, state.english.level, state.cantonese.level),
		facts: [
			FunnyDisclosureText(k.affects, strLang),
			FunnyDisclosureText(k.defaults, strLang, values),
			FunnyDisclosureText(state.controls ? k.changeHere : k.changeLater, strLang)
		],
		controls: state.controls,
		english: row('english', k.englishLabel, k.resetEnglish),
		cantonese: row('cantonese', k.cantoneseLabel, k.resetCantonese)
	};
}

// Where Back and Next lead. The step sits between the region page (11) and
// the experience programme page (3); the region chosen on 11 travels on to 3.
function FunnyDisclosureRoute(direction, region)
{
	if (direction === 'back')
		return '../11/index.html';
	let target = '../3/index.html';
	if (region)
		target += '?region=' + encodeURIComponent(region);
	return target;
}
