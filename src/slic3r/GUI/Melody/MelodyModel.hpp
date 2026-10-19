#ifndef slic3r_GUI_MelodyModel_hpp_
#define slic3r_GUI_MelodyModel_hpp_

// The Melody creator's model: a tune the printer plays through M1006, kept
// free of wxWidgets and of the rest of the application so that
// tests/melody_model_test.cpp can execute it on its own.
//
// Bambu Lab printers play their start and finish jingles with lines such as
//   M1006 A0 B10 L100 C37 D10 M60 E37 F10 N60
// between `M1006 S1` and `M1006 W`. Read against the stock tunes, each line is
// one time step with three voices: (A, B, L), (C, D, M) and (E, F, N) are the
// pitch, the duration in ticks and the volume (0-100) of a voice, and a pitch
// of 0 keeps that voice silent. Nothing documents the command, so the note
// numbering (MIDI-style, 69 = A4 = 440 Hz) and the tick length are inferred;
// the tick length is a user setting of the melody, used only for the preview.

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <sstream>
#include <string>
#include <vector>

namespace Slic3r { namespace GUI {

struct MelodyVoice
{
    int pitch{0};    // 0 = silent, otherwise a note number
    int ticks{10};   // duration in firmware ticks
    int volume{100}; // 0-100

    bool operator==(const MelodyVoice &o) const { return pitch == o.pitch && ticks == o.ticks && volume == o.volume; }
    bool operator!=(const MelodyVoice &o) const { return !(*this == o); }
};

struct MelodyStep
{
    std::array<MelodyVoice, 3> voices{};

    bool operator==(const MelodyStep &o) const { return voices == o.voices; }
    bool operator!=(const MelodyStep &o) const { return !(*this == o); }
};

struct Melody
{
    std::string             name;
    int                     tick_ms{50}; // preview only: milliseconds per tick
    std::vector<MelodyStep> steps;

    bool operator==(const Melody &o) const { return name == o.name && tick_ms == o.tick_ms && steps == o.steps; }
    bool operator!=(const Melody &o) const { return !(*this == o); }
};

constexpr int    MELODY_MAX_STEPS   = 256;
constexpr int    MELODY_MAX_PITCH   = 127;
constexpr int    MELODY_MAX_TICKS   = 255;
constexpr int    MELODY_MAX_VOLUME  = 100;
constexpr int    MELODY_MIN_TICK_MS = 5;
constexpr int    MELODY_MAX_TICK_MS = 1000;
constexpr size_t MELODY_MAX_NAME    = 64;
inline const char *MELODY_START_MARKER = ";===== melody start =====";
inline const char *MELODY_END_MARKER   = ";===== melody end =====";
inline const char *MELODY_NAME_PREFIX  = "; melody name: ";

enum class MelodyProblem
{
    None,
    NoSteps,
    TooManySteps,
    NameTooLong,
    PitchOutOfRange,
    TicksOutOfRange,
    VolumeOutOfRange,
    TickLengthOutOfRange,
};

inline MelodyProblem melody_validate(const Melody &melody)
{
    if (melody.steps.empty()) return MelodyProblem::NoSteps;
    if (melody.steps.size() > static_cast<size_t>(MELODY_MAX_STEPS)) return MelodyProblem::TooManySteps;
    if (melody.name.size() > MELODY_MAX_NAME) return MelodyProblem::NameTooLong;
    if (melody.tick_ms < MELODY_MIN_TICK_MS || melody.tick_ms > MELODY_MAX_TICK_MS) return MelodyProblem::TickLengthOutOfRange;
    for (const MelodyStep &step : melody.steps)
        for (const MelodyVoice &voice : step.voices) {
            if (voice.pitch < 0 || voice.pitch > MELODY_MAX_PITCH) return MelodyProblem::PitchOutOfRange;
            if (voice.ticks < 0 || voice.ticks > MELODY_MAX_TICKS) return MelodyProblem::TicksOutOfRange;
            if (voice.volume < 0 || voice.volume > MELODY_MAX_VOLUME) return MelodyProblem::VolumeOutOfRange;
        }
    return MelodyProblem::None;
}

// ---- note names -------------------------------------------------------------

inline const char *const *melody_note_names()
{
    static const char *const names[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    return names;
}

// 60 -> "C4", 61 -> "C#4", 0 -> "" (silent).
inline std::string melody_note_name(int pitch)
{
    if (pitch <= 0 || pitch > MELODY_MAX_PITCH) return std::string();
    return std::string(melody_note_names()[pitch % 12]) + std::to_string(pitch / 12 - 1);
}

// "C4" -> 60, "c#4" -> 61, "Db4" -> 61, "r"/"" -> 0 (silent), anything else -> -1.
inline int melody_note_number(const std::string &text)
{
    std::string s;
    for (char c : text)
        if (c != ' ' && c != '\t') s.push_back(c);
    if (s.empty() || s == "r" || s == "R" || s == "-") return 0;
    size_t i = 0;
    const char letter = static_cast<char>(std::toupper(static_cast<unsigned char>(s[i++])));
    static const int base[7] = {9, 11, 0, 2, 4, 5, 7}; // A B C D E F G
    if (letter < 'A' || letter > 'G') return -1;
    int semitone = base[letter - 'A'];
    if (i < s.size() && (s[i] == '#' || s[i] == 's')) { ++semitone; ++i; }
    else if (i < s.size() && (s[i] == 'b' || s[i] == 'f')) { --semitone; ++i; }
    if (i >= s.size()) return -1;
    char *end = nullptr;
    const long octave = std::strtol(s.c_str() + i, &end, 10);
    if (end == nullptr || *end != '\0') return -1;
    const long pitch = (octave + 1) * 12 + semitone;
    if (pitch < 1 || pitch > MELODY_MAX_PITCH) return -1;
    return static_cast<int>(pitch);
}

// ---- G-code -------------------------------------------------------------------

// The block the printer plays: motors on, queue drained, the tune, then the
// motors released again, inside markers the creator finds again on import.
inline std::string melody_to_gcode(const Melody &melody)
{
    std::ostringstream out;
    out << MELODY_START_MARKER << "\n";
    if (!melody.name.empty()) out << MELODY_NAME_PREFIX << melody.name << "\n";
    out << "M17\nM400 S1\nM1006 S1\n";
    for (const MelodyStep &step : melody.steps) {
        const MelodyVoice &a = step.voices[0];
        const MelodyVoice &c = step.voices[1];
        const MelodyVoice &e = step.voices[2];
        out << "M1006 A" << a.pitch << " B" << a.ticks << " L" << a.volume
            << " C" << c.pitch << " D" << c.ticks << " M" << c.volume
            << " E" << e.pitch << " F" << e.ticks << " N" << e.volume << "\n";
    }
    out << "M1006 W\nM18\n" << MELODY_END_MARKER << "\n";
    return out.str();
}

// Reads every `M1006 A.. B.. L.. C.. D.. M.. E.. F.. N..` line of `text` (a
// stock start G-code, a finish G-code or a block written above) into
// `out.steps`; `S1` and `W` lines are skipped. A `; melody name:` line sets
// the name. Returns false when no note line was found.
inline bool melody_parse_gcode(const std::string &text, Melody &out)
{
    out.steps.clear();
    std::istringstream in(text);
    std::string line;
    bool found = false;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        size_t start = line.find_first_not_of(" \t");
        if (start == std::string::npos) continue;
        line = line.substr(start);
        if (line.compare(0, std::char_traits<char>::length(MELODY_NAME_PREFIX), MELODY_NAME_PREFIX) == 0) {
            out.name = line.substr(std::char_traits<char>::length(MELODY_NAME_PREFIX));
            while (!out.name.empty() && (out.name.back() == ' ' || out.name.back() == '\t')) out.name.pop_back();
            continue;
        }
        if (line.compare(0, 5, "M1006") != 0) continue;
        const size_t semicolon = line.find(';');
        if (semicolon != std::string::npos) line = line.substr(0, semicolon);
        int values[9];
        bool have[9] = {false, false, false, false, false, false, false, false, false};
        static const char letters[9] = {'A', 'B', 'L', 'C', 'D', 'M', 'E', 'F', 'N'};
        std::istringstream words(line.substr(5));
        std::string word;
        while (words >> word) {
            if (word.size() < 2) continue;
            const char letter = static_cast<char>(std::toupper(static_cast<unsigned char>(word[0])));
            for (int k = 0; k < 9; ++k)
                if (letters[k] == letter) {
                    char *end = nullptr;
                    const long v = std::strtol(word.c_str() + 1, &end, 10);
                    if (end && *end == '\0') { values[k] = static_cast<int>(v); have[k] = true; }
                }
        }
        bool complete = true;
        for (bool h : have) complete = complete && h;
        if (!complete) continue; // M1006 S1, M1006 W and partial lines
        MelodyStep step;
        step.voices[0] = {values[0], values[1], values[2]};
        step.voices[1] = {values[3], values[4], values[5]};
        step.voices[2] = {values[6], values[7], values[8]};
        out.steps.push_back(step);
        found = true;
    }
    return found;
}

// Replaces the marker block of `gcode` with `block`, or appends `block` when
// there is none. Returns true when a block was replaced.
inline bool melody_insert_block(std::string &gcode, const std::string &block)
{
    const size_t start = gcode.find(MELODY_START_MARKER);
    const size_t end   = start == std::string::npos ? std::string::npos : gcode.find(MELODY_END_MARKER, start);
    if (start != std::string::npos && end != std::string::npos) {
        size_t stop = end + std::char_traits<char>::length(MELODY_END_MARKER);
        if (stop < gcode.size() && gcode[stop] == '\n') ++stop;
        gcode.replace(start, stop - start, block);
        return true;
    }
    if (!gcode.empty() && gcode.back() != '\n') gcode += "\n";
    gcode += block;
    return false;
}

// ---- text notation ------------------------------------------------------------

// A quick way to type a single-voice tune: whitespace-separated notes such as
// `c4 d4/8 e4/20 r/5 g#4`, where `/n` is the duration in ticks (default 10)
// and `r` a rest. Every note goes to the first voice at full volume. Returns
// the number of the first token that could not be read, or 0.
inline int melody_parse_notation(const std::string &text, Melody &out, int default_ticks = 10)
{
    out.steps.clear();
    std::istringstream in(text);
    std::string token;
    int index = 0;
    while (in >> token) {
        ++index;
        std::string note = token;
        int ticks = default_ticks;
        const size_t slash = token.find('/');
        if (slash != std::string::npos) {
            note = token.substr(0, slash);
            char *end = nullptr;
            const long t = std::strtol(token.c_str() + slash + 1, &end, 10);
            if (end == nullptr || *end != '\0' || t < 1 || t > MELODY_MAX_TICKS) return index;
            ticks = static_cast<int>(t);
        }
        const int pitch = melody_note_number(note);
        if (pitch < 0) return index;
        MelodyStep step;
        step.voices[0] = {pitch, ticks, pitch == 0 ? 0 : MELODY_MAX_VOLUME};
        step.voices[1] = {0, ticks, 0};
        step.voices[2] = {0, ticks, 0};
        out.steps.push_back(step);
    }
    return 0;
}

// ---- JSON (own small format, no library needed) ------------------------------

inline std::string melody_json_escape(const std::string &s)
{
    std::string out;
    for (char c : s) {
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (static_cast<unsigned char>(c) < 0x20) {
                char buf[8];
                std::snprintf(buf, sizeof buf, "\\u%04x", static_cast<unsigned>(static_cast<unsigned char>(c)));
                out += buf;
            } else
                out.push_back(c);
        }
    }
    return out;
}

inline std::string melody_to_json(const Melody &melody)
{
    std::ostringstream out;
    out << "{\n  \"schema\": \"bambustudio-melody/1\",\n  \"name\": \"" << melody_json_escape(melody.name) << "\",\n"
        << "  \"tick_ms\": " << melody.tick_ms << ",\n  \"steps\": [\n";
    for (size_t i = 0; i < melody.steps.size(); ++i) {
        out << "    [";
        for (size_t v = 0; v < 3; ++v) {
            const MelodyVoice &voice = melody.steps[i].voices[v];
            out << (v ? ", " : "") << "[" << voice.pitch << ", " << voice.ticks << ", " << voice.volume << "]";
        }
        out << "]" << (i + 1 < melody.steps.size() ? "," : "") << "\n";
    }
    out << "  ]\n}\n";
    return out.str();
}

// Reads what melody_to_json writes (whitespace and key order are free; unknown
// keys are skipped). Returns false on any malformed input.
inline bool melody_from_json(const std::string &text, Melody &out)
{
    size_t pos = 0;
    auto skip = [&]() { while (pos < text.size() && (text[pos] == ' ' || text[pos] == '\n' || text[pos] == '\r' || text[pos] == '\t')) ++pos; };
    auto expect = [&](char c) { skip(); if (pos < text.size() && text[pos] == c) { ++pos; return true; } return false; };
    auto read_string = [&](std::string &s) {
        skip();
        if (pos >= text.size() || text[pos] != '"') return false;
        ++pos;
        s.clear();
        while (pos < text.size() && text[pos] != '"') {
            if (text[pos] == '\\') {
                ++pos;
                if (pos >= text.size()) return false;
                switch (text[pos]) {
                case 'n': s.push_back('\n'); break;
                case 'r': s.push_back('\r'); break;
                case 't': s.push_back('\t'); break;
                case 'u': {
                    if (pos + 4 >= text.size()) return false;
                    const long code = std::strtol(text.substr(pos + 1, 4).c_str(), nullptr, 16);
                    if (code < 0x80) s.push_back(static_cast<char>(code));
                    else if (code < 0x800) { s.push_back(static_cast<char>(0xC0 | (code >> 6))); s.push_back(static_cast<char>(0x80 | (code & 0x3F))); }
                    else { s.push_back(static_cast<char>(0xE0 | (code >> 12))); s.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F))); s.push_back(static_cast<char>(0x80 | (code & 0x3F))); }
                    pos += 4;
                    break;
                }
                default: s.push_back(text[pos]);
                }
            } else
                s.push_back(text[pos]);
            ++pos;
        }
        if (pos >= text.size()) return false;
        ++pos;
        return true;
    };
    auto read_int = [&](int &v) {
        skip();
        const char *begin = text.c_str() + pos;
        char *end = nullptr;
        const long value = std::strtol(begin, &end, 10);
        if (end == begin) return false;
        pos += static_cast<size_t>(end - begin);
        v = static_cast<int>(value);
        return true;
    };
    // Skips one JSON value of any kind (for unknown keys).
    std::function<bool()> skip_value = [&]() -> bool {
        skip();
        if (pos >= text.size()) return false;
        if (text[pos] == '"') { std::string s; return read_string(s); }
        if (text[pos] == '[' || text[pos] == '{') {
            const char close = text[pos] == '[' ? ']' : '}';
            ++pos;
            skip();
            if (pos < text.size() && text[pos] == close) { ++pos; return true; }
            while (true) {
                if (close == '}') { std::string key; if (!read_string(key) || !expect(':')) return false; }
                if (!skip_value()) return false;
                skip();
                if (pos < text.size() && text[pos] == ',') { ++pos; continue; }
                return expect(close);
            }
        }
        while (pos < text.size() && text[pos] != ',' && text[pos] != '}' && text[pos] != ']') ++pos;
        return true;
    };

    Melody result;
    bool have_steps = false;
    if (!expect('{')) return false;
    skip();
    if (pos < text.size() && text[pos] == '}') return false;
    while (true) {
        std::string key;
        if (!read_string(key) || !expect(':')) return false;
        if (key == "name") {
            if (!read_string(result.name)) return false;
        } else if (key == "tick_ms") {
            if (!read_int(result.tick_ms)) return false;
        } else if (key == "steps") {
            if (!expect('[')) return false;
            have_steps = true;
            skip();
            if (pos < text.size() && text[pos] == ']') { ++pos; }
            else
                while (true) {
                    MelodyStep step;
                    if (!expect('[')) return false;
                    for (size_t v = 0; v < 3; ++v) {
                        if (v && !expect(',')) return false;
                        if (!expect('[')) return false;
                        if (!read_int(step.voices[v].pitch) || !expect(',') || !read_int(step.voices[v].ticks) || !expect(',') ||
                            !read_int(step.voices[v].volume) || !expect(']'))
                            return false;
                    }
                    if (!expect(']')) return false;
                    result.steps.push_back(step);
                    skip();
                    if (pos < text.size() && text[pos] == ',') { ++pos; continue; }
                    if (!expect(']')) return false;
                    break;
                }
        } else {
            if (!skip_value()) return false;
        }
        skip();
        if (pos < text.size() && text[pos] == ',') { ++pos; continue; }
        if (!expect('}')) return false;
        break;
    }
    if (!have_steps) return false;
    out = result;
    return true;
}

// ---- preview ------------------------------------------------------------------

// Milliseconds of one step: the longest voice of the step, or 0 for an empty one.
inline int melody_step_ms(const MelodyStep &step, int tick_ms)
{
    int ticks = 0;
    for (const MelodyVoice &voice : step.voices) ticks = std::max(ticks, voice.ticks);
    return ticks * tick_ms;
}

inline int melody_total_ms(const Melody &melody)
{
    int total = 0;
    for (const MelodyStep &step : melody.steps) total += melody_step_ms(step, melody.tick_ms);
    return total;
}

inline double melody_frequency(int pitch) { return 440.0 * std::pow(2.0, (pitch - 69) / 12.0); }

// A 16-bit mono PCM WAV of the melody, square waves, the three voices mixed,
// for the local preview. The firmware's own timbre is unknown; this is only
// so the shape of a tune can be heard before it goes to a printer.
inline std::vector<std::uint8_t> melody_render_wav(const Melody &melody, int sample_rate = 22050)
{
    std::vector<std::int16_t> samples;
    for (const MelodyStep &step : melody.steps) {
        const int step_ms = melody_step_ms(step, melody.tick_ms);
        const size_t count = static_cast<size_t>(static_cast<long long>(sample_rate) * step_ms / 1000);
        const size_t base = samples.size();
        samples.resize(base + count, 0);
        for (const MelodyVoice &voice : step.voices) {
            if (voice.pitch <= 0 || voice.volume <= 0 || voice.ticks <= 0) continue;
            const double freq = melody_frequency(voice.pitch);
            const size_t voice_count = std::min(count, static_cast<size_t>(static_cast<long long>(sample_rate) * voice.ticks * melody.tick_ms / 1000));
            const double amplitude = 8000.0 * voice.volume / 100.0;
            for (size_t i = 0; i < voice_count; ++i) {
                const double phase = std::fmod(static_cast<double>(i) * freq / sample_rate, 1.0);
                const int value = static_cast<int>(samples[base + i]) + static_cast<int>(phase < 0.5 ? amplitude : -amplitude);
                samples[base + i] = static_cast<std::int16_t>(std::max(-32768, std::min(32767, value)));
            }
        }
    }
    const std::uint32_t data_size = static_cast<std::uint32_t>(samples.size() * 2);
    std::vector<std::uint8_t> wav;
    wav.reserve(44 + data_size);
    auto put32 = [&](std::uint32_t v) { for (int i = 0; i < 4; ++i) wav.push_back(static_cast<std::uint8_t>((v >> (8 * i)) & 0xFF)); };
    auto put16 = [&](std::uint16_t v) { for (int i = 0; i < 2; ++i) wav.push_back(static_cast<std::uint8_t>((v >> (8 * i)) & 0xFF)); };
    auto tag = [&](const char *t) { for (int i = 0; i < 4; ++i) wav.push_back(static_cast<std::uint8_t>(t[i])); };
    tag("RIFF"); put32(36 + data_size); tag("WAVE");
    tag("fmt "); put32(16); put16(1); put16(1); put32(static_cast<std::uint32_t>(sample_rate)); put32(static_cast<std::uint32_t>(sample_rate * 2)); put16(2); put16(16);
    tag("data"); put32(data_size);
    for (std::int16_t s : samples) put16(static_cast<std::uint16_t>(s));
    return wav;
}

// ---- presets ------------------------------------------------------------------

struct MelodyPreset
{
    const char *id;
    const char *name;  // untranslated; the dialog translates it
    const char *notes; // M1006 lines as the stock templates carry them
};

// The stock tunes of the Bambu Lab start and finish G-code templates, plus a
// scale to start from. Parsed through melody_parse_gcode like any import.
inline const std::vector<MelodyPreset> &melody_presets()
{
    static const std::vector<MelodyPreset> presets = {
        {"x1_start", "Bambu Lab X1 start",
         "M1006 A0 B10 L100 C37 D10 M60 E37 F10 N60\nM1006 A0 B10 L100 C41 D10 M60 E41 F10 N60\nM1006 A0 B10 L100 C44 D10 M60 E44 F10 N60\n"
         "M1006 A0 B10 L100 C0 D10 M60 E0 F10 N60\nM1006 A46 B10 L100 C43 D10 M70 E39 F10 N100\nM1006 A0 B10 L100 C0 D10 M60 E0 F10 N100\n"
         "M1006 A43 B10 L100 C0 D10 M60 E39 F10 N100\nM1006 A0 B10 L100 C0 D10 M60 E0 F10 N100\nM1006 A41 B10 L100 C0 D10 M100 E41 F10 N100\n"
         "M1006 A44 B10 L100 C0 D10 M100 E44 F10 N100\nM1006 A49 B10 L100 C0 D10 M100 E49 F10 N100\nM1006 A0 B10 L100 C0 D10 M100 E0 F10 N100\n"
         "M1006 A48 B10 L100 C44 D10 M60 E39 F10 N100\nM1006 A0 B10 L100 C0 D10 M60 E0 F10 N100\nM1006 A44 B10 L100 C0 D10 M90 E39 F10 N100\n"
         "M1006 A0 B10 L100 C0 D10 M60 E0 F10 N100\nM1006 A46 B10 L100 C43 D10 M60 E39 F10 N100\n"},
        {"x1_finish", "Bambu Lab X1 finish",
         "M1006 A0 B20 L100 C37 D20 M40 E42 F20 N60\nM1006 A0 B10 L100 C44 D10 M60 E44 F10 N60\nM1006 A0 B10 L100 C46 D10 M80 E46 F10 N80\n"
         "M1006 A44 B20 L100 C39 D20 M60 E48 F20 N60\nM1006 A0 B10 L100 C44 D10 M60 E44 F10 N60\nM1006 A0 B10 L100 C0 D10 M60 E0 F10 N60\n"
         "M1006 A0 B10 L100 C39 D10 M60 E39 F10 N60\nM1006 A0 B10 L100 C0 D10 M60 E0 F10 N60\nM1006 A0 B10 L100 C44 D10 M60 E44 F10 N60\n"
         "M1006 A0 B10 L100 C0 D10 M60 E0 F10 N60\nM1006 A0 B10 L100 C39 D10 M60 E39 F10 N60\nM1006 A0 B10 L100 C0 D10 M60 E0 F10 N60\n"
         "M1006 A0 B10 L100 C48 D10 M60 E44 F10 N100\nM1006 A0 B10 L100 C0 D10 M60 E0 F10 N100\nM1006 A49 B20 L100 C44 D20 M100 E41 F20 N100\n"
         "M1006 A0 B20 L100 C0 D20 M60 E0 F20 N100\nM1006 A0 B20 L100 C37 D20 M30 E37 F20 N60\n"},
        {"a1_start", "Bambu Lab A1 start",
         "M1006 A0 B10 L100 C37 D10 M60 E37 F10 N60\nM1006 A0 B10 L100 C41 D10 M60 E41 F10 N60\nM1006 A0 B10 L100 C44 D10 M60 E44 F10 N60\n"
         "M1006 A0 B10 L100 C0 D10 M60 E0 F10 N60\nM1006 A43 B10 L100 C46 D10 M70 E39 F10 N80\nM1006 A0 B10 L100 C0 D10 M60 E0 F10 N80\n"
         "M1006 A0 B10 L100 C43 D10 M60 E39 F10 N80\nM1006 A0 B10 L100 C0 D10 M60 E0 F10 N80\nM1006 A0 B10 L100 C41 D10 M80 E41 F10 N80\n"
         "M1006 A0 B10 L100 C44 D10 M80 E44 F10 N80\nM1006 A0 B10 L100 C49 D10 M80 E49 F10 N80\nM1006 A0 B10 L100 C0 D10 M80 E0 F10 N80\n"
         "M1006 A44 B10 L100 C48 D10 M60 E39 F10 N80\nM1006 A0 B10 L100 C0 D10 M60 E0 F10 N80\nM1006 A0 B10 L100 C44 D10 M80 E39 F10 N80\n"
         "M1006 A0 B10 L100 C0 D10 M60 E0 F10 N80\nM1006 A43 B10 L100 C46 D10 M60 E39 F10 N80\n"},
        {"a1_mini_start", "Bambu Lab A1 mini start",
         "M1006 A0 B0 L100 C37 D10 M100 E37 F10 N100\nM1006 A0 B0 L100 C41 D10 M100 E41 F10 N100\nM1006 A0 B0 L100 C44 D10 M100 E44 F10 N100\n"
         "M1006 A0 B10 L100 C0 D10 M100 E0 F10 N100\nM1006 A43 B10 L100 C39 D10 M100 E46 F10 N100\nM1006 A0 B0 L100 C0 D10 M100 E0 F10 N100\n"
         "M1006 A0 B0 L100 C39 D10 M100 E43 F10 N100\nM1006 A0 B0 L100 C0 D10 M100 E0 F10 N100\nM1006 A0 B0 L100 C41 D10 M100 E41 F10 N100\n"
         "M1006 A0 B0 L100 C44 D10 M100 E44 F10 N100\nM1006 A0 B0 L100 C49 D10 M100 E49 F10 N100\nM1006 A0 B0 L100 C0 D10 M100 E0 F10 N100\n"
         "M1006 A44 B10 L100 C39 D10 M100 E48 F10 N100\nM1006 A0 B0 L100 C0 D10 M100 E0 F10 N100\nM1006 A0 B0 L100 C39 D10 M100 E44 F10 N100\n"
         "M1006 A0 B0 L100 C0 D10 M100 E0 F10 N100\nM1006 A43 B10 L100 C39 D10 M100 E46 F10 N100\n"},
        {"h2d_start", "Bambu Lab H2D start",
         "M1006 A53 B9 L99 C53 D9 M99 E53 F9 N99\nM1006 A56 B9 L99 C56 D9 M99 E56 F9 N99\nM1006 A61 B9 L99 C61 D9 M99 E61 F9 N99\n"
         "M1006 A53 B9 L99 C53 D9 M99 E53 F9 N99\nM1006 A56 B9 L99 C56 D9 M99 E56 F9 N99\nM1006 A61 B18 L99 C61 D18 M99 E61 F18 N99\n"},
        {"h2d_finish", "Bambu Lab H2D finish",
         "M1006 A53 B10 L99 C53 D10 M99 E53 F10 N99\nM1006 A57 B10 L99 C57 D10 M99 E57 F10 N99\nM1006 A0 B15 L0 C0 D15 M0 E0 F15 N0\n"
         "M1006 A53 B10 L99 C53 D10 M99 E53 F10 N99\nM1006 A57 B10 L99 C57 D10 M99 E57 F10 N99\nM1006 A0 B15 L0 C0 D15 M0 E0 F15 N0\n"
         "M1006 A48 B10 L99 C48 D10 M99 E48 F10 N99\nM1006 A0 B15 L0 C0 D15 M0 E0 F15 N0\nM1006 A60 B10 L99 C60 D10 M99 E60 F10 N99\n"},
        {"c_major_scale", "C major scale",
         "M1006 A48 B10 L100 C0 D10 M0 E0 F10 N0\nM1006 A50 B10 L100 C0 D10 M0 E0 F10 N0\nM1006 A52 B10 L100 C0 D10 M0 E0 F10 N0\n"
         "M1006 A53 B10 L100 C0 D10 M0 E0 F10 N0\nM1006 A55 B10 L100 C0 D10 M0 E0 F10 N0\nM1006 A57 B10 L100 C0 D10 M0 E0 F10 N0\n"
         "M1006 A59 B10 L100 C0 D10 M0 E0 F10 N0\nM1006 A60 B20 L100 C0 D20 M0 E0 F20 N0\n"},
    };
    return presets;
}

inline Melody melody_from_preset(const MelodyPreset &preset)
{
    Melody melody;
    melody.name = preset.name;
    melody_parse_gcode(preset.notes, melody);
    return melody;
}

}} // namespace Slic3r::GUI

#endif
