#include "PdfAdapter.hpp"
#include <qpdf/qpdf-c.h>
#include <algorithm>
#include <cstring>
#include <memory>
#include <set>
#include <stdexcept>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

namespace Slic3r::LocalConverter {
namespace {
#define PDF_SYMBOLS(X) \
 X(qpdf_init) X(qpdf_cleanup) X(qpdf_get_qpdf_version) X(qpdf_silence_errors) \
 X(qpdf_set_suppress_warnings) X(qpdf_set_attempt_recovery) X(qpdf_read_memory) \
 X(qpdf_has_error) X(qpdf_more_warnings) X(qpdf_get_error) X(qpdf_get_error_code) \
 X(qpdf_is_encrypted) X(qpdf_get_root) X(qpdf_get_trailer) X(qpdf_get_num_pages) \
 X(qpdf_get_page_n) X(qpdf_push_inherited_attributes_to_page) X(qpdf_empty_pdf) \
 X(qpdf_add_page) X(qpdf_get_info_key) X(qpdf_set_info_key) X(qpdf_init_write_memory) \
 X(qpdf_set_stream_data_mode) X(qpdf_set_object_stream_mode) X(qpdf_write) \
 X(qpdf_get_buffer_length) X(qpdf_get_buffer) X(qpdf_oh_is_null) X(qpdf_oh_is_integer) \
 X(qpdf_oh_is_dictionary) X(qpdf_oh_is_array) X(qpdf_oh_is_stream) X(qpdf_oh_get_dict) \
 X(qpdf_oh_get_key) X(qpdf_oh_has_key) X(qpdf_oh_get_int_value) X(qpdf_oh_new_integer) \
 X(qpdf_oh_replace_key) X(qpdf_oh_begin_dict_key_iter) X(qpdf_oh_dict_more_keys) \
 X(qpdf_oh_dict_next_key) X(qpdf_oh_get_array_n_items) X(qpdf_oh_get_array_item) \
 X(qpdf_oh_unparse) X(qpdf_oh_get_stream_data) X(qpdf_oh_free_buffer) \
 X(qpdf_oh_is_string) X(qpdf_oh_get_binary_utf8_value) X(qpdf_oh_new_dictionary) X(qpdf_oh_new_unicode_string)
struct Api {
#define DECLARE(n) decltype(&::n) n = nullptr;
 PDF_SYMBOLS(DECLARE)
#undef DECLARE
 bool ready = false;
} api;
constexpr std::size_t max_pages = 1000;
const std::set<std::string> info_keys = {"Title", "Author", "Subject", "Keywords", "Creator", "Producer", "CreationDate", "ModDate", "Trapped"};
struct Problem { const char *code; };
void require(bool yes, const char *code) { if (!yes) throw Problem{code}; }
void check(qpdf_data q) { require(!api.qpdf_has_error(q) && !api.qpdf_more_warnings(q), "pdf_malformed"); }
struct Document {
 qpdf_data q = api.qpdf_init();
 Document() { require(q != nullptr, "pdf_memory_limit"); api.qpdf_silence_errors(q); api.qpdf_set_suppress_warnings(q, QPDF_TRUE); api.qpdf_set_attempt_recovery(q, QPDF_FALSE); }
 ~Document() { if(q) api.qpdf_cleanup(&q); }
 Document(const Document &) = delete;
};
std::vector<std::string> keys(qpdf_data q, qpdf_oh h) {
 std::vector<std::string> result;
 api.qpdf_oh_begin_dict_key_iter(q,h);
 while(api.qpdf_oh_dict_more_keys(q)) { require(result.size()<Limits::items,"pdf_item_limit"); result.emplace_back(api.qpdf_oh_dict_next_key(q)); }
 std::sort(result.begin(),result.end()); return result;
}
void append(std::string &out, const char *data, std::size_t len) {
 require(len <= Limits::output_bytes && out.size() <= Limits::output_bytes-len,"pdf_expansion_limit"); out.append(data,len);
}
// Stable structural page representation, independent of indirect object numbers.
// Raw streams are preserved by the writer, so identical bytes validate resources
// as well as content. Cyclic structures exceed the explicit depth bound.
void canonical(qpdf_data q, qpdf_oh h, std::string &out, std::size_t &items, std::size_t depth=0, bool page=false, bool stream_dictionary=false) {
 require(depth<=Limits::depth,"pdf_depth_limit"); require(++items<=Limits::items,"pdf_item_limit");
 if(api.qpdf_oh_is_stream(q,h)) {
  canonical(q,api.qpdf_oh_get_dict(q,h),out,items,depth+1,false,true);
  unsigned char *data=nullptr; size_t n=0;
  auto rc=api.qpdf_oh_get_stream_data(q,h,qpdf_dl_none,nullptr,&data,&n);
  std::unique_ptr<unsigned char,void(*)(unsigned char*)> owned(data,[](unsigned char *p){api.qpdf_oh_free_buffer(&p);});
  require(rc==0,"pdf_unsupported_stream"); append(out,"stream",6); append(out,reinterpret_cast<const char*>(data),n); append(out,"endstream",9);
 } else if(api.qpdf_oh_is_dictionary(q,h)) {
  append(out,"<<",2);
  for(const auto &key:keys(q,h)) {
   if((stream_dictionary && key=="/Length") || (page && (key=="/Parent" || key=="/Rotate"))) continue;
   append(out,key.data(),key.size()); canonical(q,api.qpdf_oh_get_key(q,h,key.c_str()),out,items,depth+1);
  } append(out,">>",2);
 } else if(api.qpdf_oh_is_array(q,h)) {
  const int count=api.qpdf_oh_get_array_n_items(q,h); require(count>=0 && count<=static_cast<int>(Limits::items),"pdf_item_limit");
  append(out,"[",1); for(int i=0;i<count;++i) canonical(q,api.qpdf_oh_get_array_item(q,h,i),out,items,depth+1); append(out,"]",1);
 } else { const char *value=api.qpdf_oh_unparse(q,h); require(value!=nullptr,"pdf_malformed"); append(out,value,std::strlen(value)); append(out," ",1); }
 check(q);
}
int rotation(qpdf_data q,qpdf_oh page) {
 auto h=api.qpdf_oh_get_key(q,page,"/Rotate"); if(api.qpdf_oh_is_null(q,h)) return 0;
 require(api.qpdf_oh_is_integer(q,h),"pdf_unsupported_rotation"); auto r=api.qpdf_oh_get_int_value(q,h);
 require(r%90==0 && r>=-36000 && r<=36000,"pdf_unsupported_rotation"); return static_cast<int>((r%360+360)%360);
}
std::map<std::string,std::string> metadata(qpdf_data q) {
 std::map<std::string,std::string> result;
 auto info=api.qpdf_oh_get_key(q,api.qpdf_get_trailer(q),"/Info");
 if(api.qpdf_oh_is_null(q,info)) return result;
 require(api.qpdf_oh_is_dictionary(q,info),"pdf_unsupported_metadata");
 for(const auto &key:keys(q,info)) {
  require(key.size()>1 && info_keys.count(key.substr(1)),"pdf_unsupported_metadata");
  auto h=api.qpdf_oh_get_key(q,info,key.c_str());
  require(api.qpdf_oh_is_string(q,h),"pdf_unsupported_metadata");
  size_t n=0; const char *v=api.qpdf_oh_get_binary_utf8_value(q,h,&n);
  require(v && n<=4096 && std::memchr(v,0,n)==nullptr,"pdf_metadata_limit"); result[key.substr(1)]=std::string(v,n);
 } check(q); return result;
}
void read(Document &d,const Bytes &bytes) {
 require(bytes.size()>=8 && std::memcmp(bytes.data(),"%PDF-",5)==0,"pdf_signature");
 auto rc=api.qpdf_read_memory(d.q,"local-document",reinterpret_cast<const char*>(bytes.data()),bytes.size(),nullptr);
 if(rc!=0 && api.qpdf_has_error(d.q)) {
  auto e=api.qpdf_get_error(d.q); require(api.qpdf_get_error_code(d.q,e)!=qpdf_e_password,"pdf_encrypted"); throw Problem{"pdf_malformed"};
 }
 require(!api.qpdf_is_encrypted(d.q),"pdf_encrypted"); check(d.q);
 auto root=api.qpdf_get_root(d.q);
 require(!api.qpdf_oh_has_key(d.q,root,"/Perms"),"pdf_signed");
 // Only static page trees are supported; document-level behavior cannot be
 // silently lost when constructing a new document from selected pages.
 for(const auto &key:keys(d.q,root)) require(key=="/Type" || key=="/Pages" || key=="/Version", "pdf_unsupported_catalog");
 require(api.qpdf_push_inherited_attributes_to_page(d.q)==0,"pdf_malformed");
 int count=api.qpdf_get_num_pages(d.q); require(count>0 && count<=static_cast<int>(max_pages),"pdf_page_limit");
 for(int i=0;i<count;++i) {
  auto page=api.qpdf_get_page_n(d.q,i);
  require(!api.qpdf_oh_has_key(d.q,page,"/Annots") && !api.qpdf_oh_has_key(d.q,page,"/AA"),"pdf_unsupported_page_features");
  rotation(d.q,page);
 }
 metadata(d.q); check(d.q);
}
struct Page { qpdf_data q; qpdf_oh h; int rotate; };
Bytes write(const std::vector<Page> &pages,const std::map<std::string,std::string> &meta) {
 Document out;
 require(api.qpdf_empty_pdf(out.q)==0,"pdf_write_failed");
 for(const auto &p:pages) {
  require(api.qpdf_add_page(out.q,p.q,p.h,QPDF_FALSE)==0,"pdf_write_failed");
  auto added=api.qpdf_get_page_n(out.q,api.qpdf_get_num_pages(out.q)-1);
  api.qpdf_oh_replace_key(out.q,added,"/Rotate",api.qpdf_oh_new_integer(out.q,p.rotate));
 }
 if(!meta.empty()) {
  auto info=api.qpdf_oh_new_dictionary(out.q);
  for(const auto &kv:meta) api.qpdf_oh_replace_key(out.q,info,("/"+kv.first).c_str(),api.qpdf_oh_new_unicode_string(out.q,kv.second.c_str()));
  api.qpdf_oh_replace_key(out.q,api.qpdf_get_trailer(out.q),"/Info",info);
 }
 check(out.q); require(api.qpdf_init_write_memory(out.q)==0,"pdf_write_failed");
 api.qpdf_set_stream_data_mode(out.q,qpdf_s_preserve); api.qpdf_set_object_stream_mode(out.q,qpdf_o_disable);
 require(api.qpdf_write(out.q)==0,"pdf_write_failed"); check(out.q);
 auto size=api.qpdf_get_buffer_length(out.q); require(size>0 && size<=Limits::output_bytes,"pdf_output_limit");
 auto data=api.qpdf_get_buffer(out.q); require(data!=nullptr,"pdf_write_failed"); Bytes bytes(data,data+size);
 Document reopened; read(reopened,bytes);
 require(api.qpdf_get_num_pages(reopened.q)==static_cast<int>(pages.size()),"pdf_validation_page_count");
 require(metadata(reopened.q)==meta,"pdf_validation_metadata");
 for(std::size_t i=0;i<pages.size();++i) {
  auto h=api.qpdf_get_page_n(reopened.q,i); require(rotation(reopened.q,h)==pages[i].rotate,"pdf_validation_rotation");
  std::string before,after; std::size_t n=0; canonical(pages[i].q,pages[i].h,before,n,0,true); n=0; canonical(reopened.q,h,after,n,0,true);
  require(before==after,"pdf_validation_page_order");
 } return bytes;
}
}
bool pdf_install_engine(void *module) {
 api=Api{};
#ifdef _WIN32
 if(!module) return false;
#define LOAD(n) api.n = reinterpret_cast<decltype(api.n)>(GetProcAddress(static_cast<HMODULE>(module),#n)); if(!api.n) { api=Api{}; return false; }
 PDF_SYMBOLS(LOAD)
#undef LOAD
 if(std::strcmp(api.qpdf_get_qpdf_version(),"12.4.2")!=0) {api=Api{};return false;}
 api.ready=true; return true;
#else
 (void)module; return false;
#endif
}
PdfResult pdf_transform(PdfOperation op,const std::vector<Bytes> &sources,const PdfOptions &options) {
 PdfResult result;
 try {
  require(api.ready,"pdf_engine_unavailable"); require(!sources.empty() && sources.size()<=max_pages,"pdf_source_limit");
  require(op==PdfOperation::Merge || sources.size()==1,"pdf_source_count");
  require(static_cast<unsigned>(op)<=static_cast<unsigned>(PdfOperation::Metadata),"pdf_operation");
  require(options.pages.size()<=max_pages,"pdf_page_limit");
  require(options.rotation==0 || options.rotation==90 || options.rotation==180 || options.rotation==270,"pdf_rotation_option");
  std::size_t input=0; std::vector<std::unique_ptr<Document>> documents; std::vector<Page> all;
  for(const auto &bytes:sources) {
   require(bytes.size()<=Limits::input_bytes && input<=Limits::input_bytes-bytes.size(),"pdf_input_limit"); input+=bytes.size();
   auto d=std::make_unique<Document>(); read(*d,bytes);
   int count=api.qpdf_get_num_pages(d->q); require(all.size()+count<=max_pages,"pdf_page_limit");
   for(int i=0;i<count;++i) {auto h=api.qpdf_get_page_n(d->q,i); all.push_back({d->q,h,rotation(d->q,h)});}
   documents.push_back(std::move(d));
  }
  auto meta=metadata(documents.front()->q);
  if(op==PdfOperation::Merge) for(const auto &d:documents) require(metadata(d->q)==meta,"pdf_merge_metadata_conflict");
  require(options.metadata.size()<=info_keys.size(),"pdf_metadata_limit");
  for(const auto &kv:options.metadata) {
   require(op==PdfOperation::Metadata,"pdf_unexpected_metadata"); require(info_keys.count(kv.first),"pdf_metadata_key");
   require(kv.second.size()<=4096 && kv.second.find('\0')==std::string::npos && valid_utf8(Bytes(kv.second.begin(),kv.second.end())),"pdf_metadata_value"); meta[kv.first]=kv.second;
  }
  std::vector<Page> selected;
  if(op==PdfOperation::Extract || op==PdfOperation::Reorder) {
   require(!options.pages.empty(),"pdf_selection_required"); std::set<std::size_t> unique;
   for(auto n:options.pages) {require(n>0 && n<=all.size(),"pdf_page_range");require(unique.insert(n).second,"pdf_duplicate_page");selected.push_back(all[n-1]);}
   if(op==PdfOperation::Reorder) require(selected.size()==all.size(),"pdf_reorder_permutation");
  } else selected=all;
  if(op==PdfOperation::Rotate) {
   std::set<std::size_t> unique;
   for(auto n:options.pages) {require(n>0 && n<=selected.size(),"pdf_page_range"); require(unique.insert(n).second,"pdf_duplicate_page");}
   for(std::size_t i=0;i<selected.size();++i) if(unique.empty() || unique.count(i+1)) selected[i].rotate=options.rotation;
  } else require(options.rotation==0,"pdf_unexpected_rotation");
  if(op!=PdfOperation::Extract && op!=PdfOperation::Reorder && op!=PdfOperation::Rotate) require(options.pages.empty(),"pdf_unexpected_selection");
  result.page_count=selected.size(); result.metadata=meta;
  for(const auto &p:selected) result.rotations.push_back(p.rotate);
  // Inspection walks every page too, so unsupported graphs do not appear valid.
  for(const auto &p:all) {std::string value;std::size_t n=0;canonical(p.q,p.h,value,n,0,true);}
  if(op==PdfOperation::Split) {
   std::size_t total=0;
   for(const auto &p:selected) {auto bytes=write({p},meta);require(total<=Limits::output_bytes-bytes.size(),"pdf_output_limit");total+=bytes.size();result.outputs.push_back(std::move(bytes));}
  } else if(op!=PdfOperation::Inspect) result.outputs.push_back(write(selected,meta));
  result.outcome=Outcome::Converted; result.code="ok";
 } catch(const Problem &p) {result=PdfResult{};result.code=p.code;} catch(const std::bad_alloc &) {result=PdfResult{};result.code="pdf_memory_limit";} catch(...) {result=PdfResult{};result.code="pdf_internal_error";}
 return result;
}
}
