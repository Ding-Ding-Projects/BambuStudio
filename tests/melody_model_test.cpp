// Standalone assertions for src/slic3r/GUI/Melody/MelodyModel.hpp, the model
// behind the Melody creator: the M1006 tune block the printer plays, its
// import, the note names, the text notation, the JSON file and the preview
// rendering. Build without NDEBUG (this file undefines it), for example:
//   g++ -std=c++17 -Wall -Wextra -Werror tests/melody_model_test.cpp -o mmt && ./mmt
// tests/melody_model.test.mjs compiles and runs it in the contract gate.
#undef NDEBUG
#include "../src/slic3r/GUI/Melody/MelodyModel.hpp"
#include <cassert>
#include <cstring>
#include <iostream>
#include <string>

using namespace Slic3r::GUI;
static int assertions = 0;
#define CHECK(expr) do { assert(expr); ++assertions; } while (0)

// The first and last note lines of the stock X1 start tune, as the template carries them.
static const char *X1_START =
    ";===== start printer sound ================\nM17\nM400 S1\nM1006 S1\n"
    "M1006 A0 B10 L100 C37 D10 M60 E37 F10 N60\n"
    "M1006 A46 B10 L100 C43 D10 M70 E39 F10 N100\n"
    "M1006 A0 B10 L100 C0 D10 M60 E0 F10  N100\n" // the template has a double space here
    "M1006 W\n;===== turn on the HB fan =================\n";

static void import_assertions()
{
    Melody m;
    CHECK(melody_parse_gcode(X1_START, m));
    CHECK(m.steps.size() == 3);
    CHECK(m.steps[0].voices[0] == (MelodyVoice{0, 10, 100}));
    CHECK(m.steps[0].voices[1] == (MelodyVoice{37, 10, 60}));
    CHECK(m.steps[0].voices[2] == (MelodyVoice{37, 10, 60}));
    CHECK(m.steps[1].voices[0] == (MelodyVoice{46, 10, 100}));
    CHECK(m.steps[1].voices[1] == (MelodyVoice{43, 10, 70}));
    CHECK(m.steps[1].voices[2] == (MelodyVoice{39, 10, 100}));
    CHECK(m.steps[2].voices[2] == (MelodyVoice{0, 10, 100}));
    // Nothing to import.
    Melody none;
    CHECK(!melody_parse_gcode("G28\nM1006 S1\nM1006 W\n", none));
    CHECK(none.steps.empty());
    // The name line comes back.
    Melody named;
    CHECK(melody_parse_gcode("; melody name: Hello tune \nM1006 A60 B10 L100 C0 D10 M0 E0 F10 N0\n", named));
    CHECK(named.name == "Hello tune");
}

static void gcode_assertions()
{
    Melody m;
    m.name = "Two notes";
    MelodyStep s1; s1.voices[0] = {60, 10, 100}; s1.voices[1] = {64, 10, 80}; s1.voices[2] = {0, 10, 0};
    MelodyStep s2; s2.voices[0] = {67, 20, 100}; s2.voices[1] = {0, 20, 0};  s2.voices[2] = {0, 20, 0};
    m.steps = {s1, s2};
    const std::string g = melody_to_gcode(m);
    CHECK(g ==
          ";===== melody start =====\n; melody name: Two notes\nM17\nM400 S1\nM1006 S1\n"
          "M1006 A60 B10 L100 C64 D10 M80 E0 F10 N0\n"
          "M1006 A67 B20 L100 C0 D20 M0 E0 F20 N0\n"
          "M1006 W\nM18\n;===== melody end =====\n");
    // Round trip through the importer.
    Melody back;
    CHECK(melody_parse_gcode(g, back));
    back.tick_ms = m.tick_ms;
    CHECK(back == m);
    // The block replaces an older one or is appended.
    std::string start = "G28\n;===== melody start =====\nM1006 A1 B1 L1 C1 D1 M1 E1 F1 N1\n;===== melody end =====\nM190 S60\n";
    CHECK(melody_insert_block(start, g));
    CHECK(start == "G28\n" + g + "M190 S60\n");
    std::string plain = "G28\nM190 S60";
    CHECK(!melody_insert_block(plain, g));
    CHECK(plain == "G28\nM190 S60\n" + g);
}

static void note_assertions()
{
    CHECK(melody_note_name(60) == "C4");
    CHECK(melody_note_name(61) == "C#4");
    CHECK(melody_note_name(69) == "A4");
    CHECK(melody_note_name(37) == "C#2");
    CHECK(melody_note_name(0).empty());
    CHECK(melody_note_number("C4") == 60);
    CHECK(melody_note_number("c#4") == 61);
    CHECK(melody_note_number("Db4") == 61);
    CHECK(melody_note_number("A4") == 69);
    CHECK(melody_note_number("r") == 0);
    CHECK(melody_note_number("") == 0);
    CHECK(melody_note_number("H4") == -1);
    CHECK(melody_note_number("C") == -1);
    CHECK(melody_note_number("C4x") == -1);
    CHECK(melody_note_number("C-2") == -1);
    for (int p = 1; p <= MELODY_MAX_PITCH; ++p)
        CHECK(melody_note_number(melody_note_name(p)) == p);
    CHECK(std::abs(melody_frequency(69) - 440.0) < 1e-9);
    CHECK(std::abs(melody_frequency(81) - 880.0) < 1e-9);
}

static void notation_assertions()
{
    Melody m;
    CHECK(melody_parse_notation("c4 d4/8 r/5 g#4/20", m) == 0);
    CHECK(m.steps.size() == 4);
    CHECK(m.steps[0].voices[0] == (MelodyVoice{60, 10, 100}));
    CHECK(m.steps[1].voices[0] == (MelodyVoice{62, 8, 100}));
    CHECK(m.steps[2].voices[0] == (MelodyVoice{0, 5, 0}));
    CHECK(m.steps[3].voices[0] == (MelodyVoice{68, 20, 100}));
    CHECK(m.steps[3].voices[1] == (MelodyVoice{0, 20, 0}));
    // The number of the first token that cannot be read.
    CHECK(melody_parse_notation("c4 x4 e4", m) == 2);
    CHECK(melody_parse_notation("c4/0", m) == 1);
    CHECK(melody_parse_notation("c4/300", m) == 1);
    CHECK(melody_parse_notation("", m) == 0 && m.steps.empty());
}

static void validation_assertions()
{
    Melody m;
    CHECK(melody_validate(m) == MelodyProblem::NoSteps);
    MelodyStep s; s.voices[0] = {60, 10, 100};
    m.steps.push_back(s);
    CHECK(melody_validate(m) == MelodyProblem::None);
    m.steps[0].voices[1].pitch = 128;
    CHECK(melody_validate(m) == MelodyProblem::PitchOutOfRange);
    m.steps[0].voices[1].pitch = 0;
    m.steps[0].voices[2].ticks = 256;
    CHECK(melody_validate(m) == MelodyProblem::TicksOutOfRange);
    m.steps[0].voices[2].ticks = 10;
    m.steps[0].voices[0].volume = 101;
    CHECK(melody_validate(m) == MelodyProblem::VolumeOutOfRange);
    m.steps[0].voices[0].volume = 100;
    m.tick_ms = 1;
    CHECK(melody_validate(m) == MelodyProblem::TickLengthOutOfRange);
    m.tick_ms = 50;
    m.name.assign(MELODY_MAX_NAME + 1, 'x');
    CHECK(melody_validate(m) == MelodyProblem::NameTooLong);
    m.name = "ok";
    m.steps.assign(MELODY_MAX_STEPS + 1, s);
    CHECK(melody_validate(m) == MelodyProblem::TooManySteps);
    m.steps.assign(MELODY_MAX_STEPS, s);
    CHECK(melody_validate(m) == MelodyProblem::None);
}

static void json_assertions()
{
    Melody m;
    m.name = "Quote \" and \\ slash\tTab";
    m.tick_ms = 75;
    MelodyStep s1; s1.voices[0] = {60, 10, 100}; s1.voices[1] = {64, 12, 80}; s1.voices[2] = {67, 14, 60};
    MelodyStep s2; s2.voices[0] = {0, 5, 0};
    m.steps = {s1, s2};
    const std::string text = melody_to_json(m);
    CHECK(text.find("\"schema\": \"bambustudio-melody/1\"") != std::string::npos);
    Melody back;
    CHECK(melody_from_json(text, back));
    CHECK(back == m);
    // Key order and whitespace are free, unknown keys are skipped.
    Melody loose;
    CHECK(melody_from_json("{\"steps\":[[[1,2,3],[4,5,6],[7,8,9]]],\"extra\":{\"a\":[1,2,{\"b\":\"c\"}]},\"name\":\"x\",\"tick_ms\":20}", loose));
    CHECK(loose.name == "x" && loose.tick_ms == 20 && loose.steps.size() == 1);
    CHECK(loose.steps[0].voices[2] == (MelodyVoice{7, 8, 9}));
    // Malformed input leaves the output untouched.
    Melody untouched = m;
    CHECK(!melody_from_json("{\"name\":\"x\"}", untouched));           // no steps
    CHECK(!melody_from_json("{\"steps\":[[[1,2],[4,5,6],[7,8,9]]]}", untouched));
    CHECK(!melody_from_json("not json", untouched));
    CHECK(!melody_from_json("", untouched));
    CHECK(untouched == m);
}

static void preview_assertions()
{
    Melody m;
    m.tick_ms = 100;
    MelodyStep s1; s1.voices[0] = {69, 10, 100}; s1.voices[1] = {0, 5, 0}; s1.voices[2] = {0, 5, 0};
    MelodyStep s2; s2.voices[0] = {0, 5, 0};     s2.voices[1] = {0, 5, 0}; s2.voices[2] = {0, 5, 0};
    m.steps = {s1, s2};
    CHECK(melody_step_ms(s1, 100) == 1000);
    CHECK(melody_total_ms(m) == 1500);
    const auto wav = melody_render_wav(m, 8000);
    CHECK(wav.size() == 44 + 2 * (8000 + 4000));
    CHECK(std::memcmp(wav.data(), "RIFF", 4) == 0 && std::memcmp(wav.data() + 8, "WAVE", 4) == 0);
    CHECK(std::memcmp(wav.data() + 36, "data", 4) == 0);
    const std::uint32_t data_size = wav[40] | (wav[41] << 8) | (wav[42] << 16) | (wav[43] << 24);
    CHECK(data_size == 2 * 12000);
    // The note plays, the rest is silent.
    CHECK(wav[44] != 0 || wav[45] != 0);
    CHECK(wav[44 + 2 * 8000 + 100] == 0 && wav[44 + 2 * 8000 + 101] == 0);
    CHECK(melody_render_wav(Melody{}, 8000).size() == 44);
}

static void preset_assertions()
{
    const auto &presets = melody_presets();
    CHECK(presets.size() == 7);
    for (const MelodyPreset &preset : presets) {
        const Melody m = melody_from_preset(preset);
        CHECK(!m.steps.empty());
        CHECK(melody_validate(m) == MelodyProblem::None);
        CHECK(m.name == preset.name);
    }
    CHECK(melody_from_preset(presets[0]).steps.size() == 17); // X1 start
    CHECK(melody_from_preset(presets[4]).steps.size() == 6);  // H2D start
    CHECK(melody_from_preset(presets[6]).steps.size() == 8);  // scale
}

int main()
{
    import_assertions();
    gcode_assertions();
    note_assertions();
    notation_assertions();
    validation_assertions();
    json_assertions();
    preview_assertions();
    preset_assertions();
    std::cout << assertions << " melody model assertions passed\n";
    return 0;
}
