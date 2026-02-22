#include "api.h"

#include "drawable.h"
#include "layer.h"

#include <cmath>
#include <cstdint>
#include <stdexcept>

namespace {
constexpr float kPi = 3.14159265358979323846f;

void drawSmiley(RasterImage& image) {
    Drawable d(image);

    const Color yellow(255, 220, 40);
    const Color black(0, 0, 0);

    d.fill(Color(255, 255, 255));
    d.fillCircle(128, 128, 100, yellow);
    d.circle(128, 128, 100, black);

    d.fillCircle(92, 96, 12, black);
    d.fillCircle(164, 96, 12, black);

    d.arc(128, 130, 58, 0.2f * kPi, 0.8f * kPi, black);
    d.arc(128, 131, 58, 0.2f * kPi, 0.8f * kPi, black);
    d.arc(128, 132, 58, 0.2f * kPi, 0.8f * kPi, black);
}

class LayerRasterAdapter : public RasterImage {
public:
    explicit LayerRasterAdapter(ImageBuffer& buffer) : m_buffer(buffer), m_defaultAlpha(255) {}
    LayerRasterAdapter(ImageBuffer& buffer, std::uint8_t defaultAlpha)
        : m_buffer(buffer), m_defaultAlpha(defaultAlpha) {}

    int width() const override { return m_buffer.width(); }
    int height() const override { return m_buffer.height(); }
    bool inBounds(int x, int y) const override { return m_buffer.inBounds(x, y); }

    const Color& getPixel(int x, int y) const override {
        if (!m_buffer.inBounds(x, y)) {
            throw std::out_of_range("LayerRasterAdapter pixel out of bounds");
        }
        const PixelRGBA8& p = m_buffer.getPixel(x, y);
        m_cache = Color(p.r, p.g, p.b);
        return m_cache;
    }

    void setPixel(int x, int y, const Color& color) override {
        if (!m_buffer.inBounds(x, y)) {
            return;
        }
        m_buffer.setPixel(x, y, PixelRGBA8(color.r, color.g, color.b, m_defaultAlpha));
    }

private:
    ImageBuffer& m_buffer;
    std::uint8_t m_defaultAlpha;
    mutable Color m_cache;
};
} // namespace

namespace api {
BMPImage createSmiley256BMP() {
    BMPImage image(256, 256, Color(255, 255, 255));
    drawSmiley(image);
    return image;
}

PNGImage createSmiley256PNG() {
    PNGImage image(256, 256, Color(255, 255, 255));
    drawSmiley(image);
    return image;
}

JPGImage createSmiley256JPG() {
    JPGImage image(256, 256, Color(255, 255, 255));
    drawSmiley(image);
    return image;
}

GIFImage createSmiley256GIF() {
    GIFImage image(256, 256, Color(255, 255, 255));
    drawSmiley(image);
    return image;
}

PNGImage createSmiley256LayeredPNG() {
    Document doc(256, 256);

    Layer background("Background", 256, 256, PixelRGBA8(255, 255, 255, 255));
    doc.addLayer(background);

    Layer face("Face", 256, 256, PixelRGBA8(0, 0, 0, 0));
    {
        LayerRasterAdapter canvas(face.image(), 255);
        Drawable d(canvas);
        d.fillCircle(128, 128, 100, Color(255, 220, 40));
    }
    doc.addLayer(face);

    Layer outline("Outline", 256, 256, PixelRGBA8(0, 0, 0, 0));
    {
        LayerRasterAdapter canvas(outline.image(), 255);
        Drawable d(canvas);
        d.circle(128, 128, 100, Color(0, 0, 0));
    }
    doc.addLayer(outline);

    Layer leftEye("Left Eye", 256, 256, PixelRGBA8(0, 0, 0, 0));
    {
        LayerRasterAdapter canvas(leftEye.image(), 255);
        Drawable d(canvas);
        d.fillCircle(92, 96, 12, Color(0, 0, 0));
    }
    doc.addLayer(leftEye);

    Layer rightEye("Right Eye", 256, 256, PixelRGBA8(0, 0, 0, 0));
    {
        LayerRasterAdapter canvas(rightEye.image(), 255);
        Drawable d(canvas);
        d.fillCircle(164, 96, 12, Color(0, 0, 0));
    }
    doc.addLayer(rightEye);

    Layer mouth("Mouth", 256, 256, PixelRGBA8(0, 0, 0, 0));
    {
        LayerRasterAdapter canvas(mouth.image(), 255);
        Drawable d(canvas);
        d.arc(128, 130, 58, 0.2f * kPi, 0.8f * kPi, Color(0, 0, 0));
        d.arc(128, 131, 58, 0.2f * kPi, 0.8f * kPi, Color(0, 0, 0));
        d.arc(128, 132, 58, 0.2f * kPi, 0.8f * kPi, Color(0, 0, 0));
    }
    doc.addLayer(mouth);

    ImageBuffer composited = doc.composite();
    PNGImage out(256, 256, Color(0, 0, 0));
    copyToRasterImage(composited, out);
    return out;
}

PNGImage createLayerBlendDemoPNG() {
    PNGImage base = createSmiley256PNG();

    Document doc(256, 256);

    Layer baseLayer("Base", 256, 256, PixelRGBA8(0, 0, 0, 0));
    baseLayer.image() = fromRasterImage(base, 255);
    doc.addLayer(baseLayer);

    Layer tintLayer("Blue Screen Tint", 256, 256, PixelRGBA8(0, 0, 0, 0));
    tintLayer.setBlendMode(BlendMode::Screen);
    tintLayer.setOpacity(0.65f);
    for (int y = 0; y < 256; ++y) {
        for (int x = 0; x < 256; ++x) {
            const int dx = x - 128;
            const int dy = y - 128;
            if (dx * dx + dy * dy <= 95 * 95) {
                tintLayer.image().setPixel(x, y, PixelRGBA8(40, 130, 255, 160));
            }
        }
    }
    doc.addLayer(tintLayer);

    Layer vignette("Multiply Vignette", 256, 256, PixelRGBA8(0, 0, 0, 0));
    vignette.setBlendMode(BlendMode::Multiply);
    vignette.setOpacity(0.85f);
    for (int y = 0; y < 256; ++y) {
        for (int x = 0; x < 256; ++x) {
            const float nx = (static_cast<float>(x) - 128.0f) / 128.0f;
            const float ny = (static_cast<float>(y) - 128.0f) / 128.0f;
            float radial = std::sqrt(nx * nx + ny * ny);
            if (radial > 1.0f) {
                radial = 1.0f;
            }
            const std::uint8_t alpha = static_cast<std::uint8_t>(std::lround(radial * 180.0f));
            vignette.image().setPixel(x, y, PixelRGBA8(25, 25, 30, alpha));
        }
    }
    doc.addLayer(vignette);

    ImageBuffer composited = doc.composite();
    PNGImage out(256, 256, Color(0, 0, 0));
    copyToRasterImage(composited, out);
    return out;
}
} // namespace api
