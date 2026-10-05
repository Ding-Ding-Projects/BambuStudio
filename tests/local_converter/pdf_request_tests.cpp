#include "libslic3r/LocalConverter/PdfRequest.hpp"
#include "libslic3r/LocalConverter/PdfAdapter.hpp"
#include "libslic3r/LocalConverter/Worker.hpp"
#include "libslic3r/LocalConverter/PdfPackage.hpp"
#include "miniz/miniz.h"
#include <nlohmann/json.hpp>
#define NOMINMAX
#include <windows.h>
#include <cstdio>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace Slic3r::LocalConverter;
using Json=nlohmann::ordered_json;
namespace {
int assertions=0;
void expect(bool yes,const char *label){++assertions;if(!yes)throw std::runtime_error(label);}
Bytes bytes(const std::string &s){return Bytes(s.begin(),s.end());}
Bytes fixture(){
 const std::vector<std::string> objects={"<< /Type /Catalog /Pages 2 0 R >>","<< /Type /Pages /Count 2 /Kids [3 0 R 4 0 R] >>","<< /Type /Page /Parent 2 0 R /MediaBox [0 0 100 200] /Resources << >> >>","<< /Type /Page /Parent 2 0 R /MediaBox [0 0 300 400] /Resources << >> >>"};
 std::string output="%PDF-1.4\n";std::vector<std::size_t> offsets;
 for(std::size_t i=0;i<objects.size();++i){offsets.push_back(output.size());output+=std::to_string(i+1)+" 0 obj\n"+objects[i]+"\nendobj\n";}
 const auto xref=output.size();output+="xref\n0 5\n0000000000 65535 f \n";
 for(auto offset:offsets){char line[40];std::snprintf(line,sizeof(line),"%010llu 00000 n \n",static_cast<unsigned long long>(offset));output+=line;}
 output+="trailer\n<< /Size 5 /Root 1 0 R >>\nstartxref\n"+std::to_string(xref)+"\n%%EOF\n";return bytes(output);
}
std::string encoded(const Bytes &data){auto result=transform("base64.encode",data);expect(result.outcome==Outcome::Converted,"fixture encoding");return std::string(result.output.begin(),result.output.end());}
Conversion run(const std::string &adapter,const Json &j){return pdf_request_transform(adapter,bytes(j.dump()));}
void success(const Conversion &c,const char *label){if(c.outcome!=Outcome::Converted)std::cerr<<label<<": "<<c.code<<"\n";expect(c.outcome==Outcome::Converted,label);}
void rejected(const std::string &adapter,const Json &j,const std::string &code){auto c=run(adapter,j);if(c.code!=code)std::cerr<<"Expected "<<code<<", got "<<c.code<<"\n";expect(c.outcome==Outcome::Failed && c.code==code && c.output.empty(),code.c_str());}
}
int wmain(int argc,wchar_t **argv){
 try {
  expect(argc==2||argc==4,"absolute engine path required");auto module=LoadLibraryExW(argv[1],nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);expect(module && pdf_install_engine(module),"install engine");
  const auto source=fixture();Json request={{"sources",Json::array({encoded(source)})},{"options",Json::object()}};
  auto inspect=run("pdf.inspect",request);success(inspect,"inspect");auto information=Json::parse(inspect.output);expect(information.at("page_count")==2 && information.at("rotations")==Json::array({0,0}),"inspection response");
  auto split=run("pdf.split",request);success(split,"split");expect(split.output.size()>4 && split.output[0]=='P' && split.output[1]=='K',"ZIP signature");
  mz_zip_archive archive{};expect(mz_zip_reader_init_mem(&archive,split.output.data(),split.output.size(),0)!=0,"reopen split ZIP");
  struct End {mz_zip_archive *a;~End(){mz_zip_reader_end(a);}} end{&archive};
  expect(mz_zip_reader_get_num_files(&archive)==2,"split count");
  std::vector<Bytes> pages;
  for(unsigned i=0;i<2;++i){mz_zip_archive_file_stat stat{};expect(mz_zip_reader_file_stat(&archive,i,&stat)!=0,"ZIP entry stat");expect(std::string(stat.m_filename)=="page-"+std::to_string(i+1)+".pdf","ZIP safe filename");Bytes page(static_cast<std::size_t>(stat.m_uncomp_size));expect(mz_zip_reader_extract_to_mem(&archive,i,page.data(),page.size(),0)!=0,"ZIP extract");expect(pdf_transform(PdfOperation::Inspect,{page}).page_count==1,"split PDF reopen");pages.push_back(std::move(page));}
  auto direct=pdf_transform(PdfOperation::Split,{source});expect(direct.outcome==Outcome::Converted && direct.outputs.size()==2,"typed split reference");
  // qpdf's generated document IDs may vary, so compare page structure through
  // the adapter's reopen validator rather than an unrelated byte reproduction.
  Json merge=request;merge["sources"]=Json::array({encoded(pages[1]),encoded(pages[0])});auto merged=run("pdf.merge",merge);success(merged,"merge");expect(pdf_transform(PdfOperation::Inspect,{merged.output}).page_count==2,"merged reopen");
  Json change=request;change["options"]={{"pages",Json::array({2})}};auto extract=run("pdf.extract",change);success(extract,"extract");expect(pdf_transform(PdfOperation::Inspect,{extract.output}).page_count==1,"extract reopen");
  change["options"]={{"pages",Json::array({2,1})}};success(run("pdf.reorder",change),"reorder");
  change["options"]={{"pages",Json::array({2})},{"rotation",270}};auto rotated=run("pdf.rotate",change);success(rotated,"rotate");expect(pdf_transform(PdfOperation::Inspect,{rotated.output}).rotations==std::vector<int>({0,270}),"rotation reopen");
  change["options"]={{"metadata",{{"Title",u8"Request 廣東話"}}}};auto meta=run("pdf.metadata",change);success(meta,"metadata");expect(pdf_transform(PdfOperation::Inspect,{meta.output}).metadata.at("Title")==u8"Request 廣東話","Unicode metadata");
  rejected("pdf.unknown",request,"pdf_request_operation");rejected("pdf.inspect",Json::array(),"pdf_request_object");rejected("pdf.inspect",Json::object(),"pdf_request_sources");
  change=request;change["unexpected"]=1;rejected("pdf.inspect",change,"pdf_request_unknown_option");
  change=request;change["options"]={{"unknown",true}};rejected("pdf.inspect",change,"pdf_request_unknown_option");
  change=request;change["options"]={{"rotation",0}};rejected("pdf.inspect",change,"pdf_unexpected_rotation");
  change["options"]={{"metadata",Json::object()}};rejected("pdf.rotate",change,"pdf_unexpected_metadata");
  change["options"]={{"pages",Json::array()}};rejected("pdf.split",change,"pdf_unexpected_selection");
  change["options"]={{"rotation",std::numeric_limits<std::uint64_t>::max()}};rejected("pdf.rotate",change,"pdf_rotation_option");
  change["options"]={{"rotation",90.0}};rejected("pdf.rotate",change,"pdf_rotation_option");
  change["options"]={{"rotation",-90}};rejected("pdf.rotate",change,"pdf_rotation_option");
  change["options"]={{"pages",Json::array({std::numeric_limits<std::uint64_t>::max()})}};rejected("pdf.extract",change,"pdf_page_range");
  change["options"]={{"pages",Json::array({true})}};rejected("pdf.extract",change,"pdf_request_page_type");
  change["options"]={{"metadata",{{"Title",42}}}};rejected("pdf.metadata",change,"pdf_request_metadata_value");
  change["options"]={{"metadata",{{"Title",std::string(4097,'x')}}}};rejected("pdf.metadata",change,"pdf_metadata_limit");
  change=request;change["sources"]=Json::array();rejected("pdf.inspect",change,"pdf_source_limit");
  change["sources"]=Json::array({3});rejected("pdf.inspect",change,"pdf_request_source_type");
  for(const std::string bad:{"","Zg","Zh==","Zg==Zg==","Z g=","=AAA","AAAA=AAA"}){change["sources"]=Json::array({bad});rejected("pdf.inspect",change,"pdf_request_base64");}
  auto duplicate=pdf_request_transform("pdf.inspect",bytes("{\"sources\":[],\"sources\":[]}"));expect(duplicate.code=="pdf_request_duplicate_key","duplicate root keys");
  auto nestedDuplicate=pdf_request_transform("pdf.metadata",bytes("{\"sources\":[],\"options\":{\"metadata\":{\"Title\":\"a\",\"Title\":\"b\"}}}"));expect(nestedDuplicate.code=="pdf_request_duplicate_key","duplicate metadata keys");
  expect(pdf_request_transform("pdf.inspect",bytes("{")).code=="pdf_request_json","invalid JSON");
  Bytes invalidUtf8={'{',0xff,'}'};expect(pdf_request_transform("pdf.inspect",invalidUtf8).code=="pdf_request_utf8","invalid UTF8");
  expect(pdf_request_transform("pdf.inspect",Bytes(24*1024*1024+1,' ')).code=="pdf_envelope_limit","envelope bound");
  expect(pdf_request_transform("pdf.inspect",bytes(std::string(65,'[')+std::string(65,']'))).code=="pdf_request_depth_limit","preparse depth bound");
  std::string many="[";for(int i=0;i<100001;++i)many+="0,";many+="0]";expect(pdf_request_transform("pdf.inspect",bytes(many)).code=="pdf_request_item_limit","parser item bound");
  // This exceeds the core adapter's single-call byte limit but remains a valid
  // request envelope. Reaching PDF signature validation proves chunk decoding.
  change=request;change["sources"]=Json::array({std::string(18*1024*1024,'A')});rejected("pdf.inspect",change,"pdf_signature");
  change["sources"]=Json::array({std::string(23*1024*1024,'A')});rejected("pdf.inspect",change,"pdf_input_limit");
  change["sources"]=Json::array({std::string(12*1024*1024,'A'),std::string(12*1024*1024-4096,'A')});rejected("pdf.merge",change,"pdf_input_limit");
  if(argc==4){
   const std::filesystem::path worker(argv[2]);const std::wstring wide(argv[3]);PackageProof proof{worker.parent_path(),worker,std::string(wide.begin(),wide.end())};std::string reason;
   expect(verify_pdf_package(proof.installed_directory,reason),"compiled PDF package pins");std::atomic<bool> cancel{false};
   for(const auto *operation:{"inspect","split","merge","extract","reorder","rotate","metadata"}){
    Json framed=request;
    const std::string op(operation);
    if(op=="merge")framed["sources"]=Json::array({encoded(source),encoded(source)});
    if(op=="extract")framed["options"]={{"pages",Json::array({2})}};
    if(op=="reorder")framed["options"]={{"pages",Json::array({2,1})}};
    if(op=="rotate")framed["options"]={{"rotation",90}};
    if(op=="metadata")framed["options"]={{"metadata",{{"Title",u8"Isolated 廣東話"}}}};
    auto isolated=isolated_transform(proof,"pdf."+op,bytes(framed.dump()),cancel);success(isolated,("isolated "+op).c_str());
    if(op=="inspect")expect(Json::parse(isolated.output)["page_count"]==2,"isolated inspect facts");
    else if(op=="split")expect(isolated.output.size()>4 && isolated.output[0]=='P' && isolated.output[1]=='K',"isolated split ZIP");
    else expect(pdf_transform(PdfOperation::Inspect,{isolated.output}).outcome==Outcome::Converted,"isolated output reopen");
   }
   cancel=true;expect(isolated_transform(proof,"pdf.inspect",bytes(request.dump()),cancel).outcome==Outcome::Cancelled,"isolated cancellation before launch");
  }
  std::cout<<"PDF request assertions: "<<assertions<<" passed\n";return 0;
 }catch(const std::exception &e){std::cerr<<e.what()<<"\n";return 1;}
}
