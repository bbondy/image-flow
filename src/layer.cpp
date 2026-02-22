#include "layer.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace {
std::size_t pixelIndex(int x, int y, int width) {
    return static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x);
}

float clamp01(float v) {
    if (v < 0.0f) {
        return 0.0f;
    }
    if (v > 1.0f) {
        return 1.0f;
    }
    return v;
}

float srgbToLinear(float c) {
    if (c <= 0.04045f) {
        return c / 12.92f;
    }
    return std::pow((c + 0.055f) / 1.055f, 2.4f);
}

float linearToSrgb(float c) {
    const float clamped = clamp01(c);
    if (clamped <= 0.0031308f) {
        return clamped * 12.92f;
    }
    return 1.055f * std::pow(clamped, 1.0f / 2.4f) - 0.055f;
}

std::uint8_t toByte(float unit) {
    const float clamped = clamp01(unit);
    return static_cast<std::uint8_t>(std::lround(clamped * 255.0f));
}

float blendChannel(BlendMode mode, float d, float s) {
    switch (mode) {
        case BlendMode::Normal:
            return s;
        case BlendMode::Multiply:
            return d * s;
        case BlendMode::Screen:
            return 1.0f - (1.0f - d) * (1.0f - s);
        case BlendMode::Overlay:
            return d < 0.5f ? (2.0f * d * s) : (1.0f - 2.0f * (1.0f - d) * (1.0f - s));
        case BlendMode::Darken:
            return std::min(d, s);
        case BlendMode::Lighten:
            return std::max(d, s);
        case BlendMode::Add:
            return std::min(1.0f, d + s);
        case BlendMode::Subtract:
            return std::max(0.0f, d - s);
        case BlendMode::Difference:
            return std::abs(d - s);
        default:
            return s;
    }
}
} // namespace

ImageBuffer::ImageBuffer() : m_width(0), m_height(0) {}

ImageBuffer::ImageBuffer(int width, int height, const PixelRGBA8& fill)
    : m_width(width), m_height(height), m_pixels(static_cast<std::size_t>(width) * static_cast<std::size_t>(height), fill) {
    if (width <= 0 || height <= 0) {
        throw std::invalid_argument("ImageBuffer dimensions must be positive");
    }
}

int ImageBuffer::width() const {
    return m_width;
}

int ImageBuffer::height() const {
    return m_height;
}

bool ImageBuffer::inBounds(int x, int y) const {
    return x >= 0 && x < m_width && y >= 0 && y < m_height;
}

const PixelRGBA8& ImageBuffer::getPixel(int x, int y) const {
    if (!inBounds(x, y)) {
        throw std::out_of_range("ImageBuffer pixel out of bounds");
    }
    return m_pixels[pixelIndex(x, y, m_width)];
}

void ImageBuffer::setPixel(int x, int y, const PixelRGBA8& pixel) {
    if (!inBounds(x, y)) {
        return;
    }
    m_pixels[pixelIndex(x, y, m_width)] = pixel;
}

void ImageBuffer::fill(const PixelRGBA8& pixel) {
    std::fill(m_pixels.begin(), m_pixels.end(), pixel);
}

Layer::Layer()
    : m_name("Layer"), m_visible(true), m_opacity(1.0f), m_blendMode(BlendMode::Normal), m_offsetX(0), m_offsetY(0) {}

Layer::Layer(const std::string& name, int width, int height, const PixelRGBA8& fill)
    : m_name(name),
      m_visible(true),
      m_opacity(1.0f),
      m_blendMode(BlendMode::Normal),
      m_offsetX(0),
      m_offsetY(0),
      m_image(width, height, fill) {}

const std::string& Layer::name() const {
    return m_name;
}

void Layer::setName(const std::string& name) {
    m_name = name;
}

bool Layer::visible() const {
    return m_visible;
}

void Layer::setVisible(bool visible) {
    m_visible = visible;
}

float Layer::opacity() const {
    return m_opacity;
}

void Layer::setOpacity(float opacity) {
    m_opacity = clamp01(opacity);
}

BlendMode Layer::blendMode() const {
    return m_blendMode;
}

void Layer::setBlendMode(BlendMode mode) {
    m_blendMode = mode;
}

int Layer::offsetX() const {
    return m_offsetX;
}

int Layer::offsetY() const {
    return m_offsetY;
}

void Layer::setOffset(int x, int y) {
    m_offsetX = x;
    m_offsetY = y;
}

ImageBuffer& Layer::image() {
    return m_image;
}

const ImageBuffer& Layer::image() const {
    return m_image;
}

Document::Document(int width, int height) : m_width(width), m_height(height) {
    if (width <= 0 || height <= 0) {
        throw std::invalid_argument("Document dimensions must be positive");
    }
}

int Document::width() const {
    return m_width;
}

int Document::height() const {
    return m_height;
}

Layer& Document::addLayer(const Layer& layer) {
    m_layers.push_back(layer);
    return m_layers.back();
}

std::size_t Document::layerCount() const {
    return m_layers.size();
}

Layer& Document::layer(std::size_t index) {
    return m_layers.at(index);
}

const Layer& Document::layer(std::size_t index) const {
    return m_layers.at(index);
}

ImageBuffer Document::composite() const {
    ImageBuffer out(m_width, m_height, PixelRGBA8(0, 0, 0, 0));

    for (const Layer& layer : m_layers) {
        if (!layer.visible() || layer.opacity() <= 0.0f) {
            continue;
        }

        for (int sy = 0; sy < layer.image().height(); ++sy) {
            const int dy = sy + layer.offsetY();
            if (dy < 0 || dy >= m_height) {
                continue;
            }

            for (int sx = 0; sx < layer.image().width(); ++sx) {
                const int dx = sx + layer.offsetX();
                if (dx < 0 || dx >= m_width) {
                    continue;
                }

                const PixelRGBA8& src = layer.image().getPixel(sx, sy);
                PixelRGBA8 dst = out.getPixel(dx, dy);

                const float sa = (static_cast<float>(src.a) / 255.0f) * layer.opacity();
                if (sa <= 0.0f) {
                    continue;
                }
                const float da = static_cast<float>(dst.a) / 255.0f;

                const float sr = srgbToLinear(static_cast<float>(src.r) / 255.0f);
                const float sg = srgbToLinear(static_cast<float>(src.g) / 255.0f);
                const float sb = srgbToLinear(static_cast<float>(src.b) / 255.0f);

                const float dr = srgbToLinear(static_cast<float>(dst.r) / 255.0f);
                const float dg = srgbToLinear(static_cast<float>(dst.g) / 255.0f);
                const float db = srgbToLinear(static_cast<float>(dst.b) / 255.0f);

                const float br = blendChannel(layer.blendMode(), dr, sr);
                const float bg = blendChannel(layer.blendMode(), dg, sg);
                const float bb = blendChannel(layer.blendMode(), db, sb);

                const float outA = sa + da * (1.0f - sa);

                float outR = 0.0f;
                float outG = 0.0f;
                float outB = 0.0f;

                if (outA > 0.0f) {
                    const float premR = dr * da * (1.0f - sa) + sr * sa * (1.0f - da) + br * sa * da;
                    const float premG = dg * da * (1.0f - sa) + sg * sa * (1.0f - da) + bg * sa * da;
                    const float premB = db * da * (1.0f - sa) + sb * sa * (1.0f - da) + bb * sa * da;
                    outR = premR / outA;
                    outG = premG / outA;
                    outB = premB / outA;
                }

                out.setPixel(dx, dy, PixelRGBA8(
                    toByte(linearToSrgb(outR)),
                    toByte(linearToSrgb(outG)),
                    toByte(linearToSrgb(outB)),
                    toByte(outA)));
            }
        }
    }

    return out;
}

ImageBuffer fromRasterImage(const RasterImage& source, std::uint8_t alpha) {
    ImageBuffer out(source.width(), source.height(), PixelRGBA8(0, 0, 0, alpha));
    for (int y = 0; y < source.height(); ++y) {
        for (int x = 0; x < source.width(); ++x) {
            const Color& c = source.getPixel(x, y);
            out.setPixel(x, y, PixelRGBA8(c.r, c.g, c.b, alpha));
        }
    }
    return out;
}

void copyToRasterImage(const ImageBuffer& source, RasterImage& destination) {
    if (source.width() != destination.width() || source.height() != destination.height()) {
        throw std::invalid_argument("copyToRasterImage dimensions must match");
    }

    for (int y = 0; y < source.height(); ++y) {
        for (int x = 0; x < source.width(); ++x) {
            const PixelRGBA8& p = source.getPixel(x, y);
            destination.setPixel(x, y, Color(p.r, p.g, p.b));
        }
    }
}
