// Funny-level step of the first-run guide. The wording and routing live in
// funny-disclosure.js; this file talks to the application and fills the page.
var m_FunnyState = null;
var m_FunnyRegion = null;

function OnInit()
{
	TranslatePage();

	m_FunnyRegion = GetQueryString('region');

	$('#FunnyEnglishRange').on('input', function () { PreviewFunnyLevel('english', this.value); })
		.on('change', function () { SaveFunnyLevel('en', this.value); });
	$('#FunnyCantoneseRange').on('input', function () { PreviewFunnyLevel('cantonese', this.value); })
		.on('change', function () { SaveFunnyLevel('yue', this.value); });
	$('#FunnyEnglishReset').on('click', function () { ResetFunnyLevel('en'); });
	$('#FunnyCantoneseReset').on('click', function () { ResetFunnyLevel('yue'); });

	if (IsInSlicer() == null) {
		// Opened outside the application: the facts still stand, with the
		// shipped defaults; the controls need the application and stay hidden.
		RenderFunnyDisclosure(FunnyDisclosureState(null));
		return;
	}

	SendFunnyMessage({ 'command': 'request_funny_disclosure' });
}

function SendFunnyMessage(message)
{
	message['sequence_id'] = Math.round(new Date() / 1000);
	SendWXMessage(JSON.stringify(message));
}

function HandleStudio(pVal)
{
	if (!pVal || pVal['command'] !== 'response_funny_disclosure')
		return;

	let state = FunnyDisclosureState(pVal);
	if (!state.available) {
		// School mode: funny levels are not installed, so this step does not
		// exist. Leave in the direction the visitor was travelling.
		window.location.replace(FunnyDisclosureRoute(GetQueryString('dir') === 'back' ? 'back' : 'next', m_FunnyRegion));
		return;
	}
	RenderFunnyDisclosure(state);
}

function RenderFunnyDisclosure(state)
{
	m_FunnyState = state;
	let copy = BuildFunnyDisclosureCopy(state);

	document.title = copy.titlePlain;
	$('#FunnyTitle').html(copy.title).prop('hidden', false);
	$('#FunnyIntro').html(copy.intro);
	$('#FunnyFactAffects').html(copy.facts[0]);
	$('#FunnyFactDefaults').html(copy.facts[1]);
	$('#FunnyFactChange').html(copy.facts[2]);

	RenderFunnyRow('English', copy.english);
	RenderFunnyRow('Cantonese', copy.cantonese);
	$('#FunnyControls').prop('hidden', !copy.controls);
	$('#FunnyDisclosure').prop('hidden', false);
}

function RenderFunnyRow(side, row)
{
	$('#Funny' + side + 'Label').html(row.label);
	let range = $('#Funny' + side + 'Range');
	range.attr('min', FUNNY_DISCLOSURE_MIN_LEVEL).attr('max', FUNNY_DISCLOSURE_MAX_LEVEL);
	range.val(row.level).attr('aria-valuetext', row.valuePlain);
	$('#Funny' + side + 'Value').html(row.value);
	$('#Funny' + side + 'Reset').html(row.reset);

	let sample = $('#Funny' + side + 'Sample');
	sample.find('.FunnySampleLabel').html(row.sampleLabel);
	// The sample is a real message from the application; show it as text.
	sample.find('.FunnySampleText').text(row.sample);
	sample.prop('hidden', row.sample === '');
}

// While a slider moves, its value reads back at once; the level is saved, and
// the opening line and sample follow, when the move ends.
function PreviewFunnyLevel(side, value)
{
	if (!m_FunnyState)
		return;
	let level = FunnyDisclosureClampLevel(value);
	let name = side === 'english' ? 'English' : 'Cantonese';
	$('#Funny' + name + 'Range').attr('aria-valuetext', FunnyDisclosurePlainText(FUNNY_DISCLOSURE_KEYS.levelValue, { level: level }));
	$('#Funny' + name + 'Value').html(FunnyDisclosureText(FUNNY_DISCLOSURE_KEYS.levelValue, GetCurrentWebLang(), { level: level }));
}

function SaveFunnyLevel(language, value)
{
	SendFunnyMessage({ 'command': 'save_funny_level', 'language': language, 'level': FunnyDisclosureClampLevel(value) });
}

function ResetFunnyLevel(language)
{
	SendFunnyMessage({ 'command': 'reset_funny_level', 'language': language });
}

function GotoPreviousPage()
{
	window.location.href = FunnyDisclosureRoute('back', m_FunnyRegion);
}

function GotoNextPage()
{
	if (m_FunnyState && m_FunnyState.available)
		SendFunnyMessage({ 'command': 'acknowledge_funny_disclosure' });
	window.location.href = FunnyDisclosureRoute('next', m_FunnyRegion);
}
