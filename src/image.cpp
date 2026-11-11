#include "image.h"

#include "binary_stream.h"
#include "content_sprites.h"

#include <bit>
#include <cstring>
#include <png.h>
#include <stdexcept>
#include <string>

namespace otu {

void blitSprite(Image& dst, const uint8_t* sprite, int x, int y) {
    for (int row = 0; row < kSpriteSize; row++) {
        const uint8_t* src = sprite + size_t(row) * kSpriteSize * 4;
        uint8_t* out = dst.rgba.data() + (size_t(y + row) * dst.width + x) * 4;
        for (int col = 0; col < kSpriteSize; col++, src += 4, out += 4)
            if (src[3] != 0)
                std::memcpy(out, src, 4);
    }
}

std::vector<uint8_t> encodePng(const Image& image) {
    png_image png{};
    png.version = PNG_IMAGE_VERSION;
    png.width = static_cast<png_uint_32>(image.width);
    png.height = static_cast<png_uint_32>(image.height);
    png.format = PNG_FORMAT_RGBA;

    std::vector<uint8_t> out(PNG_IMAGE_PNG_SIZE_MAX(png));
    png_alloc_size_t size = out.size();
    if (!png_image_write_to_memory(&png, out.data(), &size, 0, image.rgba.data(), 0, nullptr)) {
        std::string message = png.message;
        png_image_free(&png);
        throw std::runtime_error("PNG encoding failed: " + message);
    }
    out.resize(size);
    return out;
}

namespace {

constexpr uint32_t kBiRgb = 0;
constexpr uint32_t kBiBitfields = 3;
constexpr uint32_t kBiAlphaBitfields = 6;

struct Channel {
    uint32_t mask = 0;
    int shift = 0;
    uint32_t max = 0;

    explicit Channel(uint32_t m) : mask(m) {
        if (mask) {
            shift = std::countr_zero(mask);
            max = mask >> shift;
        }
    }
    uint8_t operator()(uint32_t pixel, uint8_t fallback) const {
        if (!mask)
            return fallback;
        const uint32_t v = (pixel & mask) >> shift;
        return max == 0xFF ? static_cast<uint8_t>(v) : static_cast<uint8_t>((v * 255 + max / 2) / max);
    }
};

} // namespace

Image decodeBmp(const uint8_t* data, size_t size) {
    BinaryReader in(data, size);
    if (in.u8() != 'B' || in.u8() != 'M')
        throw std::runtime_error("not a BMP file");
    in.skip(8);
    const uint32_t pixelOffset = in.u32();

    const uint32_t headerSize = in.u32();
    if (headerSize < 40)
        throw std::runtime_error("unsupported BMP header (size " + std::to_string(headerSize) + ")");
    const int32_t width = in.i32();
    const int32_t rawHeight = in.i32();
    in.skip(2); // planes
    const uint16_t bpp = in.u16();
    const uint32_t compression = in.u32();
    in.skip(20); // image size, resolution, palette counts

    if (width <= 0 || rawHeight == 0 || width > 16384 || rawHeight > 16384 || rawHeight < -16384)
        throw std::runtime_error("unsupported BMP dimensions");
    const bool topDown = rawHeight < 0;
    const int height = topDown ? -rawHeight : rawHeight;

    uint32_t masks[4] = {0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000}; // r, g, b, a
    if (compression == kBiBitfields || compression == kBiAlphaBitfields) {
        if (bpp != 32 && bpp != 16)
            throw std::runtime_error("unsupported BMP bitfields depth " + std::to_string(bpp));
        // The masks directly follow the 40-byte header, both for BITMAPINFOHEADER and as
        // part of the V2+ headers. The alpha mask exists from V3 (56 bytes) on.
        masks[0] = in.u32();
        masks[1] = in.u32();
        masks[2] = in.u32();
        masks[3] = (headerSize >= 56 || compression == kBiAlphaBitfields) ? in.u32() : 0;
    } else if (compression != kBiRgb || (bpp != 32 && bpp != 24)) {
        throw std::runtime_error("unsupported BMP format (compression " + std::to_string(compression) + ", " +
                                 std::to_string(bpp) + " bpp)");
    }

    const size_t stride = (size_t(width) * bpp + 31) / 32 * 4;
    if (pixelOffset > size || size - pixelOffset < stride * height)
        throw std::runtime_error("truncated BMP pixel data");

    const Channel r(masks[0]), g(masks[1]), b(masks[2]), a(bpp == 24 ? 0 : masks[3]);
    const int bytesPerPixel = bpp / 8;
    Image image(width, height);
    bool anyAlpha = false;
    for (int y = 0; y < height; y++) {
        const uint8_t* row = data + pixelOffset + stride * (topDown ? y : height - 1 - y);
        uint8_t* out = image.rgba.data() + size_t(y) * width * 4;
        for (int x = 0; x < width; x++, row += bytesPerPixel, out += 4) {
            uint32_t pixel = 0;
            for (int i = 0; i < bytesPerPixel; i++)
                pixel |= uint32_t(row[i]) << (8 * i);
            out[0] = r(pixel, 0);
            out[1] = g(pixel, 0);
            out[2] = b(pixel, 0);
            out[3] = a(pixel, 0xFF);
            anyAlpha |= out[3] != 0;
        }
    }

    // A 32-bit BI_RGB bitmap's fourth byte is formally unused; treat it as alpha only if
    // the file actually uses it.
    if (compression == kBiRgb && bpp == 32 && !anyAlpha)
        for (size_t i = 3; i < image.rgba.size(); i += 4)
            image.rgba[i] = 0xFF;

    return image;
}

} // namespace otu
