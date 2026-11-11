#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <vector>

namespace google::protobuf {
class MessageLite;
}

namespace otu {

// Spritesheet atlas size for legacy clients (Tibia 11+ ships its own 384x384 sheets).
constexpr int kSheetDimension = 512;

// Converts <version>/Tibia.dat + Tibia.spr into <version>/result/appearances.otud + assets.otus.
bool convertLegacy(int clientVersion, bool useAlpha);

// Converts the Tibia 11+ assets in <version>/<build>/assets into <version>/<build>/result.
bool convertProtobuf(int clientVersion, int buildVersion);

// One spritesheet record of assets.otus.
struct SheetEntry {
    uint32_t atlasId = 0;
    uint16_t spriteType = 0; // 1: 32x32, 2: 32x64, 3: 64x32, 4: 64x64
    uint32_t firstSpriteId = 0;
    uint32_t lastSpriteId = 0;
    std::vector<uint8_t> png;
};

// assets.otus: u32 sheet count, then per sheet u32 atlas id, u16 sprite type, u32 first and
// last sprite id, u32 PNG size and the PNG itself (all little-endian).
void writeAssets(const std::filesystem::path& path, const std::vector<const SheetEntry*>& sheets);
void writeMessage(const std::filesystem::path& path, const google::protobuf::MessageLite& message);

std::vector<uint8_t> readFile(const std::filesystem::path& path);
void writeFile(const std::filesystem::path& path, const void* data, size_t size);

} // namespace otu
