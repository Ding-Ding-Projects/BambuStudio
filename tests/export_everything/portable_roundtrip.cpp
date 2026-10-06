#include "slic3r/GUI/Export/ExportEverything.hpp"
#include <filesystem>
#include <fstream>
using namespace Slic3r::GUI::Export;
int main(int argc, char **argv) {
    if (argc != 2) return 2;
    Dataset d;
    d.kind = DatasetKind::Structured;
    d.schema_id = "test.portable";
    d.name = "Portable fixture";
    d.root = Value::make_object();
    d.root.set("large", Value::from_int(9007199254740993LL));
    d.root.set("text", Value::from_string("quotes \" \\ newline\n\t測試"));
    d.root.set("empty", Value::null());
    for (auto f : {Format::JSON, Format::SQL, Format::JavaScript, Format::TypeScript, Format::Python, Format::Go, Format::Rust, Format::JSONSchema, Format::Protobuf}) {
        auto value = serialize(d, f);
        std::ofstream out(std::filesystem::u8path(argv[1]) / (std::string("fixture.") + format_extension(f)), std::ios::binary);
        out << value.body;
        if (!out) return 3;
    }
}
