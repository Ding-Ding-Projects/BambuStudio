#include "Converter.hpp"
#ifdef LOCAL_CONVERTER_WITH_PDF
#include "PdfPackage.hpp"
#endif
#include <array>
#include <cstdio>
#include <iostream>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

using namespace Slic3r::LocalConverter;
int main()
{
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY); _setmode(_fileno(stdout), _O_BINARY);
    HANDLE token = nullptr; DWORD length = 0, container = 0;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return 20;
    const bool isolated = GetTokenInformation(token, TokenIsAppContainer, &container, sizeof(container), &length) && container;
    CloseHandle(token); if (!isolated) return 21;
#else
    return 21;
#endif
    std::array<char, 64> adapter{};
    std::uint64_t size = 0;
    std::cin.read(adapter.data(), adapter.size());
    std::cin.read(reinterpret_cast<char *>(&size), sizeof(size));
    if (!std::cin || adapter.back() != 0 || size > Limits::wire_bytes) return 22;
    Bytes input(static_cast<std::size_t>(size));
    std::cin.read(reinterpret_cast<char *>(input.data()), static_cast<std::streamsize>(size));
    if (!std::cin || std::cin.peek() != std::char_traits<char>::eof()) return 23;
    Conversion converted;
    if(std::string(adapter.data())=="worker.capabilities"){
        bool pdf_ready=false;std::string reason="pdf_adapter_not_built";
#ifdef LOCAL_CONVERTER_WITH_PDF
        pdf_ready=load_pdf_engine(reason);
#endif
        const std::string receipt=std::string("{\"core\":true,\"pdf\":")+(pdf_ready?"true":"false")+",\"pdf_reason\":\""+reason+"\"}";
        converted={Outcome::Converted,"capabilities",Bytes(receipt.begin(),receipt.end())};
    }else{
#ifdef LOCAL_CONVERTER_WITH_PDF
    std::string load_code;
    const bool pdf_ready=std::string(adapter.data()).rfind("pdf.",0)!=0 || load_pdf_engine(load_code);
    converted = pdf_ready ? transform(adapter.data(),input) : Conversion{Outcome::Failed,load_code,{}};
#else
    converted = transform(adapter.data(),input);
#endif
    }
    const std::uint32_t status = static_cast<std::uint32_t>(converted.outcome);
    const std::uint64_t output_size = converted.output.size();
    std::array<char, 96> code{};
    if (converted.code.size() >= code.size()) return 24;
    std::copy(converted.code.begin(),converted.code.end(),code.begin());
    std::cout.write(reinterpret_cast<const char *>(&status),sizeof(status));
    std::cout.write(code.data(),code.size());
    std::cout.write(reinterpret_cast<const char *>(&output_size),sizeof(output_size));
    std::cout.write(reinterpret_cast<const char *>(converted.output.data()),static_cast<std::streamsize>(output_size));
    std::cout.flush(); return std::cout ? 0 : 25;
}
