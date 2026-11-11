#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace otu {

// 8-bit RGBA, rows top to bottom, not premultiplied.
struct Image {
    int width = 0;
    int height = 0;
    std::vector<uint8_t> rgba;

    Image() = default;
    Image(int w, int h) : width(w), height(h), rgba(size_t(w) * h * 4, 0) {}
};

// Copies a 32x32 RGBA sprite onto `dst` at (x, y). Fully transparent source pixels are
// skipped, which matches drawing the sprite onto a transparent canvas.
void blitSprite(Image& dst, const uint8_t* sprite, int x, int y);

std::vector<uint8_t> encodePng(const Image& image);

// Uncompressed 24/32-bit BMP (BITMAPINFOHEADER or later, BI_RGB or BI_BITFIELDS).
// Colour values of fully transparent pixels are preserved.
Image decodeBmp(const uint8_t* data, size_t size);

} // namespace otu
