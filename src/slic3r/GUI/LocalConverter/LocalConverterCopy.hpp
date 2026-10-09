#pragma once
// Voice ladders for the converter's non-factual copy: the guidance line, empty
// states, and the progress and completion lines. Factual copy (adapter names,
// change disclosures, unavailable reasons, states, result messages, limits and
// safety statements) never varies with the funny level and is translated
// directly. Every variant states the same facts and keeps the same printf
// placeholders in the same order; only the voice changes.
//
// The English variants are catalogue sources: the panel looks each one up in
// the message catalogue, so Hong Kong Cantonese and bilingual modes come from
// bbl/i18n/yue_HK. Levels expand like the shared three-entry ladder
// (LanguageMode.cpp): levels 1-2 serious, 3 light, 4-5 playful.
//
// This header is free of GUI includes so the tests can build it alone.
#include <cstddef>

// Marks catalogue sources for xgettext, exactly as slic3r/GUI/I18N.hpp does.
#ifndef L
#define L(s) s
#endif

namespace Slic3r::GUI::LocalConverterCopy {

inline constexpr int FUNNY_LEVEL_MIN = 1;
inline constexpr int FUNNY_LEVEL_MAX = 5;

enum class Line : unsigned {
    Intro,
    EmptyQueue,
    NoMatches,
    NoAdapterSelected,
    Checking,
    ChecksFinished,
    Converting,
    Stopped,  // two %llu counts: added, then not admitted
    Exported,
    Count
};

struct Ladder {
    const char *serious;
    const char *light;
    const char *playful;
};

inline const Ladder &ladder(Line line)
{
    static const Ladder ladders[static_cast<std::size_t>(Line::Count)] = {
        {L("Choose a verified adapter, review its data changes, choose an output folder, then add files. Sources are preserved. Existing outputs are skipped. Processing stays offline."),
         L("Pick a verified adapter, read what it changes, choose an output folder, then add files. Sources are preserved, existing outputs are skipped and processing stays offline."),
         L("Pick a verified adapter, read exactly what it changes, choose where the results land, then hand it some files. Sources are preserved, existing outputs are skipped, and processing never leaves this computer.")},
        {L("Empty queue. No source files have been selected."),
         L("The queue is empty. No source files have been selected yet."),
         L("The queue is empty and no source files have been selected. It is ready when you are.")},
        {L("No adapters match this search."),
         L("No adapters match this search. Try fewer words."),
         L("No adapters match this search. Try fewer words, or clear it to see the whole category again.")},
        {L("No adapter selected. Unavailable formats remain listed with their exact reason."),
         L("Select an adapter to see what it changes. Unavailable formats remain listed with their exact reason."),
         L("Pick an adapter and it will spell out what it changes. Unavailable formats stay listed, each with its exact reason.")},
        {L("Checking the bundled worker and offline sandbox. No converter is enabled until its runtime check completes."),
         L("Checking the bundled worker and its offline sandbox. Converters stay disabled until the check completes."),
         L("Giving the bundled worker and its offline sandbox a once-over. Every converter stays disabled until that check completes.")},
        {L("Bundled worker checks finished. Each unavailable adapter shows its exact reason."),
         L("Worker checks finished. Each unavailable adapter shows its exact reason."),
         L("The worker checks are finished. Anything still unavailable says exactly why.")},
        {L("Converting offline in the isolated worker. One file runs at a time; every result is saved."),
         L("Converting offline in the isolated worker, one file at a time. Every result is saved."),
         L("Converting offline in the isolated worker, one file at a time, and writing down every result as it goes.")},
        {L("Operation stopped. Added: %llu. Not admitted: %llu. Review each saved result below."),
         L("Stopped. Added: %llu. Not admitted: %llu. Each saved result is listed below."),
         L("All stopped. %llu added and %llu not admitted, with every saved result listed below.")},
        {L("Visible result rows exported as UTF-8 CSV. Source paths were excluded."),
         L("Exported the visible result rows as UTF-8 CSV, without source paths."),
         L("The visible result rows are out as UTF-8 CSV, and the source paths stayed behind.")},
    };
    return ladders[static_cast<std::size_t>(line)];
}

// The English catalogue source for a line at a funny level. Out-of-range
// levels clamp to 1..5.
inline const char *source(Line line, int level)
{
    const Ladder &entry = ladder(line);
    if (level <= 2) return entry.serious;
    if (level == 3) return entry.light;
    return entry.playful;
}

} // namespace Slic3r::GUI::LocalConverterCopy
