#include "ArchiveAdapter.hpp"
#include "miniz/miniz.h"
#include <cstring>

namespace Slic3r::LocalConverter {
namespace {
struct Reader {
    mz_zip_archive zip{};
    bool ready = false;
    ~Reader() { if (ready) mz_zip_reader_end(&zip); }
};
Conversion extract(const Bytes &input)
{
    Reader r; r.ready = mz_zip_reader_init_mem(&r.zip,input.data(),input.size(),0) != 0;
    if (!r.ready) return {Outcome::Failed,"invalid_zip",{}};
    if (mz_zip_reader_get_num_files(&r.zip) != 1) return {Outcome::Failed,"zip_requires_one_regular_entry",{}};
    mz_zip_archive_file_stat stat{};
    if (!mz_zip_reader_file_stat(&r.zip,0,&stat) || stat.m_is_directory || (stat.m_bit_flag & 1) || stat.m_method > 8)
        return {Outcome::Failed,"unsupported_or_encrypted_zip",{}};
    if (stat.m_uncomp_size > Limits::output_bytes) return {Outcome::Failed,"archive_output_limit",{}};
    // Names are never interpreted as filesystem destinations, but traversal
    // and absolute names are rejected so the output cannot normalize an unsafe
    // archive into something that appears to have passed safety validation.
    const std::string name(stat.m_filename);
    if (name.empty() || name.find('/') != std::string::npos || name.find('\\') != std::string::npos || name.find(':') != std::string::npos || name == "." || name == "..")
        return {Outcome::Failed,"unsafe_archive_entry_name",{}};
    Bytes output(static_cast<std::size_t>(stat.m_uncomp_size));
    if (!mz_zip_reader_extract_to_mem(&r.zip,0,output.data(),output.size(),0)) return {Outcome::Failed,"archive_crc_or_decode_failed",{}};
    return {Outcome::Converted,"converted",std::move(output)};
}
}
Conversion archive_transform(const std::string &adapter,const Bytes &input)
{
    if (input.size() > Limits::input_bytes) return {Outcome::Failed,"input_limit",{}};
    if (adapter == "zip.decode") return extract(input);
    if (adapter != "zip.encode") return {Outcome::Failed,"adapter_unavailable",{}};
    mz_zip_archive zip{};
    if (!mz_zip_writer_init_heap(&zip,0,65536)) return {Outcome::Failed,"archive_memory_limit",{}};
    struct End { mz_zip_archive *zip; ~End(){ mz_zip_writer_end(zip); } } end{&zip};
    if (!mz_zip_writer_add_mem(&zip,"payload.bin",input.data(),input.size(),MZ_BEST_SPEED)) return {Outcome::Failed,"archive_encode_failed",{}};
    void *buffer = nullptr; size_t size = 0;
    if (!mz_zip_writer_finalize_heap_archive(&zip,&buffer,&size)) return {Outcome::Failed,"archive_finalize_failed",{}};
    struct Free { void *p; ~Free(){ mz_free(p); } } free{buffer};
    if (size > Limits::output_bytes) return {Outcome::Failed,"output_limit",{}};
    Bytes output(static_cast<unsigned char *>(buffer),static_cast<unsigned char *>(buffer)+size);
    const auto checked = extract(output);
    if (checked.outcome != Outcome::Converted || checked.output != input) return {Outcome::Failed,"archive_roundtrip_failed",{}};
    return {Outcome::Converted,"converted",std::move(output)};
}
}
