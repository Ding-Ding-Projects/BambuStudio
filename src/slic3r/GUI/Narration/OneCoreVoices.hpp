#pragma once
#include "VoiceCatalog.hpp"

#include <vector>

namespace Slic3r::GUI::Narration {
// Reads the system speech inventory that Windows keeps for OneCore and natural
// voices (where Hong Kong Cantonese voices normally live). Read-only registry
// access; returns an empty list on other platforms or when the key is absent.
// These voices are listed but not speakable by the SAPI playback path.
std::vector<CatalogVoice> enumerate_onecore_voices();
}
