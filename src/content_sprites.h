#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace otu {

constexpr int kSpriteSize = 32;
constexpr size_t kSpriteBytes = kSpriteSize * kSpriteSize * 4;

// Tibia.spr of a legacy (< 11) client.
class ContentSprites {
public:
    ContentSprites(std::vector<uint8_t> buffer, int clientVersion, bool useAlpha);

    uint32_t signature = 0;
    uint32_t spritesCount = 0;

    // Decodes a 32x32 sprite into `rgba` (kSpriteBytes, transparent pixels zeroed).
    // Returns false for id 0 and empty sprites. Safe to call from several threads.
    bool getSprite(uint32_t id, uint8_t* rgba) const;

    // Re-encodes this .spr in the format of another client version.
    std::vector<uint8_t> convertTo(int newVersion) const;

    static uint32_t signatureFor(int version);

private:
    std::vector<uint8_t> m_buffer;
    int m_clientVersion;
    bool m_useAlpha;
    size_t m_spritesOffset = 0;
};

} // namespace otu
