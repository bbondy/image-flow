#include "effects.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace {
std::uint8_t clampByte(float v) {
    const float clamped = std::max(0.0f, std::min(255.0f, v));
    return static_cast<std::uint8_t>(std::lround(clamped));
}

float clamp01(float v) {
    return std::max(0.0f, std::min(1.0f, v));
}

std::uint8_t grayscaleLuma(std::uint8_t r, std::uint8_t g, std::uint8_t b) {
    return clampByte(0.299f * static_cast<float>(r) +
                     0.587f * static_cast<float>(g) +
                     0.114f * static_cast<float>(b));
}

Color sepiaColor(const Color& c, float strength) {
    const float s = clamp01(strength);
    const float r = static_cast<float>(c.r);
    const float g = static_cast<float>(c.g);
    const float b = static_cast<float>(c.b);

    const float sepiaR = 0.393f * r + 0.769f * g + 0.189f * b;
    const float sepiaG = 0.349f * r + 0.686f * g + 0.168f * b;
    const float sepiaB = 0.272f * r + 0.534f * g + 0.131f * b;

    return Color(
        clampByte((1.0f - s) * r + s * sepiaR),
        clampByte((1.0f - s) * g + s * sepiaG),
        clampByte((1.0f - s) * b + s * sepiaB));
}
} // namespace

void applyGrayscale(RasterImage& image) {
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const Color src = image.getPixel(x, y);
            const std::uint8_t gray = grayscaleLuma(src.r, src.g, src.b);
            image.setPixel(x, y, Color(gray, gray, gray));
        }
    }
}

void applyGrayscale(ImageBuffer& buffer) {
    for (int y = 0; y < buffer.height(); ++y) {
        for (int x = 0; x < buffer.width(); ++x) {
            const PixelRGBA8 src = buffer.getPixel(x, y);
            const std::uint8_t gray = grayscaleLuma(src.r, src.g, src.b);
            buffer.setPixel(x, y, PixelRGBA8(gray, gray, gray, src.a));
        }
    }
}

void applyGrayscale(Layer& layer) {
    applyGrayscale(layer.image());
}

void applySepia(RasterImage& image, float strength) {
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const Color src = image.getPixel(x, y);
            image.setPixel(x, y, sepiaColor(src, strength));
        }
    }
}

void applySepia(ImageBuffer& buffer, float strength) {
    for (int y = 0; y < buffer.height(); ++y) {
        for (int x = 0; x < buffer.width(); ++x) {
            const PixelRGBA8 src = buffer.getPixel(x, y);
            const Color sepia = sepiaColor(Color(src.r, src.g, src.b), strength);
            buffer.setPixel(x, y, PixelRGBA8(sepia.r, sepia.g, sepia.b, src.a));
        }
    }
}

void applySepia(Layer& layer, float strength) {
    applySepia(layer.image(), strength);
}
