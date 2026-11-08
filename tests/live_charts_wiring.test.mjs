// Source checks for the live charts page (docs/features/device/live-charts.md):
//
// - The recorder samples the selected printer from startup, whichever page is
//   open, and stops when the app closes. It keeps AMS humidity per unit (the
//   percentage when the unit sends one, otherwise its level, never mixing the
//   two) and the part cooling fan as the percentage the Device page shows.
// - The chart options are read with their saved keys, the clock follows
//   Windows until one is chosen, and the time on the axis, under the
//   crosshair and in the stalled note goes through the chosen clock.
// - New highs pass through the watch; particles never fly with reduced motion
//   (a still note shows instead), and the arrow sits outside the readings.
// - The right-click menu opens Interface motion, and Every feature opens the
//   charts straight on AMS humidity or the part cooling fan.
//
// Each checker also runs against a broken copy of the source and must fail.
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import test from 'node:test';
import {fileURLToPath} from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const read = (file) => fs.readFileSync(path.join(root, file), 'utf8').replaceAll('\r\n', '\n');
const app = read('src/slic3r/GUI/GUI_App.cpp');
const panel = read('src/slic3r/GUI/LiveCharts/LiveChartsPanel.cpp');
const mainframe = read('src/slic3r/GUI/MainFrame.cpp');
const catalogue = read('src/slic3r/GUI/FeatureFinder/FeatureCatalogue.cpp');
const prefs = read('src/slic3r/GUI/Preferences.cpp');

function body(source, name) {
    const start = source.indexOf(name);
    assert(start >= 0, name);
    const begin = source.indexOf('{', start);
    const masked = source.slice(begin).replace(/\/\*[\s\S]*?\*\/|\/\/[^\n]*|"(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*'/g, (t) => ' '.repeat(t.length));
    let depth = 0;
    for (let i = 0; i < masked.length; ++i) {
        if (masked[i] === '{') ++depth;
        if (masked[i] === '}' && --depth === 0) return source.slice(start, begin + i + 1);
    }
    assert.fail(`Unbalanced ${name}`);
}
const code = (text) => text.replace(/\/\/[^\n]*/g, '');

function checkSampling(appSource, panelSource) {
    assert.match(code(appSource), /ThemeFollow::start_watching\(\);\s*LiveCharts::Recorder::instance\(\)\.start_sampling\(\);/,
                 'sampling starts with the app');
    assert.match(code(appSource), /ThemeFollow::stop_watching\(\);\s*LiveCharts::Recorder::instance\(\)\.stop_sampling\(\);/,
                 'sampling stops before the app goes');
    const notify = code(body(panelSource, 'void Notify() override'));
    assert.match(notify, /dm->get_selected_machine\(\);\s*if \(obj && obj->is_info_ready\(\)\) Recorder::instance\(\)\.record\(obj\);/);
    assert.match(code(body(panelSource, 'void Recorder::start_sampling(')), /if \(m_sampler\) return;[\s\S]*m_sampler->Start\(500\);/);
}

function checkRecorder(panelSource) {
    const record = code(body(panelSource, 'void Recorder::record('));
    assert.match(record, /if \(push <= 0 \|\| push <= d\.last_push\) return;/, 'a reading only for a new push');
    assert.match(record, /ams->SupportHumidityPercent\(\) && ams->GetHumidityPercent\(\) >= 0/, 'the percentage first');
    assert.match(record, /ams->SupportHumidityLevel\(\) && ams->GetHumidityLevel\(\) >= 1 && ams->GetHumidityLevel\(\) <= 5/, 'then the level');
    assert.match(record, /if \(!unit\.series\.readings\(\)\.empty\(\) && unit\.percent != percent\) unit\.series\.clear\(\);/, 'never mixes the scales');
    assert.match(record, /FanMotionMath::percent_from_pwm\(fan->GetCoolingFanSpeed\(\)\)/, 'the fan as the Device page shows it');
}

function checkOptionsAndClock(panelSource) {
    assert.match(panelSource,
                 /options_from\(cfg->get\(kReferenceKey\), cfg->get\(kMomentumKey\), cfg->get\(kParticlesKey\), cfg->get\(kClockKey\), windows_twelve_hour\(\)\)/);
    assert.match(code(body(panelSource, 'bool windows_twelve_hour(')), /LOCALE_STIMEFORMAT[\s\S]*twelve_hour_format\(/);
    const time = code(body(panelSource, 'wxString LiveChartsPanel::time_text('));
    assert.match(time, /clock_face\(when\.GetHour\(\), when\.GetMinute\(\), when\.GetSecond\(\), m_options\.twelve_hour, seconds\)/);
    assert.match(time, /face\.pm \? _L\("%s PM"\) : _L\("%s AM"\)/);
    assert.match(code(body(panelSource, 'void on_paint(wxPaintEvent &)')), /m_owner->time_text\(t, seconds\)/, 'the axis uses the clock');
    const refresh = code(body(panelSource, 'void LiveChartsPanel::refresh()'));
    assert.match(refresh, /time_text\(last->t, true\)/, 'the stalled note uses the clock');
    assert.match(refresh, /time_text\(shown\[std::size_t\(cross\)\]\.t, true\)/, 'the crosshair uses the clock');
    for (const key of ['kReferenceKey', 'kMomentumKey', 'kParticlesKey'])
        assert.match(panelSource, new RegExp(`set_option\\(${key}, on\\)`), `${key} is saved from its switch`);
    assert.match(code(body(panelSource, 'void LiveChartsPanel::set_clock(')), /cfg->set\(kClockKey, twelve_hour \? "12" : "24"\)/);
}

function checkHighsAndMotion(panelSource) {
    const data = code(body(panelSource, 'void set_data(ChartView view)'));
    assert.match(data, /if \(m_highs\.see\(r\)\) burst = true;/, 'every new reading passes the watch');
    assert.match(data, /if \(!m_view\.particles \|\| m_view\.reduced\) m_particles\.clear\(\);/, 'no particles with reduced motion');
    assert.match(data, /if \(m_view\.reduced\) \{\s*m_high_note_until = now_seconds\(\) \+ 3;/, 'a still note instead');
    assert.match(data, /m_highs\.prime\(m_view\.readings\);/, 'what was already shown never bursts');
    assert.match(code(body(panelSource, 'bool LiveChartsPanel::reduced(')), /m_preview_reduced \|\| MD3::Motion::reduced\(\)/);
    const plot = code(body(panelSource, 'wxRect plot() const'));
    assert.match(plot, /m_view\.momentum && !m_view\.candles \? 44 : 14/, 'room for the arrow beside the newest point');
}

function checkEntries(panelSource, mainframeSource, catalogueSource, prefsSource) {
    const menu = code(body(panelSource, 'void LiveChartsPanel::show_menu('));
    assert.match(menu, /_L\("Edit appearance, including Animation and transition"\)/);
    assert.match(menu, /else if \(chosen == motion\) open_motion_settings\(\);/);
    assert.match(code(body(panelSource, 'void LiveChartsPanel::open_motion_settings(')), /open_preferences\("motion_preference"\)/);
    assert.match(prefsSource, /"motion_preference", \{_L\("System"\), _L\("Reduce motion"\)\}/, 'the motion row exists');
    assert.match(mainframeSource, /Commands::instance\(\)\.add\("live-charts-humidity", \[\]\(\) \{ LiveCharts::LiveChartsPanel::open\(int\(LiveCharts::Kind::Humidity\)\); \}\);/);
    assert.match(mainframeSource, /Commands::instance\(\)\.add\("live-charts-fan", \[\]\(\) \{ LiveCharts::LiveChartsPanel::open\(int\(LiveCharts::Kind::Fan\)\); \}\);/);
    assert.match(catalogueSource, /\{"live-humidity", "look", 0xF87E \/\* humidity_percentage \*\/[^\n]*"command:live-charts-humidity"\}/);
    assert.match(catalogueSource, /\{"live-fan", "look", 0xF168 \/\* mode_fan \*\/[^\n]*"command:live-charts-fan"\}/);
    const open = code(body(panelSource, 'void LiveChartsPanel::open('));
    assert.match(open, /dynamic_cast<LiveChartsPanel \*>\(page\); charts && kind >= 0 && kind < int\(Kind::Count\)\) charts->set_kind\(kind\);/);
}

test('the recorder samples from startup and stops at exit', () => checkSampling(app, panel));
test('AMS humidity and the part fan are recorded honestly', () => checkRecorder(panel));
test('chart options are saved and the clock is used everywhere', () => checkOptionsAndClock(panel));
test('new highs and reduced motion', () => checkHighsAndMotion(panel));
test('the motion settings and the direct entries', () => checkEntries(panel, mainframe, catalogue, prefs));

test('each checker fails on a broken copy', () => {
    assert.throws(() => checkSampling(app.replace('LiveCharts::Recorder::instance().start_sampling();', ''), panel));
    assert.throws(() => checkRecorder(panel.replace('unit.series.clear();', '(void) 0;')));
    assert.throws(() => checkOptionsAndClock(panel.replace('m_options.twelve_hour, seconds)', 'false, seconds)')));
    assert.throws(() => checkHighsAndMotion(panel.replace('if (!m_view.particles || m_view.reduced) m_particles.clear();', 'if (!m_view.particles) m_particles.clear();')));
    assert.throws(() => checkEntries(panel, mainframe.replace('"live-charts-fan"', '"live-charts-fans"'), catalogue, prefs));
});
