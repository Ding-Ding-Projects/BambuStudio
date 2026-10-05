#include "QrImport.hpp"
#include <ZXing/ReadBarcode.h>
#include <ZXing/ReaderOptions.h>
#include <openssl/crypto.h>
namespace Slic3r::LocalSecurity {
Secret decode_qr_luminance(unsigned width,unsigned height,const std::vector<unsigned char>& pixels){
    if(!width||!height||width>4096||height>4096||std::uint64_t(width)*height>4*1024*1024||pixels.size()!=std::size_t(width)*height)throw Failure(Error::InvalidInput);
    ZXing::ReaderOptions options;options.setFormats(ZXing::BarcodeFormat::QRCode).setTryHarder(false).setTryRotate(true).setTryInvert(true).setMaxNumberOfSymbols(2);
    const ZXing::ImageView image(pixels.data(),static_cast<int>(width),static_cast<int>(height),ZXing::ImageFormat::Lum);
    auto results=ZXing::ReadBarcodes(image,options);
    if(results.size()!=1||!results.front().isValid()||results.front().isPartOfSequence())throw Failure(Error::InvalidInput);
    auto uri=results.front().text(ZXing::TextMode::Plain);
    struct Wipe{std::string& value;~Wipe(){if(!value.empty())OPENSSL_cleanse(value.data(),value.size());}} wipe{uri};
    if(uri.size()>8192)throw Failure(Error::InvalidInput);
    // Validate all enrollment parameters before crossing back to the host.
    auto validated=parse_otpauth(uri);(void)validated;
    return Secret(uri);
}
}
