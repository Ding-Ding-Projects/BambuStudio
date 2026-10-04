#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace Slic3r { namespace GUI {
struct SearchRecord {
    std::string id, query, view, category, device, status, from, to;
    bool regex = false;
    bool case_sensitive = false;
    bool whole_word = false;
    bool multiline = false;
    bool pinned = false;
    std::int64_t submitted_at = 0;
};
// Local-only submitted search history. No search is recorded while typing.
class HistorySearchStore {
public:
    static HistorySearchStore &instance();
    explicit HistorySearchStore(std::string storage_path);
    const std::vector<SearchRecord> &records() const { return m_records; }
    bool load();
    bool submit(SearchRecord record);
    bool set_pinned(const std::string &id, bool pinned);
    bool update(const SearchRecord &record);
    bool remove(const std::string &id);
    bool clear(bool include_pinned = false);
    static bool safe_to_store(const SearchRecord &record);
private:
    bool save(const std::vector<SearchRecord> &records);
    static void trim(std::vector<SearchRecord> &records);
    std::string m_path;
    std::vector<SearchRecord> m_records;
};
}}

