#include "libslic3r/LocalConverter/Converter.hpp"
#include "libslic3r/LocalConverter/Worker.hpp"
#include <nlohmann/json.hpp>
#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>
#include <stdexcept>

using namespace Slic3r::LocalConverter;
namespace fs = std::filesystem;
static int assertions = 0;
void require(bool condition, const char *name) { ++assertions; if (!condition) throw std::runtime_error(name); }
template<class F> void rejects(F fn, const char *name) { bool rejected = false; try { fn(); } catch (...) { rejected = true; } require(rejected,name); }
Bytes b(const std::string &s) { return {s.begin(),s.end()}; }
std::string s(const Bytes &bytes) { return {bytes.begin(),bytes.end()}; }
void put(const fs::path &p, const std::string &v) { std::ofstream out(p,std::ios::binary); out << v; }
std::string get(const fs::path &p) { std::ifstream in(p,std::ios::binary); return {std::istreambuf_iterator<char>(in),{}}; }
void good(const char *id, const std::string &input, const std::string &expected)
{ auto r = transform(id,b(input)); require(r.outcome == Outcome::Converted,id); require(s(r.output) == expected,id); }
void bad(const char *id, const Bytes &input)
{ auto r = transform(id,input); require(r.outcome == Outcome::Failed,id); require(r.output.empty(),"failed conversion exposes no output"); require(result_message(r.code) != nullptr,"every failure code has a translatable message"); }
int main(int argc, char **argv)
{
    auto root = fs::temp_directory_path() / ("bambu-converter-tests-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(root);
    try {
        good("hex.encode",std::string("\0\xffZ",3),"00ff5a"); good("hex.decode","00FF5a",std::string("\0\xffZ",3));
        bad("hex.decode",b("a")); bad("hex.decode",b("xx")); bad("hex.decode",b("00\n"));
        good("base64.encode","hello","aGVsbG8="); good("base64.decode","aGVsbG8=","hello");
        good("base64.encode","",""); good("base64.decode","","");
        for (const auto *invalid : {"A===","AB==","AAA","AA=A","AA==AAAA","AA==\n","!!!!"}) bad("base64.decode",b(invalid));
        good("text.lf","a\r\nb\rc\n","a\nb\nc\n"); good("text.crlf","a\r\nb\rc\n","a\r\nb\r\nc\r\n");
        bad("text.lf",Bytes{0xc0,0x80}); bad("text.lf",Bytes{0xed,0xa0,0x80}); bad("text.lf",Bytes{0xf4,0x90,0x80,0x80});
        require(valid_utf8(Bytes{0xf0,0x9f,0x98,0x80}),"valid four byte UTF8");
        good("json.pretty","{\"x\":1}","{\n  \"x\": 1\n}\n");
        good("json.pretty","[184467440737095516160000,0.123456789012345678901,\"\\u0041\"]","[\n  184467440737095516160000,\n  0.123456789012345678901,\n  \"\\u0041\"\n]\n");
        good("json.pretty","{\"empty\":{},\"array\":[],\"text\":\"{ [ , :\"}","{\n  \"empty\": {},\n  \"array\": [],\n  \"text\": \"{ [ , :\"\n}\n");
        bad("json.pretty",b("{\"x\":1,\"x\":2}")); bad("json.pretty",b("{"));
        bad("json.pretty",b(std::string(70,'[') + "0" + std::string(70,']')));
        auto csv = transform("csv.json",b("a,b\r\n\"x,y\",\"z\"\"q\"\r\n"));
        require(csv.outcome == Outcome::Converted,"CSV conversion");
        require(nlohmann::json::parse(csv.output) == nlohmann::json::parse("[[\"a\",\"b\"],[\"x,y\",\"z\\\"q\"]]"),"CSV preserves cells");
        auto reverse = transform("json.csv",csv.output);
        require(reverse.outcome == Outcome::Converted,"JSON CSV reverse");
        require(transform("csv.json",reverse.output).output == csv.output,"CSV semantic roundtrip");
        good("json.tsv","[[\"a\",\"b\"]]","\"a\"\t\"b\"\r\n");
        bad("csv.json",b("a,b\nc\n")); bad("csv.json",b("\"x\"q")); bad("csv.json",b("\"x"));
        bad("json.csv",b("[[1]]")); bad("json.csv",b("[[]]")); bad("json.tsv",b("{\"a\":1}"));
        const auto ppm = b(std::string("P6\n2 1\n255\n") + std::string("\xff\0\0\0\xff\0",6));
        auto bmp = transform("ppm.bmp",ppm); require(bmp.outcome == Outcome::Converted,"PPM to BMP");
        require(transform("bmp.ppm",bmp.output).output == ppm,"RGB pixel roundtrip");
        bad("ppm.bmp",b("P6\n999999 999999\n255\nx")); bad("ppm.bmp",b("P6\n1 1\n65535\nx"));
        auto corrupt = bmp.output; corrupt[28] = 32; bad("bmp.ppm",corrupt);
        require(detect(b("%PDF-1.7")) == Kind::Pdf,"PDF signature");
        require(detect(b("{\"a\":true}")) == Kind::Json,"JSON signature");
        require(detect(Bytes{0,255}) == Kind::Binary,"binary detection");
        require(detect(b("not a PNG, regardless of extension")) == Kind::Utf8,"no extension guessing");
        require(detect(ppm) == Kind::Ppm,"PPM signature");
        bad("missing",b("x")); bad("hex.encode",Bytes(Limits::input_bytes+1));
        const auto zipped=transform("zip.encode",b("byte-preserving archive"));
        require(zipped.outcome==Outcome::Converted,"ZIP real encode");
        require(transform("zip.decode",zipped.output).output==b("byte-preserving archive"),"ZIP CRC roundtrip");
        bad("zip.decode",b("PK\003\004garbage"));
        auto adapters = catalog({}); require(adapters.size() >= 20,"catalog includes unavailable categories");
        std::array<bool,8> categories{};
        for (const auto &a : adapters) { categories[static_cast<unsigned>(a.category)] = true; require(!a.enabled,"unproven adapter disabled"); require(!a.reason.empty(),"disabled reason explicit"); }
        for (bool covered : categories) require(covered,"all categories visible");
        for (const auto &a : adapters) {
            // Registry text is fixed catalogue English: a runtime diagnostic never
            // joins it, so every reason can be translated as one message.
            require(!a.name.empty() && !a.validator.empty(),"adapter text present");
            require(a.reason.find(": ")==std::string::npos,"no diagnostic concatenated into a reason");
            require(a.detail.empty() || result_message(a.detail) != nullptr,"diagnostic detail is a described code");
        }
        require(result_message("converted") != nullptr && result_message("cancelled") != nullptr,"outcome messages");
        require(result_message("isolated_worker_start_1114") != nullptr && result_message("pdf_system_ui_load_1114") != nullptr,"numbered Windows codes described by prefix");
        require(result_message("isolated_worker_start_") == nullptr && result_message("isolated_worker_start_x") == nullptr,"prefix needs a Windows number");
        require(result_message("no_such_code") == nullptr && result_message("") == nullptr,"unknown codes are reported as unknown");
        require(std::string(state_name(State::RecoveryRequired)) == "Recovery required" && std::string(kind_name(Kind::Utf8)) == "UTF-8 text","state and kind names stay catalogue English");
        std::atomic<bool> cancel{false};
        Executor exec = [](const std::string &a,const Bytes &v,const std::atomic<bool>&){ return transform(a,v); };
        const auto source = root / "input.dat"; put(source,"hello"); const auto output = root / "out.hex";
        auto result = convert_file(source,output,"hex.encode",exec,cancel);
        require(result.outcome == Outcome::Converted && get(output) == "68656c6c6f","atomic conversion");
        require(get(source) == "hello","source unchanged");
        require(convert_file(source,output,"hex.encode",exec,cancel).outcome == Outcome::Skipped,"no overwrite");
        require(get(output) == "68656c6c6f","old destination preserved");
        cancel = true; require(convert_file(source,root/"cancelled.hex","hex.encode",exec,cancel).outcome == Outcome::Cancelled,"preflight cancellation");
        require(!fs::exists(root/"cancelled.hex"),"cancellation creates no output"); cancel = false;
        require(convert_file(source,root/"failed.bin","hex.decode",exec,cancel).outcome == Outcome::Failed,"invalid conversion fails");
        require(!fs::exists(root/"failed.bin"),"failed conversion no partial output");
        {
            Queue queue(root/"queue");
            require(queue.paused(),"new queue paused");
            rejects([&]{Queue second(root/"queue");},"exclusive queue ownership");
            for (unsigned i = 0; i < 125; ++i) queue.enqueue(source,root/("queued-"+std::to_string(i)+".hex"),"hex.encode");
            require(queue.count() == 125,"no total queue cap"); require(queue.page(0).size() == 100,"bounded first page"); require(queue.page(100).size() == 25,"bounded second page");
            rejects([&]{queue.page(0,101);},"reject unbounded page");
            require(!queue.step(exec,cancel),"paused queue does no work"); queue.pause(false);
            require(queue.step(exec,cancel),"resume processes job"); require(queue.page(0,1)[0].state == State::Converted,"completion durable");
            queue.pause(true); require(!queue.step(exec,cancel),"pause respected");
        }
        {
            Queue queue(root/"queue"); require(queue.count() == 125 && queue.paused(),"restart retained and paused");
            require(queue.page(0,1)[0].state == State::Converted,"restart retains result");
            queue.cancel_pending(); require(queue.page(1,1)[0].state == State::Cancelled,"durable cancellation");
            require(queue.page(0,1)[0].state==State::Converted,"cancel preserves prior result");
            queue.retry(2); require(queue.page(1,1)[0].state == State::Pending,"explicit retry");
            put(source,"changed"); queue.pause(false); queue.step(exec,cancel);
            require(queue.page(1,1)[0].code == "source_changed_since_admission","source change rejected after restart");
        }
        put(source,"hello");
        {
            Queue queue(root/"recovery"); queue.enqueue(source,root/"recover.hex","hex.encode");
        }
        const auto interrupted = root/"recovery"/"1.json";
        auto j = nlohmann::json::parse(get(interrupted)); j["state"] = static_cast<unsigned>(State::Running); put(interrupted,j.dump());
        { Queue queue(root/"recovery"); require(queue.page(0,1)[0].state == State::RecoveryRequired,"crash not reported success"); }
        {
            Queue queue(root/"rejected");queue.record_rejected(root/"missing",root/"unwritten","hex.encode","source_unavailable");
            require(queue.page(0,1)[0].state==State::Skipped&&queue.page(0,1)[0].code=="source_unavailable","rejected admission has durable per-file outcome");
            rejects([&]{queue.record_rejected(source,output,"hex.encode","C:/private/path");},"reject private-path diagnostic");
        }
        {
            Queue queue(root/"concurrent");queue.enqueue(source,root/"concurrent1.hex","hex.encode");queue.enqueue(source,root/"concurrent2.hex","hex.encode");queue.pause(false);
            std::atomic<bool> entered{false},release{false};
            Executor blocking=[&](const std::string &id,const Bytes &v,const std::atomic<bool>&){entered=true;while(!release.load())std::this_thread::yield();return transform(id,v);};
            std::thread first([&]{queue.step(blocking,cancel);});
            while(!entered.load())std::this_thread::yield();
            const bool second=queue.step(exec,cancel);queue.pause(true);release=true;first.join();
            require(!second,"bounded concurrency prevents second active job");require(queue.paused(),"pause survives active completion");
            require(queue.page(0,2)[0].state==State::Converted&&queue.page(0,2)[1].state==State::Pending,"only one file ran");
        }
        {
            Executor cancelled=[&](const std::string &id,const Bytes &v,const std::atomic<bool>&){cancel=true;return transform(id,v);};
            const auto late=root/"late-cancel.hex";require(convert_file(source,late,"hex.encode",cancelled,cancel).outcome==Outcome::Cancelled,"cancel before publication");
            require(!fs::exists(late),"late cancellation does not publish");cancel=false;
            const auto raced=root/"raced.hex";
            Executor race=[&](const std::string &id,const Bytes &v,const std::atomic<bool>&){put(raced,"other writer");return transform(id,v);};
            require(convert_file(source,raced,"hex.encode",race,cancel).outcome!=Outcome::Converted,"destination race cannot replace");
            require(get(raced)=="other writer","concurrent destination preserved");
        }
        if (argc == 3) {
            const fs::path worker = fs::u8path(argv[1]); const PackageProof proof{worker.parent_path(),worker,argv[2]};
            std::string why; require(verify_package(proof,why),"real package digest verified");
            auto real = isolated_transform(proof,"hex.encode",b("offline"),cancel);
            if (real.outcome != Outcome::Converted) std::cerr << "worker_code=" << real.code << '\n';
            require(real.outcome == Outcome::Converted && s(real.output) == "6f66666c696e65","AppContainer real conversion");
            auto wrong = proof; wrong.expected_sha256[0] = wrong.expected_sha256[0] == '0' ? '1' : '0';
            require(!verify_package(wrong,why),"wrong digest rejected");
        }
        fs::remove_all(root); std::cout << "PASS " << assertions << " behavioral assertions\n"; return 0;
    } catch (const std::exception &e) { std::cerr << "FAIL: " << e.what() << '\n'; fs::remove_all(root); return 1; }
}
