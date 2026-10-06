#include "PairingQr.hpp"
#include "../third_party/qrcodegen.hpp"
#include <openssl/crypto.h>

namespace Slic3r::GUI::LocalSecurityUI {
bool PairingQr::module(unsigned x,unsigned y) const {
    if(x>=modules||y>=modules||dark.size()!=std::size_t(modules)*modules)throw LocalSecurity::Failure(LocalSecurity::Error::InvalidInput);
    return dark[y*modules+x]!=0;
}
PairingQr PairingQr::encode(const LocalSecurity::Enrollment& enrollment) {
    auto uri=LocalSecurity::pairing_uri(enrollment);
    struct Clear {std::string& value;~Clear(){if(!value.empty())OPENSSL_cleanse(value.data(),value.size());}} clear{uri};
    if(uri.size()>2048)throw LocalSecurity::Failure(LocalSecurity::Error::InvalidInput);
    auto qr=qrcodegen::QrCode::encodeText(uri.c_str(),qrcodegen::QrCode::Ecc::MEDIUM);
    PairingQr result;result.modules=static_cast<unsigned>(qr.getSize());result.dark.resize(std::size_t(result.modules)*result.modules);
    for(unsigned y=0;y<result.modules;++y)for(unsigned x=0;x<result.modules;++x)result.dark[y*result.modules+x]=qr.getModule(static_cast<int>(x),static_cast<int>(y))?1:0;
    return result;
}
}
