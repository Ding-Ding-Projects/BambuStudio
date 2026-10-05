#include "PdfRequest.hpp"
#include "PdfAdapter.hpp"
#include "miniz/miniz.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cstring>
#include <limits>
#include <map>
#include <set>

namespace Slic3r::LocalConverter {
namespace {
using Json = nlohmann::ordered_json;
constexpr std::size_t envelope_limit = 24 * 1024 * 1024;
constexpr std::size_t source_limit = 1000;
struct Problem { const char *code; };
void require(bool yes,const char *code) {if(!yes)throw Problem{code};}
void only_keys(const Json &object,const std::set<std::string> &allowed) {
 require(object.is_object(),"pdf_request_object");
 for(auto it=object.begin();it!=object.end();++it) require(allowed.count(it.key())!=0,"pdf_request_unknown_option");
}
Json parse(const Bytes &input) {
 require(input.size()<=envelope_limit,"pdf_envelope_limit");
 require(valid_utf8(input),"pdf_request_utf8");
 // Reject excessive nesting before the parser can allocate a deep DOM. Quoted
 // braces and escaped quotes do not alter structural depth.
 std::size_t depth=0;bool quoted=false,escape=false;
 for(unsigned char c:input) {
  if(quoted) {if(escape)escape=false;else if(c=='\\')escape=true;else if(c=='"')quoted=false;continue;}
  if(c=='"')quoted=true;
  else if(c=='{' || c=='[') require(++depth<=Limits::depth,"pdf_request_depth_limit");
  else if(c=='}' || c==']') {require(depth>0,"pdf_request_json");--depth;}
 }
 std::size_t items=0;std::vector<std::set<std::string>> keys;
 auto callback=[&](int level,Json::parse_event_t event,Json &value) {
  require(level<=static_cast<int>(Limits::depth),"pdf_request_depth_limit");
  require(++items<=Limits::items,"pdf_request_item_limit");
  if(event==Json::parse_event_t::object_start)keys.emplace_back();
  if(event==Json::parse_event_t::key) {require(!keys.empty(),"pdf_request_json");require(keys.back().insert(value.get<std::string>()).second,"pdf_request_duplicate_key");}
  if(event==Json::parse_event_t::object_end) {require(!keys.empty(),"pdf_request_json");keys.pop_back();}
  return true;
 };
 return Json::parse(input.begin(),input.end(),callback);
}
Bytes decode(const std::string &text,std::size_t &aggregate) {
 require(!text.empty() && text.size()%4==0,"pdf_request_base64");
 require(text.size()/4<=((Limits::input_bytes+2)/3),"pdf_input_limit");
 const auto padding=text.find('=');
 require(padding==std::string::npos || (padding>=text.size()-2 && text.find_first_not_of('=',padding)==std::string::npos),"pdf_request_base64");
 const std::size_t decoded_size=text.size()/4*3-(padding==std::string::npos?0:text.size()-padding);
 require(decoded_size<=Limits::input_bytes && aggregate<=Limits::input_bytes-decoded_size,"pdf_input_limit");
 Bytes result;result.reserve(decoded_size);
 // Core adapters have a 16 MiB input bound. Decode at aligned, independently
 // canonical boundaries so a permitted 16 MiB PDF can carry 22 MiB of Base64.
 constexpr std::size_t chunk=4*1024*1024;
 for(std::size_t at=0;at<text.size();) {
  const auto length=std::min(chunk,text.size()-at);
  Bytes part(text.begin()+at,text.begin()+at+length);
  auto decoded=transform("base64.decode",part);
  require(decoded.outcome==Outcome::Converted,"pdf_request_base64");
  require(decoded.output.size()<=decoded_size-result.size(),"pdf_request_base64");
  result.insert(result.end(),decoded.output.begin(),decoded.output.end());at+=length;
 }
 require(result.size()==decoded_size,"pdf_request_base64");aggregate+=result.size();return result;
}
std::size_t positive_index(const Json &value) {
 require(value.is_number_integer(),"pdf_request_page_type");
 if(value.is_number_unsigned()) {auto n=value.get<std::uint64_t>();require(n>0 && n<=source_limit,"pdf_page_range");return static_cast<std::size_t>(n);}
 auto n=value.get<std::int64_t>();require(n>0 && n<=static_cast<std::int64_t>(source_limit),"pdf_page_range");return static_cast<std::size_t>(n);
}
PdfOptions options(const Json &object) {
 only_keys(object,{"pages","rotation","metadata"});PdfOptions result;
 if(object.contains("pages")) {
  const auto &pages=object.at("pages");require(pages.is_array() && pages.size()<=source_limit,"pdf_request_pages");
  for(const auto &page:pages)result.pages.push_back(positive_index(page));
 }
 if(object.contains("rotation")) {
  const auto &r=object.at("rotation");require(r.is_number_integer(),"pdf_rotation_option");
  if(r.is_number_unsigned())require(r.get<std::uint64_t>()<=270,"pdf_rotation_option");
  else require(r.get<std::int64_t>()>=0 && r.get<std::int64_t>()<=270,"pdf_rotation_option");
  result.rotation=r.get<int>();require(result.rotation%90==0,"pdf_rotation_option");
 }
 if(object.contains("metadata")) {
  const auto &metadata=object.at("metadata");require(metadata.is_object() && metadata.size()<=9,"pdf_request_metadata");
  for(auto it=metadata.begin();it!=metadata.end();++it) {
   require(it.value().is_string(),"pdf_request_metadata_value");
   const auto &value=it.value().get_ref<const std::string &>();require(value.size()<=4096,"pdf_metadata_limit");
   result.metadata.emplace(it.key(),value);
  }
 }return result;
}
struct Zip {
 mz_zip_archive archive{};bool writer=false,reader=false;
 ~Zip(){if(writer)mz_zip_writer_end(&archive);if(reader)mz_zip_reader_end(&archive);}
};
Bytes split_zip(const std::vector<Bytes> &pages) {
 require(!pages.empty() && pages.size()<=source_limit,"pdf_split_count");
 // Stored entries have bounded filename/header overhead. Reserve a conservative
 // allowance before allocation; never let the central directory exceed budget.
 std::size_t total=pages.size()*512+1024;
 for(const auto &page:pages) {require(page.size()<=Limits::output_bytes && total<=Limits::output_bytes-page.size(),"pdf_output_limit");total+=page.size();}
 Zip writer;writer.writer=mz_zip_writer_init_heap(&writer.archive,0,0)!=0;require(writer.writer,"pdf_zip_memory");
 for(std::size_t i=0;i<pages.size();++i) {
  const auto name="page-"+std::to_string(i+1)+".pdf";
  require(mz_zip_writer_add_mem(&writer.archive,name.c_str(),pages[i].data(),pages[i].size(),MZ_NO_COMPRESSION)!=0,"pdf_zip_write");
 }
 void *buffer=nullptr;std::size_t size=0;
 require(mz_zip_writer_finalize_heap_archive(&writer.archive,&buffer,&size)!=0,"pdf_zip_finalize");
 struct Owned {void *p;~Owned(){mz_free(p);}} owned{buffer};
 require(buffer && size<=Limits::output_bytes,"pdf_output_limit");
 Bytes output(static_cast<unsigned char*>(buffer),static_cast<unsigned char*>(buffer)+size);
 Zip reader;reader.reader=mz_zip_reader_init_mem(&reader.archive,output.data(),output.size(),0)!=0;require(reader.reader,"pdf_zip_validation");
 require(mz_zip_reader_get_num_files(&reader.archive)==pages.size(),"pdf_zip_validation");
 for(std::size_t i=0;i<pages.size();++i) {
  mz_zip_archive_file_stat stat{};require(mz_zip_reader_file_stat(&reader.archive,static_cast<mz_uint>(i),&stat)!=0,"pdf_zip_validation");
  const auto name="page-"+std::to_string(i+1)+".pdf";
  require(!stat.m_is_directory && stat.m_uncomp_size==pages[i].size() && name==stat.m_filename,"pdf_zip_validation");
  Bytes restored(pages[i].size());require(mz_zip_reader_extract_to_mem(&reader.archive,static_cast<mz_uint>(i),restored.data(),restored.size(),0)!=0,"pdf_zip_validation");
  require(restored==pages[i],"pdf_zip_validation");
 }return output;
}
}
Conversion pdf_request_transform(const std::string &adapter,const Bytes &envelope) {
 try {
  const std::map<std::string,PdfOperation> operations={{"pdf.inspect",PdfOperation::Inspect},{"pdf.split",PdfOperation::Split},{"pdf.merge",PdfOperation::Merge},{"pdf.extract",PdfOperation::Extract},{"pdf.reorder",PdfOperation::Reorder},{"pdf.rotate",PdfOperation::Rotate},{"pdf.metadata",PdfOperation::Metadata}};
  const auto operation=operations.find(adapter);require(operation!=operations.end(),"pdf_request_operation");
  auto request=parse(envelope);only_keys(request,{"sources","options"});
  require(request.contains("sources") && request.at("sources").is_array(),"pdf_request_sources");
  const auto &encoded=request.at("sources");require(!encoded.empty() && encoded.size()<=source_limit,"pdf_source_limit");
  require(operation->second==PdfOperation::Merge || encoded.size()==1,"pdf_source_count");
  const auto selected=request.contains("options")?options(request.at("options")):PdfOptions{};
  if(request.contains("options")) {
   const auto &opts=request.at("options");
   require(!opts.contains("pages") || operation->second==PdfOperation::Extract || operation->second==PdfOperation::Reorder || operation->second==PdfOperation::Rotate,"pdf_unexpected_selection");
   require(!opts.contains("rotation") || operation->second==PdfOperation::Rotate,"pdf_unexpected_rotation");
   require(!opts.contains("metadata") || operation->second==PdfOperation::Metadata,"pdf_unexpected_metadata");
  }
  std::vector<Bytes> sources;sources.reserve(encoded.size());std::size_t aggregate=0;
  for(const auto &source:encoded) {require(source.is_string(),"pdf_request_source_type");sources.push_back(decode(source.get_ref<const std::string &>(),aggregate));}
  auto result=pdf_transform(operation->second,sources,selected);
  if(result.outcome!=Outcome::Converted)return {Outcome::Failed,result.code,{}};
  if(operation->second==PdfOperation::Inspect) {
   require(result.outputs.empty(),"pdf_result_shape");
   Json data={{"page_count",result.page_count},{"rotations",result.rotations},{"metadata",result.metadata}};
   auto text=data.dump();require(text.size()<=Limits::output_bytes,"pdf_output_limit");
   return {Outcome::Converted,"converted",Bytes(text.begin(),text.end())};
  }
  if(operation->second==PdfOperation::Split) {require(result.outputs.size()==result.page_count,"pdf_result_shape");return {Outcome::Converted,"converted",split_zip(result.outputs)};}
  require(result.outputs.size()==1 && result.outputs.front().size()<=Limits::output_bytes,"pdf_result_shape");
  return {Outcome::Converted,"converted",std::move(result.outputs.front())};
 } catch(const Problem &p) {return {Outcome::Failed,p.code,{}};}
 catch(const nlohmann::json::exception &) {return {Outcome::Failed,"pdf_request_json",{}};}
 catch(const std::bad_alloc &) {return {Outcome::Failed,"pdf_memory_limit",{}};}
 catch(...) {return {Outcome::Failed,"pdf_request_internal",{}};}
}
}
