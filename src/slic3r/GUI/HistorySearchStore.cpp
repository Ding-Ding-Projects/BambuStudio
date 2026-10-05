#include "HistorySearchStore.hpp"
#include "libslic3r/Utils.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <set>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#endif

namespace Slic3r { namespace GUI {
namespace {
constexpr std::size_t max_file_bytes = 4 * 1024 * 1024;
constexpr std::size_t max_records = 1100;
using Json = nlohmann::json;
Json encode(const SearchRecord &r) {
    return {{"id",r.id},{"query",r.query},{"regex",r.regex},{"case_sensitive",r.case_sensitive},{"whole_word",r.whole_word},{"multiline",r.multiline},{"view",r.view},
            {"category",r.category},{"device",r.device},{"status",r.status},
            {"from",r.from},{"to",r.to},{"pinned",r.pinned},{"submitted_at",r.submitted_at}};
}
SearchRecord decode(const Json &j) {
    SearchRecord r;
    r.id=j.at("id").get<std::string>(); r.query=j.at("query").get<std::string>();
    r.regex=j.at("regex").get<bool>(); r.pinned=j.at("pinned").get<bool>();
    r.case_sensitive=j.value("case_sensitive",false); r.whole_word=j.value("whole_word",false); r.multiline=j.value("multiline",false);
    r.submitted_at=j.at("submitted_at").get<std::int64_t>();
    r.view=j.value("view",std::string()); r.category=j.value("category",std::string());
    r.device=j.value("device",std::string()); r.status=j.value("status",std::string());
    r.from=j.value("from",std::string()); r.to=j.value("to",std::string());
    return r;
}
}
HistorySearchStore &HistorySearchStore::instance() {
    static HistorySearchStore store((std::filesystem::u8path(Slic3r::data_dir()) / "history-searches-v1.json").u8string());
    return store;
}
HistorySearchStore::HistorySearchStore(std::string path):m_path(std::move(path)) { load(); }
bool HistorySearchStore::safe_to_store(const SearchRecord &r) {
    if (r.query.size()>2048 || r.id.size()>128 || r.view.size()>128 || r.category.size()>128 ||
        r.device.size()>256 || r.status.size()>128 || r.from.size()>32 || r.to.size()>32 || r.submitted_at<0)
        return false;
    const std::string *fields[]={&r.query,&r.view,&r.category,&r.device,&r.status,&r.from,&r.to};
    for (const auto *field:fields) {
        std::string lower=*field;
        std::transform(lower.begin(),lower.end(),lower.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
        for (const char *marker:{"password", "passwd", "authorization", "bearer ", "access_token", "refresh_token", "session_id", "sessionid", "api_key", "apikey", "cookie", "secret", "private key", "token="})
            if (lower.find(marker)!=std::string::npos) return false;
        for (unsigned char c:*field) if (c<32 || c==127) return false;
    }
    return true;
}
void HistorySearchStore::trim(std::vector<SearchRecord> &records) {
    std::stable_sort(records.begin(),records.end(),[](const SearchRecord &a,const SearchRecord &b){return a.submitted_at>b.submitted_at;});
    std::size_t unpinned=0;
    records.erase(std::remove_if(records.begin(),records.end(),[&](const SearchRecord &r){return !r.pinned && ++unpinned>100;}),records.end());
}
bool HistorySearchStore::load() {
    try {
        const auto path=std::filesystem::u8path(m_path);
        if (!std::filesystem::exists(path)) { m_records.clear(); return true; }
        if (std::filesystem::file_size(path)>max_file_bytes) return false;
        std::ifstream in(path,std::ios::binary);
        if (!in) return false;
        // Bound the actual read too, including files that grew after file_size.
        std::string bytes(max_file_bytes+1,'\0');
        in.read(&bytes[0],static_cast<std::streamsize>(bytes.size()));
        bytes.resize(static_cast<std::size_t>(in.gcount()));
        if (bytes.size()>max_file_bytes) return false;
        const auto root=Json::parse(bytes, [](int depth, Json::parse_event_t, Json &) { if (depth>16) throw std::runtime_error("Search history nesting exceeds limit"); return true; });
        if (root.at("version")!=1 || !root.at("records").is_array() || root.at("records").size()>max_records) return false;
        std::vector<SearchRecord> loaded; std::set<std::string> ids;
        for (const auto &j:root.at("records")) {
            auto r=decode(j);
            if (!r.id.empty() && safe_to_store(r) && ids.insert(r.id).second) loaded.push_back(std::move(r));
        }
        trim(loaded); m_records=std::move(loaded); return true;
    } catch (...) { return false; }
}
bool HistorySearchStore::save(const std::vector<SearchRecord> &records) {
    if (records.size()>max_records) return false;
    static std::atomic<unsigned long long> sequence{0};
    auto path=std::filesystem::u8path(m_path);
    auto temp=path; temp += ".tmp-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+"-"+std::to_string(sequence++);
    try {
        Json rows=Json::array(); for(const auto &r:records) rows.push_back(encode(r));
        const auto text=Json{{"version",1},{"records",std::move(rows)}}.dump();
        if (text.size()>max_file_bytes) return false;
        if (!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path());
        { std::ofstream out(temp,std::ios::binary|std::ios::trunc); if(!out) return false;
          out.write(text.data(),static_cast<std::streamsize>(text.size())); out.flush();
          if(!out) throw std::runtime_error("Unable to write local search history"); }
#ifdef _WIN32
        if (!MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) throw std::runtime_error("Unable to replace local search history");
#else
        std::filesystem::rename(temp,path);
#endif
        m_records=records; return true;
    } catch (...) { std::error_code ec; std::filesystem::remove(temp,ec); return false; }
}
bool HistorySearchStore::submit(SearchRecord record) {
    if(!safe_to_store(record)) return false;
    static std::atomic<unsigned long long> sequence{0};
    record.submitted_at=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    do { record.id=std::to_string(record.submitted_at)+"-"+std::to_string(sequence++); } while (std::any_of(m_records.begin(),m_records.end(),[&](const SearchRecord &r){return r.id==record.id;}));
    auto rows=m_records; rows.insert(rows.begin(),std::move(record)); trim(rows); return save(rows);
}
bool HistorySearchStore::update(const SearchRecord &record) {
    if(!safe_to_store(record)) return false;
    auto rows=m_records; const auto found=std::find_if(rows.begin(),rows.end(),[&](const SearchRecord &r){return r.id==record.id;});
    if(found==rows.end()) return false; *found=record; trim(rows); return save(rows);
}
bool HistorySearchStore::set_pinned(const std::string &id,bool pinned) {
    const auto found=std::find_if(m_records.begin(),m_records.end(),[&](const SearchRecord &r){return r.id==id;});
    if(found==m_records.end()) return false; auto record=*found; record.pinned=pinned; return update(record);
}
bool HistorySearchStore::remove(const std::string &id) {
    auto rows=m_records; const auto old=rows.size();
    rows.erase(std::remove_if(rows.begin(),rows.end(),[&](const SearchRecord &r){return r.id==id;}),rows.end());
    return old!=rows.size() && save(rows);
}
bool HistorySearchStore::clear(bool include_pinned) {
    auto rows=m_records; rows.erase(std::remove_if(rows.begin(),rows.end(),[&](const SearchRecord &r){return include_pinned || !r.pinned;}),rows.end()); return save(rows);
}
}}
