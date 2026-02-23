#ifndef RESIZE_H
#define RESIZE_H

#include "image.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

enum class ResizeFilter {
    Nearest,
    Bilinear
};

namespace detail {
inline int clampInt(int value, int low, int high) {
    return std::max(low, std::min(value, high));
}

inline float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

inline std::uint8_t toByte(float value) {
    float clamped = std::max(0.0f, std::min(value, 255.0f));
    return static_cast<std::uint8_t>(std::lround(clamped));
}
} // namespace detail

template <typename ImageT>
ImageT resizeImage(const ImageT& source, int newWidth, int newHeight,
                   ResizeFilter filter = ResizeFilter::Bilinear) {
    const int srcWidth = source.width();
    const int srcHeight = source.height();
    if (srcWidth <= 0 || srcHeight <= 0) {
        throw std::invalid_argument("Source image dimensions must be positive");
    }
    if (newWidth <= 0 || newHeight <= 0) {
        throw std::invalid_argument("Resize dimensions must be positive");
    }

    ImageT output(newWidth, newHeight);
    if (newWidth == srcWidth && newHeight == srcHeight) {
        for (int y = 0; y < srcHeight; ++y) {
            for (int x = 0; x < srcWidth; ++x) {
                output.setPixel(x, y, source.getPixel(x, y));
            }
        }
        return output;
    }

    const float scaleX = static_cast<float>(srcWidth) / static_cast<float>(newWidth);
    const float scaleY = static_cast<float>(srcHeight) / static_cast<float>(newHeight);

    for (int y = 0; y < newHeight; ++y) {
        const float srcY = (static_cast<float>(y) + 0.5f) * scaleY - 0.5f;
        if (filter == ResizeFilter::Nearest) {
            const int iy = detail::clampInt(static_cast<int>(std::lround(srcY)), 0, srcHeight - 1);
            for (int x = 0; x < newWidth; ++x) {
                const float srcX = (static_cast<float>(x) + 0.5f) * scaleX - 0.5f;
                const int ix = detail::clampInt(static_cast<int>(std::lround(srcX)), 0, srcWidth - 1);
                output.setPixel(x, y, source.getPixel(ix, iy));
            }
            continue;
        }

        const int y0 = detail::clampInt(static_cast<int>(std::floor(srcY)), 0, srcHeight - 1);
        const int y1 = detail::clampInt(y0 + 1, 0, srcHeight - 1);
        const float fy = srcY - static_cast<float>(y0);
        for (int x = 0; x < newWidth; ++x) {
            const float srcX = (static_cast<float>(x) + 0.5f) * scaleX - 0.5f;
            const int x0 = detail::clampInt(static_cast<int>(std::floor(srcX)), 0, srcWidth - 1);
            const int x1 = detail::clampInt(x0 + 1, 0, srcWidth - 1);
            const float fx = srcX - static_cast<float>(x0);

            const Color& c00 = source.getPixel(x0, y0);
            const Color& c10 = source.getPixel(x1, y0);
            const Color& c01 = source.getPixel(x0, y1);
            const Color& c11 = source.getPixel(x1, y1);

            const float r0 = detail::lerp(static_cast<float>(c00.r), static_cast<float>(c10.r), fx);
            const float r1 = detail::lerp(static_cast<float>(c01.r), static_cast<float>(c11.r), fx);
            const float g0 = detail::lerp(static_cast<float>(c00.g), static_cast<float>(c10.g), fx);
            const float g1 = detail::lerp(static_cast<float>(c01.g), static_cast<float>(c11.g), fx);
            const float b0 = detail::lerp(static_cast<float>(c00.b), static_cast<float>(c10.b), fx);
            const float b1 = detail::lerp(static_cast<float>(c01.b), static_cast<float>(c11.b), fx);

            Color out;
            out.r = detail::toByte(detail::lerp(r0, r1, fy));
            out.g = detail::toByte(detail::lerp(g0, g1, fy));
            out.b = detail::toByte(detail::lerp(b0, b1, fy));
            output.setPixel(x, y, out);
        }
    }

    return output;
}

#endif
