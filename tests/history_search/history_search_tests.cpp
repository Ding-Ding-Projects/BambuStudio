#include <catch_main.hpp>
#include "slic3r/GUI/HistorySearchStore.hpp"
#include <filesystem>
#include <fstream>
#include <chrono>
using namespace Slic3r::GUI;
namespace {
struct LocalStoreFile {
    std::filesystem::path path=std::filesystem::temp_directory_path()/ ("bambu-search-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".json");
    ~LocalStoreFile(){std::error_code ec;std::filesystem::remove(path,ec);}
};
}
TEST_CASE("Submitted search history retains filters across restart", "[history_search]") {
    LocalStoreFile file;
    HistorySearchStore store(file.path.u8string());
    SearchRecord r; r.query="cube";r.regex=true;r.case_sensitive=true;r.whole_word=true;r.multiline=true;r.view="project";r.category="save";r.device="printer";r.status="success";r.from="2026-01-01";r.to="2026-10-04";
    REQUIRE(store.submit(r));
    const auto id=store.records().front().id;
    REQUIRE(store.set_pinned(id,true));
    HistorySearchStore restored(file.path.u8string());
    REQUIRE(restored.records().size()==1);
    const auto &read=restored.records().front();
    CHECK(read.id==id);CHECK(read.query=="cube");CHECK(read.regex);CHECK(read.case_sensitive);CHECK(read.whole_word);CHECK(read.multiline);CHECK(read.pinned);
    CHECK(read.category=="save");CHECK(read.device=="printer");CHECK(read.status=="success");
    CHECK(read.from==r.from);CHECK(read.to==r.to);CHECK(read.view=="project");
    REQUIRE(restored.clear());CHECK(restored.records().size()==1);
    REQUIRE(restored.remove(id));CHECK(restored.records().empty());
}
TEST_CASE("Only the last hundred unpinned submissions are kept", "[history_search]") {
    LocalStoreFile file;HistorySearchStore store(file.path.u8string());
    SearchRecord r;r.query="retained";r.pinned=true;REQUIRE(store.submit(r));
    r.pinned=false;
    for(int i=0;i<110;++i){r.query="query "+std::to_string(i);REQUIRE(store.submit(r));}
    REQUIRE(store.records().size()==101);
    REQUIRE(store.clear());REQUIRE(store.records().size()==1);CHECK(store.records().front().query=="retained");
    REQUIRE(store.clear(true));CHECK(store.records().empty());
}
TEST_CASE("Sensitive and malformed history cannot replace valid memory", "[history_search]") {
    LocalStoreFile file;HistorySearchStore store(file.path.u8string());SearchRecord r;r.query="cube";REQUIRE(store.submit(r));
    r.query="Authorization: Bearer example";CHECK_FALSE(store.submit(r));
    r.query="cube";r.device="api_key=example";CHECK_FALSE(store.submit(r));
    CHECK(store.records().size()==1);
    {std::ofstream out(file.path);out<<"{\"version\":2,\"records\":[]}";}
    CHECK_FALSE(store.load());CHECK(store.records().size()==1);
    {std::ofstream out(file.path);out<<"invalid";}
    CHECK_FALSE(store.load());CHECK(store.records().size()==1);
}
TEST_CASE("Failed persistence does not report a submitted search", "[history_search]") {
    LocalStoreFile file;{std::ofstream out(file.path);out<<"block directory creation";}
    HistorySearchStore store((file.path/"history.json").u8string());SearchRecord r;r.query="cube";
    CHECK_FALSE(store.submit(r));CHECK(store.records().empty());
}

