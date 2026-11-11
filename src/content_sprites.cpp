#include "content_sprites.h"

#include "binary_stream.h"

#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <string>

namespace otu {

ContentSprites::ContentSprites(std::vector<uint8_t> buffer, int clientVersion, bool useAlpha)
    : m_buffer(std::move(buffer)), m_clientVersion(clientVersion), m_useAlpha(useAlpha) {
    BinaryReader in(m_buffer);
    signature = in.u32();
    spritesCount = clientVersion >= 960 ? in.u32() : in.u16();
    m_spritesOffset = in.position();
}

uint32_t ContentSprites::signatureFor(int version) {
    switch (version) {
    case 770: return 0x439852be;
    case 1098: return 0x57bbd603;
    default: return 0;
    }
}

bool ContentSprites::getSprite(uint32_t id, uint8_t* rgba) const {
    std::memset(rgba, 0, kSpriteBytes);
    if (id == 0 || id > spritesCount)
        return false;

    try {
        BinaryReader in(m_buffer);
        in.seek(m_spritesOffset + size_t(id - 1) * 4);
        const uint32_t address = in.u32();
        if (address == 0)
            return false;

        in.seek(address);
        in.skip(3); // colour key
        const uint16_t pixelDataSize = in.u16();
        const int channels = m_useAlpha ? 4 : 3;

        // Run-length encoded: (transparent count, coloured count, coloured pixels...) repeated.
        size_t writePos = 0;
        int read = 0;
        while (read < pixelDataSize && writePos < kSpriteBytes) {
            const uint16_t transparentPixels = in.u16();
            const uint16_t coloredPixels = in.u16();

            writePos += std::min<size_t>(size_t(transparentPixels) * 4, kSpriteBytes - writePos);
            for (int i = 0; i < coloredPixels && writePos < kSpriteBytes; i++) {
                rgba[writePos++] = in.u8();
                rgba[writePos++] = in.u8();
                rgba[writePos++] = in.u8();
                rgba[writePos++] = m_useAlpha ? in.u8() : 0xFF;
            }

            read += 4 + channels * coloredPixels;
        }
        return true;
    } catch (const std::exception& e) {
        throw std::runtime_error("sprite " + std::to_string(id) + ": " + e.what());
    }
}

std::vector<uint8_t> ContentSprites::convertTo(int newVersion) const {
    BinaryWriter out;
    out.u32(signatureFor(newVersion));

    // Sprite addresses are absolute file offsets, so they shift with the size of the count field.
    int padding;
    if (newVersion >= 960) {
        out.u32(spritesCount);
        padding = m_clientVersion < 960 ? 2 : 0;
    } else {
        out.u16(static_cast<uint16_t>(spritesCount));
        padding = m_clientVersion >= 960 ? -2 : 0;
    }

    BinaryReader in(m_buffer);
    in.seek(m_spritesOffset);
    for (uint32_t i = 0; i < spritesCount; i++) {
        const uint32_t address = in.u32();
        out.u32(address == 0 ? 0 : static_cast<uint32_t>(address + padding));
    }

    out.bytes(in.current(), in.remaining());
    return std::move(out.data());
}

} // namespace otu
