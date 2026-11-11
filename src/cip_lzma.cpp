#include "cip_lzma.h"

#include "binary_stream.h"

#include <cstdlib>
#include <lzma.h>
#include <memory>
#include <stdexcept>
#include <string>

namespace otu {

std::vector<uint8_t> decompressCipLzma(const std::vector<uint8_t>& file) {
    BinaryReader in(file);

    // CIP header: zero padding, a constant 5-byte signature (whose first byte ends the
    // padding loop), then the compressed size as a 7-bit encoded integer.
    while (in.u8() == 0) {
    }
    in.skip(4);
    while ((in.u8() & 0x80) == 0x80) {
    }

    // .lzma-style header: 5 property bytes and an 8-byte size the decoder does not need.
    const uint8_t* properties = in.current();
    in.skip(5 + 8);

    lzma_filter filters[2] = {{LZMA_FILTER_LZMA1, nullptr}, {LZMA_VLI_UNKNOWN, nullptr}};
    if (lzma_properties_decode(&filters[0], nullptr, properties, 5) != LZMA_OK)
        throw std::runtime_error("invalid LZMA properties");
    std::unique_ptr<void, decltype(&std::free)> options(filters[0].options, &std::free);

    lzma_stream stream = LZMA_STREAM_INIT;
    if (lzma_raw_decoder(&stream, filters) != LZMA_OK)
        throw std::runtime_error("cannot initialise LZMA decoder");

    std::vector<uint8_t> out(0x100000);
    stream.next_in = in.current();
    stream.avail_in = in.remaining();
    stream.next_out = out.data();
    stream.avail_out = out.size();
    const lzma_ret ret = lzma_code(&stream, LZMA_FINISH);
    out.resize(out.size() - stream.avail_out);
    lzma_end(&stream);

    // STREAM_END: end-of-payload marker reached. OK / BUF_ERROR: output cap reached or input
    // ended without a marker; like the C# decoder, keep what was decoded.
    if (ret != LZMA_STREAM_END && ret != LZMA_OK && ret != LZMA_BUF_ERROR)
        throw std::runtime_error("LZMA decoding failed (error " + std::to_string(ret) + ")");
    return out;
}

} // namespace otu
