#pragma once

#include <cstdint>
#include <vector>

namespace otu {

// Unpacks a Tibia 11+ sprite sheet file: CIP's header followed by a raw LZMA1 stream that
// decompresses to a BMP image. Output is capped at 1 MiB, as in the C# version.
std::vector<uint8_t> decompressCipLzma(const std::vector<uint8_t>& file);

} // namespace otu
