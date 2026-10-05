#include "slic3r/GUI/LocalSecurity/PairingQr.hpp"
#include <iostream>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif
// Public RFC test material only. The image is piped directly into an independent
// decoder in memory and is never written as a real user's pairing artifact.
int main(){
    using namespace Slic3r::LocalSecurity;using Slic3r::GUI::LocalSecurityUI::PairingQr;
    Enrollment e;e.issuer="Public RFC fixture";e.account="alice@example.test";e.secret=Secret("12345678901234567890123456789012");e.parameters={Algorithm::Sha256,8,45};
    auto qr=PairingQr::encode(e);constexpr unsigned scale=4;auto side=(qr.modules+2*PairingQr::quiet_zone)*scale;
#ifdef _WIN32
    _setmode(_fileno(stdout),_O_BINARY);
#endif
    std::cout<<"P5\n"<<side<<' '<<side<<"\n255\n";
    for(unsigned y=0;y<side;++y)for(unsigned x=0;x<side;++x){unsigned mx=x/scale,my=y/scale;bool inside=mx>=4&&my>=4&&mx<qr.modules+4&&my<qr.modules+4;unsigned char pixel=inside&&qr.module(mx-4,my-4)?0:255;std::cout.write(reinterpret_cast<const char*>(&pixel),1);}
    return std::cout.good()?0:1;
}
