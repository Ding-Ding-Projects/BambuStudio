#include <catch2/catch.hpp>

#include "slic3r/GUI/BilingualRegistry.hpp"

using Slic3r::GUI::I18N::BilingualRegistry;

namespace {

wxString u8(const char *text) { return wxString::FromUTF8(text); }

BilingualRegistry &fresh(bool enabled = true)
{
    BilingualRegistry &registry = BilingualRegistry::instance();
    registry.reset(enabled);
    return registry;
}

} // namespace

TEST_CASE("Bilingual registry normalises labels as the UI shows them", "[BilingualRegistry]")
{
    REQUIRE(BilingualRegistry::normalize("&File\tCtrl+O") == L"File");
    REQUIRE(BilingualRegistry::normalize("Save && close") == L"Save & close");
    REQUIRE(BilingualRegistry::normalize("Line one\nline  two ") == L"Line one line two");
    REQUIRE(BilingualRegistry::strip_mnemonic(u8("檔案(&F)")) == u8("檔案"));
    REQUIRE(BilingualRegistry::strip_mnemonic(u8("匯出（&E）")) == u8("匯出"));
    REQUIRE(BilingualRegistry::strip_mnemonic("A && B") == "A & B");
}

TEST_CASE("Bilingual registry answers exact labels, mnemonics and decorations", "[BilingualRegistry]")
{
    BilingualRegistry &registry = fresh();
    registry.record("Save", u8("儲存"));
    registry.record("&Open project", u8("開啟項目(&O)"));
    registry.record("Layer height", u8("層高"));
    registry.record("Export", u8("匯出"));

    REQUIRE(registry.lookup("&Save\tCtrl+S") == u8("儲存"));
    REQUIRE(registry.lookup("Open project") == u8("開啟項目"));
    REQUIRE(registry.lookup("Layer height:") == u8("層高："));
    REQUIRE(registry.lookup("Export...") == u8("匯出…"));
    REQUIRE(registry.lookup("Unknown label").empty());
}

TEST_CASE("Bilingual registry fills formatted templates with the displayed facts", "[BilingualRegistry]")
{
    BilingualRegistry &registry = fresh();
    registry.record("Deleted: %s", u8("已刪除：%s"));
    registry.record("Plate %d of %d", u8("第 %d 塊，共 %d 塊"));
    registry.record("%1% objects on %2%", u8("%2% 上面有 %1% 件物件"));
    registry.record("Progress %d%%", u8("進度 %d%%"));

    REQUIRE(registry.lookup("Deleted: cube.stl") == u8("已刪除：cube.stl"));
    REQUIRE(registry.lookup("Plate 2 of 5") == u8("第 2 塊，共 5 塊"));
    REQUIRE(registry.lookup("3 objects on Plate 1") == u8("Plate 1 上面有 3 件物件"));
    REQUIRE(registry.lookup("Progress 45%") == u8("進度 45%"));
    // A template needs every placeholder to capture something.
    REQUIRE(registry.lookup("Deleted: ").empty());
}

TEST_CASE("Bilingual registry refuses ambiguous or empty pairs", "[BilingualRegistry]")
{
    BilingualRegistry &registry = fresh();
    registry.record("%s%s", u8("甲%s乙%s"));     // adjacent placeholders
    registry.record("%s: %d", u8("%s：%d"));      // almost no literal text
    registry.record("OK", "OK");                  // same text in both languages
    registry.record("Count %d items", u8("數量")); // placeholder count differs
    REQUIRE(registry.lookup("ab").empty());
    REQUIRE(registry.lookup("x: 3").empty());
    REQUIRE(registry.lookup("OK").empty());
    REQUIRE(registry.lookup("Count 3 items").empty());
    REQUIRE(registry.size() == 0);
}

TEST_CASE("Bilingual registry records nothing outside bilingual mode", "[BilingualRegistry]")
{
    BilingualRegistry &registry = fresh(false);
    registry.record("Save", u8("儲存"));
    REQUIRE(registry.lookup("Save").empty());
    REQUIRE(registry.size() == 0);
    fresh(true);
    REQUIRE(registry.lookup("Save").empty()); // reset clears everything
}
