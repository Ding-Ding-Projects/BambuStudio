#include "libslic3r/LocalConverter/PdfAdapter.hpp"
#define NOMINMAX
#include <windows.h>
#include <cstdio>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace Slic3r::LocalConverter;
namespace {
int assertions=0;
void expect(bool yes,const char *what) {++assertions;if(!yes) throw std::runtime_error(what);}
Bytes fixture(int count=3,bool active=false,bool signed_pdf=false) {
 std::vector<std::string> objects;
 objects.push_back(std::string("<< /Type /Catalog /Pages 2 0 R")+(active?" /OpenAction [3 0 R /Fit]":"")+(signed_pdf?" /Perms << /DocMDP << /Type /Sig /ByteRange [0 1 2 3] >> >>":"")+" >>");
 std::string pages="<< /Type /Pages /Count "+std::to_string(count)+" /Kids [";
 for(int i=0;i<count;++i) pages+=std::to_string(3+i*2)+" 0 R ";
 objects.push_back(pages+"] >>");
 for(int i=0;i<count;++i) {
  objects.push_back("<< /Type /Page /Parent 2 0 R /MediaBox [0 0 "+std::to_string(100+i*10)+" 200] /Resources << >> /Contents "+std::to_string(4+i*2)+" 0 R >>");
  std::string content="q\nQ\n";
  objects.push_back("<< /Length "+std::to_string(content.size())+" >>\nstream\n"+content+"endstream");
 }
 std::string s="%PDF-1.4\n";std::vector<size_t> offsets={0};
 for(size_t i=0;i<objects.size();++i) {offsets.push_back(s.size());s+=std::to_string(i+1)+" 0 obj\n"+objects[i]+"\nendobj\n";}
 auto xref=s.size();s+="xref\n0 "+std::to_string(offsets.size())+"\n0000000000 65535 f \n";
 for(size_t i=1;i<offsets.size();++i) {char line[40];std::snprintf(line,sizeof(line),"%010llu 00000 n \n",static_cast<unsigned long long>(offsets[i]));s+=line;}
 s+="trailer\n<< /Size "+std::to_string(offsets.size())+" /Root 1 0 R >>\nstartxref\n"+std::to_string(xref)+"\n%%EOF\n";
 return Bytes(s.begin(),s.end());
}
void success(const PdfResult &r,const char *operation) {if(r.code!="ok")std::cerr<<operation<<": "<<r.code<<"\n";expect(r.outcome==Outcome::Converted,operation);}
}
int wmain(int argc,wchar_t **argv) {
 try {
  expect(pdf_transform(PdfOperation::Inspect,{fixture()}).code=="pdf_engine_unavailable","engine unavailable");
  expect(argc==2,"pass absolute test DLL path");
  HMODULE module=LoadLibraryExW(argv[1],nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);
  expect(module && pdf_install_engine(module),"load pinned test engine");
  auto source=fixture();
  auto inspect=pdf_transform(PdfOperation::Inspect,{source});success(inspect,"inspect");expect(inspect.page_count==3 && inspect.outputs.empty(),"inspection facts");
  auto split=pdf_transform(PdfOperation::Split,{source});success(split,"split");expect(split.outputs.size()==3,"split outputs");
  for(const auto &p:split.outputs)expect(pdf_transform(PdfOperation::Inspect,{p}).page_count==1,"split page count");
  auto merge=pdf_transform(PdfOperation::Merge,split.outputs);success(merge,"merge");expect(pdf_transform(PdfOperation::Inspect,merge.outputs).page_count==3,"merge page count");
  PdfOptions options;options.pages={3,1};auto extract=pdf_transform(PdfOperation::Extract,{source},options);success(extract,"extract");expect(extract.page_count==2,"extract count");
  options.pages={3,1,2};auto reorder=pdf_transform(PdfOperation::Reorder,{source},options);success(reorder,"reorder");
  // Structural validator compares dimensions as well as stream bytes, detecting
  // reordering of these otherwise visually identical pages.
  options.pages={2};options.rotation=90;auto rotate=pdf_transform(PdfOperation::Rotate,{source},options);success(rotate,"rotate");
  expect(pdf_transform(PdfOperation::Inspect,rotate.outputs).rotations==std::vector<int>({0,90,0}),"rotation reopen");
  options={};options.metadata={{"Title",u8"Round trip 廣東話"},{"Author","Converter"}};
  auto meta=pdf_transform(PdfOperation::Metadata,{source},options);success(meta,"metadata");expect(pdf_transform(PdfOperation::Inspect,meta.outputs).metadata==options.metadata,"metadata reopen");
  options={};options.pages={1,1};expect(pdf_transform(PdfOperation::Extract,{source},options).code=="pdf_duplicate_page","duplicate selection");
  options.pages={0};expect(pdf_transform(PdfOperation::Extract,{source},options).code=="pdf_page_range","zero page");
  options.pages={4};expect(pdf_transform(PdfOperation::Extract,{source},options).code=="pdf_page_range","page beyond document");
  options.pages={1};expect(pdf_transform(PdfOperation::Reorder,{source},options).code=="pdf_reorder_permutation","incomplete reorder");
  options={};options.rotation=45;expect(pdf_transform(PdfOperation::Rotate,{source},options).code=="pdf_rotation_option","invalid rotation");
  expect(pdf_transform(PdfOperation::Inspect,{fixture(3,true)}).code=="pdf_unsupported_catalog","active content rejected");
  expect(pdf_transform(PdfOperation::Inspect,{fixture(3,false,true)}).code=="pdf_signed","signature container rejected");
  expect(pdf_transform(PdfOperation::Inspect,{Bytes{'x'}}).code=="pdf_signature","bad signature");
  auto damaged=source;damaged.resize(damaged.size()/2);expect(pdf_transform(PdfOperation::Inspect,{damaged}).code=="pdf_malformed","truncation rejected");
  auto tooLarge=source;tooLarge.resize(Limits::input_bytes+1);expect(pdf_transform(PdfOperation::Inspect,{tooLarge}).code=="pdf_input_limit","input budget");
  expect(pdf_transform(PdfOperation::Merge,{source,meta.outputs[0]}).code=="pdf_merge_metadata_conflict","metadata loss rejected");
  expect(pdf_transform(PdfOperation::Inspect,{source,source}).code=="pdf_source_count","single source operations");
  std::cout<<"PDF adapter assertions: "<<assertions<<" passed\n";return 0;
 } catch(const std::exception &e) {std::cerr<<e.what()<<"\n";return 1;}
}
