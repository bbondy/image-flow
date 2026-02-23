#include "cli.h"

#include "bmp.h"
#include "drawable.h"
#include "effects.h"
#include "gif.h"
#include "jpg.h"
#include "layer.h"
#include "png.h"
#include "resize.h"
#include "svg.h"
#include "webp.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cmath>
#include <deque>
#include <exception>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace {
std::string toLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

std::string extensionLower(const std::string& path) {
    const std::filesystem::path p(path);
    std::string ext = p.extension().string();
    if (!ext.empty() && ext[0] == '.') {
        ext.erase(0, 1);
    }
    return toLower(ext);
}

std::vector<std::string> collectArgs(int argc, char** argv) {
    std::vector<std::string> args;
    args.reserve(static_cast<std::size_t>(argc));
    for (int i = 0; i < argc; ++i) {
        args.emplace_back(argv[i]);
    }
    return args;
}

bool getFlagValue(const std::vector<std::string>& args, const std::string& flag, std::string& value) {
    for (std::size_t i = 0; i + 1 < args.size(); ++i) {
        if (args[i] == flag) {
            value = args[i + 1];
            return true;
        }
    }
    return false;
}

std::vector<std::string> getFlagValues(const std::vector<std::string>& args, const std::string& flag) {
    std::vector<std::string> values;
    for (std::size_t i = 0; i + 1 < args.size(); ++i) {
        if (args[i] == flag) {
            values.push_back(args[i + 1]);
        }
    }
    return values;
}

void writeUsage() {
    std::cout
        << "image_flow CLI\n\n"
        << "Usage:\n"
        << "  image_flow help\n"
        << "  image_flow new --width <w> --height <h> --out <project.iflow>\n"
        << "  image_flow new --from-image <file> [--fit <w>x<h>] --out <project.iflow>\n"
        << "  image_flow info --in <project.iflow>\n"
        << "  image_flow render --in <project.iflow> --out <image.{png|bmp|jpg|gif|webp|svg}>\n"
        << "  image_flow ops --in <project.iflow> --out <project.iflow> --op \"<action key=value ...>\" [--op ...]\n\n"
        << "  image_flow ops --width <w> --height <h> --out <project.iflow> [--op ...|--ops-file <path>|--stdin]\n\n"
        << "Notes:\n"
        << "  - WebP output requires cwebp/dwebp tooling in PATH.\n";
}

void copyBufferToImage(const ImageBuffer& source, Image& destination) {
    if (source.width() != destination.width() || source.height() != destination.height()) {
        throw std::invalid_argument("copyBufferToImage dimensions must match");
    }
    for (int y = 0; y < source.height(); ++y) {
        for (int x = 0; x < source.width(); ++x) {
            const PixelRGBA8& p = source.getPixel(x, y);
            destination.setPixel(x, y, Color(p.r, p.g, p.b));
        }
    }
}

class BufferImageView final : public Image {
public:
    explicit BufferImageView(ImageBuffer& buffer, std::uint8_t drawAlpha = 255, bool forceAlpha = true)
        : m_buffer(buffer), m_lastColor(0, 0, 0), m_drawAlpha(drawAlpha), m_forceAlpha(forceAlpha) {}

    int width() const override { return m_buffer.width(); }
    int height() const override { return m_buffer.height(); }
    bool inBounds(int x, int y) const override { return m_buffer.inBounds(x, y); }

    const Color& getPixel(int x, int y) const override {
        if (!m_buffer.inBounds(x, y)) {
            m_lastColor = Color(0, 0, 0);
            return m_lastColor;
        }
        const PixelRGBA8& px = m_buffer.getPixel(x, y);
        m_lastColor = Color(px.r, px.g, px.b);
        return m_lastColor;
    }

    void setPixel(int x, int y, const Color& color) override {
        if (!m_buffer.inBounds(x, y)) {
            return;
        }
        const PixelRGBA8 src = m_buffer.getPixel(x, y);
        const std::uint8_t alpha = m_forceAlpha ? m_drawAlpha : src.a;
        m_buffer.setPixel(x, y, PixelRGBA8(color.r, color.g, color.b, alpha));
    }

private:
    ImageBuffer& m_buffer;
    mutable Color m_lastColor;
    std::uint8_t m_drawAlpha;
    bool m_forceAlpha;
};

bool saveCompositeByExtension(const ImageBuffer& composite, const std::string& outPath) {
    const std::string ext = extensionLower(outPath);

    if (ext == "png") {
        PNGImage out(composite.width(), composite.height(), Color(0, 0, 0));
        copyToRasterImage(composite, out);
        return out.save(outPath);
    }
    if (ext == "bmp") {
        BMPImage out(composite.width(), composite.height(), Color(0, 0, 0));
        copyToRasterImage(composite, out);
        return out.save(outPath);
    }
    if (ext == "jpg" || ext == "jpeg") {
        JPGImage out(composite.width(), composite.height(), Color(0, 0, 0));
        copyToRasterImage(composite, out);
        return out.save(outPath);
    }
    if (ext == "gif") {
        GIFImage out(composite.width(), composite.height(), Color(0, 0, 0));
        copyToRasterImage(composite, out);
        return out.save(outPath);
    }
    if (ext == "webp") {
        if (!WEBPImage::isToolingAvailable()) {
            throw std::runtime_error("WebP tooling unavailable (install cwebp and dwebp)");
        }
        WEBPImage out(composite.width(), composite.height(), Color(0, 0, 0));
        copyToRasterImage(composite, out);
        return out.save(outPath);
    }
    if (ext == "svg") {
        SVGImage out(composite.width(), composite.height(), Color(0, 0, 0));
        copyBufferToImage(composite, out);
        return out.save(outPath);
    }

    throw std::runtime_error("Unsupported output extension: " + ext);
}

void printGroupInfo(const LayerGroup& group, const std::string& indent) {
    std::cout << indent << "Group '" << group.name() << "'"
              << " nodes=" << group.nodeCount()
              << " visible=" << (group.visible() ? "true" : "false")
              << " opacity=" << group.opacity()
              << " blendMode=" << static_cast<int>(group.blendMode())
              << " offset=(" << group.offsetX() << "," << group.offsetY() << ")\n";

    for (std::size_t i = 0; i < group.nodeCount(); ++i) {
        const LayerNode& node = group.node(i);
        if (node.isGroup()) {
            printGroupInfo(node.asGroup(), indent + "  ");
            continue;
        }
        const Layer& layer = node.asLayer();
        std::cout << indent << "  Layer '" << layer.name() << "'"
                  << " size=" << layer.image().width() << "x" << layer.image().height()
                  << " visible=" << (layer.visible() ? "true" : "false")
                  << " opacity=" << layer.opacity()
                  << " blendMode=" << static_cast<int>(layer.blendMode())
                  << " offset=(" << layer.offsetX() << "," << layer.offsetY() << ")"
                  << " mask=" << (layer.hasMask() ? "true" : "false") << "\n";
    }
}

std::vector<std::string> splitWhitespace(const std::string& text) {
    std::istringstream in(text);
    std::vector<std::string> parts;
    std::string token;
    while (in >> token) {
        parts.push_back(token);
    }
    return parts;
}

std::vector<std::string> splitByChar(const std::string& text, char delimiter) {
    std::vector<std::string> parts;
    std::string current;
    std::istringstream in(text);
    while (std::getline(in, current, delimiter)) {
        parts.push_back(current);
    }
    return parts;
}

std::vector<std::string> splitNonEmptyByChar(const std::string& text, char delimiter) {
    const std::vector<std::string> all = splitByChar(text, delimiter);
    std::vector<std::string> out;
    for (const std::string& value : all) {
        if (!value.empty()) {
            out.push_back(value);
        }
    }
    return out;
}

bool parseBoolFlag(const std::string& value) {
    const std::string lowered = toLower(value);
    if (lowered == "1" || lowered == "true" || lowered == "yes" || lowered == "on") {
        return true;
    }
    if (lowered == "0" || lowered == "false" || lowered == "no" || lowered == "off") {
        return false;
    }
    throw std::runtime_error("Invalid boolean value: " + value);
}

std::pair<int, int> parseIntPair(const std::string& text) {
    const std::vector<std::string> parts = splitByChar(text, ',');
    if (parts.size() != 2) {
        throw std::runtime_error("Expected integer pair x,y but got: " + text);
    }
    return {std::stoi(parts[0]), std::stoi(parts[1])};
}

std::pair<double, double> parseDoublePair(const std::string& text) {
    const std::vector<std::string> parts = splitByChar(text, ',');
    if (parts.size() != 2) {
        throw std::runtime_error("Expected numeric pair x,y but got: " + text);
    }
    return {std::stod(parts[0]), std::stod(parts[1])};
}

std::vector<std::pair<int, int>> parseDrawPoints(const std::string& text,
                                                 std::size_t minPoints,
                                                 const std::string& action) {
    const std::vector<std::string> tokens = splitNonEmptyByChar(text, ';');
    std::vector<std::pair<int, int>> points;
    points.reserve(tokens.size());
    for (const std::string& token : tokens) {
        points.push_back(parseIntPair(token));
    }
    if (points.size() < minPoints) {
        throw std::runtime_error(action + " requires at least " + std::to_string(minPoints) +
                                 " points in points=x0,y0;x1,y1;...");
    }
    return points;
}

PixelRGBA8 parseRGBA(const std::string& text, bool allowRgb = false) {
    const std::vector<std::string> parts = splitByChar(text, ',');
    if (parts.size() == 3 && allowRgb) {
        return PixelRGBA8(static_cast<std::uint8_t>(std::stoi(parts[0])),
                          static_cast<std::uint8_t>(std::stoi(parts[1])),
                          static_cast<std::uint8_t>(std::stoi(parts[2])),
                          255);
    }
    if (parts.size() != 4) {
        throw std::runtime_error("Expected rgba=r,g,b,a but got: " + text);
    }
    return PixelRGBA8(static_cast<std::uint8_t>(std::stoi(parts[0])),
                      static_cast<std::uint8_t>(std::stoi(parts[1])),
                      static_cast<std::uint8_t>(std::stoi(parts[2])),
                      static_cast<std::uint8_t>(std::stoi(parts[3])));
}

float clamp01(float value) {
    return std::max(0.0f, std::min(1.0f, value));
}

std::uint8_t clampByte(int value) {
    if (value < 0) {
        return 0;
    }
    if (value > 255) {
        return 255;
    }
    return static_cast<std::uint8_t>(value);
}

PixelRGBA8 lerpPixel(const PixelRGBA8& a, const PixelRGBA8& b, float t) {
    const float clamped = clamp01(t);
    const float inv = 1.0f - clamped;
    return PixelRGBA8(
        clampByte(static_cast<int>(std::lround(inv * static_cast<float>(a.r) + clamped * static_cast<float>(b.r)))),
        clampByte(static_cast<int>(std::lround(inv * static_cast<float>(a.g) + clamped * static_cast<float>(b.g)))),
        clampByte(static_cast<int>(std::lround(inv * static_cast<float>(a.b) + clamped * static_cast<float>(b.b)))),
        clampByte(static_cast<int>(std::lround(inv * static_cast<float>(a.a) + clamped * static_cast<float>(b.a)))));
}

void applyLinearGradientToLayer(Layer& layer,
                                const PixelRGBA8& fromColor,
                                const PixelRGBA8& toColor,
                                double x0,
                                double y0,
                                double x1,
                                double y1) {
    ImageBuffer& image = layer.image();
    const double dx = x1 - x0;
    const double dy = y1 - y0;
    const double denom = (dx * dx) + (dy * dy);

    if (denom <= 0.0) {
        image.fill(fromColor);
        return;
    }

    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const double proj = ((static_cast<double>(x) - x0) * dx + (static_cast<double>(y) - y0) * dy) / denom;
            const float t = clamp01(static_cast<float>(proj));
            image.setPixel(x, y, lerpPixel(fromColor, toColor, t));
        }
    }
}

void applyRadialGradientToLayer(Layer& layer,
                                const PixelRGBA8& innerColor,
                                const PixelRGBA8& outerColor,
                                double cx,
                                double cy,
                                double radius) {
    if (radius <= 0.0) {
        throw std::runtime_error("gradient-layer radial radius must be > 0");
    }

    ImageBuffer& image = layer.image();
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const double dx = static_cast<double>(x) - cx;
            const double dy = static_cast<double>(y) - cy;
            const double dist = std::sqrt((dx * dx) + (dy * dy));
            const float t = clamp01(static_cast<float>(dist / radius));
            image.setPixel(x, y, lerpPixel(innerColor, outerColor, t));
        }
    }
}

void applyCheckerToLayer(Layer& layer,
                         int cellWidth,
                         int cellHeight,
                         const PixelRGBA8& colorA,
                         const PixelRGBA8& colorB,
                         int offsetX,
                         int offsetY) {
    if (cellWidth <= 0 || cellHeight <= 0) {
        throw std::runtime_error("checker-layer requires cell_width>0 and cell_height>0");
    }

    ImageBuffer& image = layer.image();
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const int shiftedX = x + offsetX;
            const int shiftedY = y + offsetY;
            const int cellX = static_cast<int>(std::floor(static_cast<double>(shiftedX) / static_cast<double>(cellWidth)));
            const int cellY = static_cast<int>(std::floor(static_cast<double>(shiftedY) / static_cast<double>(cellHeight)));
            const bool useA = ((cellX + cellY) % 2) == 0;
            image.setPixel(x, y, useA ? colorA : colorB);
        }
    }
}

void applyNoiseToLayer(Layer& layer,
                       std::uint32_t seed,
                       float amount,
                       bool monochrome,
                       bool affectAlpha) {
    const float mix = clamp01(amount);
    if (mix <= 0.0f) {
        return;
    }

    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> jitter(-128, 128);
    ImageBuffer& image = layer.image();
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const PixelRGBA8 src = image.getPixel(x, y);
            const int baseNoise = jitter(rng);
            const int rNoise = monochrome ? baseNoise : jitter(rng);
            const int gNoise = monochrome ? baseNoise : jitter(rng);
            const int bNoise = monochrome ? baseNoise : jitter(rng);
            const int aNoise = monochrome ? baseNoise : jitter(rng);

            const int outR = static_cast<int>(std::lround(static_cast<float>(src.r) + mix * static_cast<float>(rNoise)));
            const int outG = static_cast<int>(std::lround(static_cast<float>(src.g) + mix * static_cast<float>(gNoise)));
            const int outB = static_cast<int>(std::lround(static_cast<float>(src.b) + mix * static_cast<float>(bNoise)));
            const int outA = affectAlpha
                                 ? static_cast<int>(std::lround(static_cast<float>(src.a) + mix * static_cast<float>(aNoise)))
                                 : static_cast<int>(src.a);

            image.setPixel(x, y, PixelRGBA8(clampByte(outR), clampByte(outG), clampByte(outB), clampByte(outA)));
        }
    }
}

double rgbDistance(const PixelRGBA8& a, const PixelRGBA8& b) {
    const double dr = static_cast<double>(a.r) - static_cast<double>(b.r);
    const double dg = static_cast<double>(a.g) - static_cast<double>(b.g);
    const double db = static_cast<double>(a.b) - static_cast<double>(b.b);
    return std::sqrt((dr * dr) + (dg * dg) + (db * db));
}

void applyReplaceColorToLayer(Layer& layer,
                              const PixelRGBA8& fromColor,
                              const PixelRGBA8& toColor,
                              double tolerance,
                              double softness,
                              bool preserveLuma) {
    const double clampedTolerance = std::max(0.0, tolerance);
    const double clampedSoftness = std::max(0.0, softness);
    const double hard = clampedTolerance;
    const double softEnd = clampedTolerance + clampedSoftness;

    ImageBuffer& image = layer.image();
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const PixelRGBA8 src = image.getPixel(x, y);
            const double dist = rgbDistance(src, fromColor);

            float mix = 0.0f;
            if (dist <= hard) {
                mix = 1.0f;
            } else if (softEnd > hard && dist < softEnd) {
                mix = static_cast<float>(1.0 - ((dist - hard) / (softEnd - hard)));
            }

            if (mix <= 0.0f) {
                continue;
            }

            PixelRGBA8 adjusted = toColor;
            adjusted.a = src.a;
            if (preserveLuma) {
                const float srcLuma = 0.299f * static_cast<float>(src.r) +
                                      0.587f * static_cast<float>(src.g) +
                                      0.114f * static_cast<float>(src.b);
                const float dstLuma = 0.299f * static_cast<float>(adjusted.r) +
                                      0.587f * static_cast<float>(adjusted.g) +
                                      0.114f * static_cast<float>(adjusted.b);
                if (dstLuma > 0.0f) {
                    const float scale = srcLuma / dstLuma;
                    adjusted.r = clampByte(static_cast<int>(std::lround(scale * static_cast<float>(adjusted.r))));
                    adjusted.g = clampByte(static_cast<int>(std::lround(scale * static_cast<float>(adjusted.g))));
                    adjusted.b = clampByte(static_cast<int>(std::lround(scale * static_cast<float>(adjusted.b))));
                }
            }

            image.setPixel(x, y, lerpPixel(src, adjusted, mix));
        }
    }
}

void applyChannelMixToLayer(Layer& layer,
                            const std::array<float, 9>& mixMatrix,
                            float clampMin,
                            float clampMax) {
    const float minV = std::min(clampMin, clampMax);
    const float maxV = std::max(clampMin, clampMax);

    ImageBuffer& image = layer.image();
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const PixelRGBA8 src = image.getPixel(x, y);
            const float r = static_cast<float>(src.r);
            const float g = static_cast<float>(src.g);
            const float b = static_cast<float>(src.b);

            float outR = mixMatrix[0] * r + mixMatrix[1] * g + mixMatrix[2] * b;
            float outG = mixMatrix[3] * r + mixMatrix[4] * g + mixMatrix[5] * b;
            float outB = mixMatrix[6] * r + mixMatrix[7] * g + mixMatrix[8] * b;

            outR = std::max(minV, std::min(maxV, outR));
            outG = std::max(minV, std::min(maxV, outG));
            outB = std::max(minV, std::min(maxV, outB));

            image.setPixel(x, y, PixelRGBA8(clampByte(static_cast<int>(std::lround(outR))),
                                            clampByte(static_cast<int>(std::lround(outG))),
                                            clampByte(static_cast<int>(std::lround(outB))),
                                            src.a));
        }
    }
}

void applyInvertToLayer(Layer& layer, bool preserveAlpha) {
    ImageBuffer& image = layer.image();
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const PixelRGBA8 src = image.getPixel(x, y);
            image.setPixel(x, y, PixelRGBA8(static_cast<std::uint8_t>(255 - src.r),
                                            static_cast<std::uint8_t>(255 - src.g),
                                            static_cast<std::uint8_t>(255 - src.b),
                                            preserveAlpha ? src.a : static_cast<std::uint8_t>(255 - src.a)));
        }
    }
}

void applyThresholdToLayer(Layer& layer, int threshold, const PixelRGBA8& lo, const PixelRGBA8& hi) {
    const int t = std::max(0, std::min(255, threshold));
    ImageBuffer& image = layer.image();
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const PixelRGBA8 src = image.getPixel(x, y);
            const int luma = static_cast<int>(std::lround(0.299 * static_cast<double>(src.r) +
                                                          0.587 * static_cast<double>(src.g) +
                                                          0.114 * static_cast<double>(src.b)));
            image.setPixel(x, y, luma >= t ? hi : lo);
        }
    }
}

float luma01(const PixelRGBA8& p) {
    return (0.299f * static_cast<float>(p.r) +
            0.587f * static_cast<float>(p.g) +
            0.114f * static_cast<float>(p.b)) / 255.0f;
}

PixelRGBA8 sampleClamped(const ImageBuffer& image, int x, int y) {
    const int sx = std::max(0, std::min(image.width() - 1, x));
    const int sy = std::max(0, std::min(image.height() - 1, y));
    return image.getPixel(sx, sy);
}

void applyGaussianBlurToBuffer(ImageBuffer& image, int radius, double sigma) {
    if (radius <= 0) {
        return;
    }
    const double effectiveSigma = sigma > 0.0 ? sigma : (0.3 * static_cast<double>(radius) + 0.8);

    std::vector<float> kernel(static_cast<std::size_t>(radius * 2 + 1), 0.0f);
    double sum = 0.0;
    for (int i = -radius; i <= radius; ++i) {
        const double x = static_cast<double>(i);
        const double w = std::exp(-(x * x) / (2.0 * effectiveSigma * effectiveSigma));
        kernel[static_cast<std::size_t>(i + radius)] = static_cast<float>(w);
        sum += w;
    }
    for (float& w : kernel) {
        w = static_cast<float>(w / sum);
    }

    ImageBuffer tmp(image.width(), image.height(), PixelRGBA8(0, 0, 0, 0));
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            double ar = 0.0;
            double ag = 0.0;
            double ab = 0.0;
            double aa = 0.0;
            for (int k = -radius; k <= radius; ++k) {
                const PixelRGBA8 s = sampleClamped(image, x + k, y);
                const float w = kernel[static_cast<std::size_t>(k + radius)];
                ar += w * static_cast<double>(s.r);
                ag += w * static_cast<double>(s.g);
                ab += w * static_cast<double>(s.b);
                aa += w * static_cast<double>(s.a);
            }
            tmp.setPixel(x, y, PixelRGBA8(clampByte(static_cast<int>(std::lround(ar))),
                                          clampByte(static_cast<int>(std::lround(ag))),
                                          clampByte(static_cast<int>(std::lround(ab))),
                                          clampByte(static_cast<int>(std::lround(aa)))));
        }
    }

    ImageBuffer out(image.width(), image.height(), PixelRGBA8(0, 0, 0, 0));
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            double ar = 0.0;
            double ag = 0.0;
            double ab = 0.0;
            double aa = 0.0;
            for (int k = -radius; k <= radius; ++k) {
                const PixelRGBA8 s = sampleClamped(tmp, x, y + k);
                const float w = kernel[static_cast<std::size_t>(k + radius)];
                ar += w * static_cast<double>(s.r);
                ag += w * static_cast<double>(s.g);
                ab += w * static_cast<double>(s.b);
                aa += w * static_cast<double>(s.a);
            }
            out.setPixel(x, y, PixelRGBA8(clampByte(static_cast<int>(std::lround(ar))),
                                          clampByte(static_cast<int>(std::lround(ag))),
                                          clampByte(static_cast<int>(std::lround(ab))),
                                          clampByte(static_cast<int>(std::lround(aa)))));
        }
    }
    image = out;
}

void applySobelToBuffer(ImageBuffer& image, bool keepAlpha) {
    static const int kx[3][3] = {
        {-1, 0, 1},
        {-2, 0, 2},
        {-1, 0, 1}};
    static const int ky[3][3] = {
        {-1, -2, -1},
        {0, 0, 0},
        {1, 2, 1}};

    ImageBuffer out(image.width(), image.height(), PixelRGBA8(0, 0, 0, 255));
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            double gx = 0.0;
            double gy = 0.0;
            for (int j = -1; j <= 1; ++j) {
                for (int i = -1; i <= 1; ++i) {
                    const float l = luma01(sampleClamped(image, x + i, y + j));
                    gx += static_cast<double>(kx[j + 1][i + 1]) * l;
                    gy += static_cast<double>(ky[j + 1][i + 1]) * l;
                }
            }
            const double mag = std::sqrt(gx * gx + gy * gy);
            const int m = std::max(0, std::min(255, static_cast<int>(std::lround(255.0 * std::min(1.0, mag / 4.0)))));
            const std::uint8_t alpha = keepAlpha ? image.getPixel(x, y).a : 255;
            out.setPixel(x, y, PixelRGBA8(static_cast<std::uint8_t>(m),
                                          static_cast<std::uint8_t>(m),
                                          static_cast<std::uint8_t>(m),
                                          alpha));
        }
    }
    image = out;
}

void applyCannyToBuffer(ImageBuffer& image, int lowThreshold, int highThreshold, bool keepAlpha) {
    const int w = image.width();
    const int h = image.height();
    if (w <= 0 || h <= 0) {
        return;
    }

    std::vector<float> gx(static_cast<std::size_t>(w) * static_cast<std::size_t>(h), 0.0f);
    std::vector<float> gy(static_cast<std::size_t>(w) * static_cast<std::size_t>(h), 0.0f);
    std::vector<float> mag(static_cast<std::size_t>(w) * static_cast<std::size_t>(h), 0.0f);
    std::vector<float> dir(static_cast<std::size_t>(w) * static_cast<std::size_t>(h), 0.0f);
    auto idx = [w](int x, int y) { return static_cast<std::size_t>(y) * static_cast<std::size_t>(w) + static_cast<std::size_t>(x); };

    static const int kx[3][3] = {
        {-1, 0, 1},
        {-2, 0, 2},
        {-1, 0, 1}};
    static const int ky[3][3] = {
        {-1, -2, -1},
        {0, 0, 0},
        {1, 2, 1}};

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float sx = 0.0f;
            float sy = 0.0f;
            for (int j = -1; j <= 1; ++j) {
                for (int i = -1; i <= 1; ++i) {
                    const float l = luma01(sampleClamped(image, x + i, y + j));
                    sx += static_cast<float>(kx[j + 1][i + 1]) * l;
                    sy += static_cast<float>(ky[j + 1][i + 1]) * l;
                }
            }
            gx[idx(x, y)] = sx;
            gy[idx(x, y)] = sy;
            mag[idx(x, y)] = std::sqrt(sx * sx + sy * sy);
            dir[idx(x, y)] = std::atan2(sy, sx);
        }
    }

    std::vector<float> nms(static_cast<std::size_t>(w) * static_cast<std::size_t>(h), 0.0f);
    for (int y = 1; y + 1 < h; ++y) {
        for (int x = 1; x + 1 < w; ++x) {
            const float angle = dir[idx(x, y)] * 180.0f / 3.14159265358979323846f;
            float norm = angle;
            if (norm < 0.0f) {
                norm += 180.0f;
            }

            float q = 0.0f;
            float r = 0.0f;
            if ((norm >= 0.0f && norm < 22.5f) || (norm >= 157.5f && norm <= 180.0f)) {
                q = mag[idx(x + 1, y)];
                r = mag[idx(x - 1, y)];
            } else if (norm >= 22.5f && norm < 67.5f) {
                q = mag[idx(x + 1, y - 1)];
                r = mag[idx(x - 1, y + 1)];
            } else if (norm >= 67.5f && norm < 112.5f) {
                q = mag[idx(x, y + 1)];
                r = mag[idx(x, y - 1)];
            } else {
                q = mag[idx(x - 1, y - 1)];
                r = mag[idx(x + 1, y + 1)];
            }

            const float m = mag[idx(x, y)];
            nms[idx(x, y)] = (m >= q && m >= r) ? m : 0.0f;
        }
    }

    const float low = static_cast<float>(std::max(0, std::min(255, lowThreshold))) / 255.0f;
    const float high = static_cast<float>(std::max(0, std::min(255, highThreshold))) / 255.0f;

    std::vector<std::uint8_t> edges(static_cast<std::size_t>(w) * static_cast<std::size_t>(h), 0);
    std::deque<std::pair<int, int>> q;
    for (int y = 1; y + 1 < h; ++y) {
        for (int x = 1; x + 1 < w; ++x) {
            const float m = nms[idx(x, y)];
            if (m >= high) {
                edges[idx(x, y)] = 255;
                q.emplace_back(x, y);
            } else if (m >= low) {
                edges[idx(x, y)] = 128;
            }
        }
    }

    while (!q.empty()) {
        const auto [x, y] = q.front();
        q.pop_front();
        for (int j = -1; j <= 1; ++j) {
            for (int i = -1; i <= 1; ++i) {
                if (i == 0 && j == 0) {
                    continue;
                }
                const int nx = x + i;
                const int ny = y + j;
                if (nx < 0 || nx >= w || ny < 0 || ny >= h) {
                    continue;
                }
                std::uint8_t& e = edges[idx(nx, ny)];
                if (e == 128) {
                    e = 255;
                    q.emplace_back(nx, ny);
                }
            }
        }
    }

    ImageBuffer out(w, h, PixelRGBA8(0, 0, 0, 255));
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const std::uint8_t v = edges[idx(x, y)] == 255 ? 255 : 0;
            const std::uint8_t alpha = keepAlpha ? image.getPixel(x, y).a : 255;
            out.setPixel(x, y, PixelRGBA8(v, v, v, alpha));
        }
    }
    image = out;
}

void applyMorphologyToBuffer(ImageBuffer& image, const std::string& op, int radius, int iterations) {
    if (radius <= 0 || iterations <= 0) {
        return;
    }
    const bool dilate = op == "dilate";
    const bool erode = op == "erode";
    if (!dilate && !erode) {
        throw std::runtime_error("morphology op must be erode or dilate");
    }

    for (int iter = 0; iter < iterations; ++iter) {
        ImageBuffer out(image.width(), image.height(), PixelRGBA8(0, 0, 0, 0));
        for (int y = 0; y < image.height(); ++y) {
            for (int x = 0; x < image.width(); ++x) {
                int bestR = dilate ? 0 : 255;
                int bestG = dilate ? 0 : 255;
                int bestB = dilate ? 0 : 255;
                int bestA = dilate ? 0 : 255;
                for (int j = -radius; j <= radius; ++j) {
                    for (int i = -radius; i <= radius; ++i) {
                        if ((i * i + j * j) > (radius * radius)) {
                            continue;
                        }
                        const PixelRGBA8 s = sampleClamped(image, x + i, y + j);
                        if (dilate) {
                            bestR = std::max(bestR, static_cast<int>(s.r));
                            bestG = std::max(bestG, static_cast<int>(s.g));
                            bestB = std::max(bestB, static_cast<int>(s.b));
                            bestA = std::max(bestA, static_cast<int>(s.a));
                        } else {
                            bestR = std::min(bestR, static_cast<int>(s.r));
                            bestG = std::min(bestG, static_cast<int>(s.g));
                            bestB = std::min(bestB, static_cast<int>(s.b));
                            bestA = std::min(bestA, static_cast<int>(s.a));
                        }
                    }
                }
                out.setPixel(x, y, PixelRGBA8(static_cast<std::uint8_t>(bestR),
                                              static_cast<std::uint8_t>(bestG),
                                              static_cast<std::uint8_t>(bestB),
                                              static_cast<std::uint8_t>(bestA)));
            }
        }
        image = out;
    }
}

void applyGammaToBuffer(ImageBuffer& image, double gamma) {
    if (gamma <= 0.0) {
        throw std::runtime_error("gamma must be > 0");
    }
    const double invGamma = 1.0 / gamma;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const PixelRGBA8 src = image.getPixel(x, y);
            const auto map = [invGamma](std::uint8_t v) {
                const double n = static_cast<double>(v) / 255.0;
                return clampByte(static_cast<int>(std::lround(255.0 * std::pow(n, invGamma))));
            };
            image.setPixel(x, y, PixelRGBA8(map(src.r), map(src.g), map(src.b), src.a));
        }
    }
}

void applyLevelsToBuffer(ImageBuffer& image,
                         int inBlack,
                         int inWhite,
                         double midGamma,
                         int outBlack,
                         int outWhite) {
    const double inB = static_cast<double>(std::max(0, std::min(255, inBlack)));
    const double inW = static_cast<double>(std::max(0, std::min(255, inWhite)));
    if (inW <= inB) {
        throw std::runtime_error("levels requires in_white > in_black");
    }
    if (midGamma <= 0.0) {
        throw std::runtime_error("levels gamma must be > 0");
    }
    const double outB = static_cast<double>(std::max(0, std::min(255, outBlack)));
    const double outW = static_cast<double>(std::max(0, std::min(255, outWhite)));

    auto mapLevel = [&](std::uint8_t v) -> std::uint8_t {
        double t = (static_cast<double>(v) - inB) / (inW - inB);
        t = std::max(0.0, std::min(1.0, t));
        t = std::pow(t, 1.0 / midGamma);
        const double out = outB + (outW - outB) * t;
        return clampByte(static_cast<int>(std::lround(out)));
    };

    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const PixelRGBA8 src = image.getPixel(x, y);
            image.setPixel(x, y, PixelRGBA8(mapLevel(src.r), mapLevel(src.g), mapLevel(src.b), src.a));
        }
    }
}

std::vector<std::pair<int, int>> parseCurvePoints(const std::string& text) {
    std::vector<std::pair<int, int>> points;
    const std::vector<std::string> tokens = splitNonEmptyByChar(text, ';');
    for (const std::string& t : tokens) {
        const std::pair<int, int> p = parseIntPair(t);
        points.push_back({std::max(0, std::min(255, p.first)), std::max(0, std::min(255, p.second))});
    }
    if (points.size() < 2) {
        throw std::runtime_error("curve requires at least 2 points");
    }
    std::sort(points.begin(), points.end(), [](const auto& a, const auto& b) {
        return a.first < b.first;
    });
    return points;
}

std::array<std::uint8_t, 256> buildCurveLut(const std::vector<std::pair<int, int>>& points) {
    std::array<std::uint8_t, 256> lut{};
    std::size_t seg = 0;
    for (int x = 0; x <= 255; ++x) {
        while (seg + 1 < points.size() && x > points[seg + 1].first) {
            ++seg;
        }
        if (seg + 1 >= points.size()) {
            lut[static_cast<std::size_t>(x)] = static_cast<std::uint8_t>(points.back().second);
            continue;
        }
        const int x0 = points[seg].first;
        const int y0 = points[seg].second;
        const int x1 = points[seg + 1].first;
        const int y1 = points[seg + 1].second;
        if (x1 == x0) {
            lut[static_cast<std::size_t>(x)] = static_cast<std::uint8_t>(y1);
            continue;
        }
        const double t = static_cast<double>(x - x0) / static_cast<double>(x1 - x0);
        const int y = static_cast<int>(std::lround(static_cast<double>(y0) + (static_cast<double>(y1 - y0) * t)));
        lut[static_cast<std::size_t>(x)] = clampByte(y);
    }
    return lut;
}

void applyCurvesToBuffer(ImageBuffer& image,
                         const std::array<std::uint8_t, 256>& rgbLut,
                         const std::array<std::uint8_t, 256>* rLut,
                         const std::array<std::uint8_t, 256>* gLut,
                         const std::array<std::uint8_t, 256>* bLut) {
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const PixelRGBA8 src = image.getPixel(x, y);
            std::uint8_t r = rgbLut[src.r];
            std::uint8_t g = rgbLut[src.g];
            std::uint8_t b = rgbLut[src.b];
            if (rLut) {
                r = (*rLut)[r];
            }
            if (gLut) {
                g = (*gLut)[g];
            }
            if (bLut) {
                b = (*bLut)[b];
            }
            image.setPixel(x, y, PixelRGBA8(r, g, b, src.a));
        }
    }
}

float hashUnitNoise(int x, int y, std::uint32_t seed) {
    std::uint32_t n = static_cast<std::uint32_t>(x) * 374761393u;
    n ^= static_cast<std::uint32_t>(y) * 668265263u;
    n ^= seed * 2246822519u;
    n = (n ^ (n >> 13)) * 1274126177u;
    n ^= (n >> 16);
    return static_cast<float>(n & 0x00ffffffu) / static_cast<float>(0x01000000u);
}

float smoothstep01(float t) {
    const float c = clamp01(t);
    return c * c * (3.0f - 2.0f * c);
}

float valueNoise(float x, float y, std::uint32_t seed) {
    const int x0 = static_cast<int>(std::floor(x));
    const int y0 = static_cast<int>(std::floor(y));
    const int x1 = x0 + 1;
    const int y1 = y0 + 1;
    const float tx = smoothstep01(x - static_cast<float>(x0));
    const float ty = smoothstep01(y - static_cast<float>(y0));

    const float v00 = hashUnitNoise(x0, y0, seed);
    const float v10 = hashUnitNoise(x1, y0, seed);
    const float v01 = hashUnitNoise(x0, y1, seed);
    const float v11 = hashUnitNoise(x1, y1, seed);

    const float a = v00 + (v10 - v00) * tx;
    const float b = v01 + (v11 - v01) * tx;
    return a + (b - a) * ty;
}

float fractalNoise(float x, float y, int octaves, float lacunarity, float gain, std::uint32_t seed) {
    float amplitude = 1.0f;
    float frequency = 1.0f;
    float sum = 0.0f;
    float norm = 0.0f;
    for (int o = 0; o < octaves; ++o) {
        const std::uint32_t octaveSeed = seed + static_cast<std::uint32_t>(o * 1013);
        sum += amplitude * valueNoise(x * frequency, y * frequency, octaveSeed);
        norm += amplitude;
        amplitude *= gain;
        frequency *= lacunarity;
    }
    if (norm <= 0.0f) {
        return 0.0f;
    }
    return sum / norm;
}

void applyFractalNoiseToBuffer(ImageBuffer& image,
                               float scale,
                               int octaves,
                               float lacunarity,
                               float gain,
                               float amount,
                               std::uint32_t seed,
                               bool monochrome) {
    const float s = scale <= 0.0f ? 64.0f : scale;
    const int oct = std::max(1, octaves);
    const float lac = std::max(1.01f, lacunarity);
    const float g = std::max(0.01f, std::min(1.0f, gain));
    const float mix = clamp01(amount);

    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const PixelRGBA8 src = image.getPixel(x, y);
            const float nx = static_cast<float>(x) / s;
            const float ny = static_cast<float>(y) / s;
            const float n = fractalNoise(nx, ny, oct, lac, g, seed);
            const float c = (n * 2.0f) - 1.0f;
            int dr = static_cast<int>(std::lround(c * 255.0f * mix));
            int dg = dr;
            int db = dr;
            if (!monochrome) {
                const float n2 = fractalNoise(nx + 37.2f, ny + 11.7f, oct, lac, g, seed + 97u);
                const float n3 = fractalNoise(nx + 73.9f, ny + 19.3f, oct, lac, g, seed + 211u);
                dg = static_cast<int>(std::lround(((n2 * 2.0f) - 1.0f) * 255.0f * mix));
                db = static_cast<int>(std::lround(((n3 * 2.0f) - 1.0f) * 255.0f * mix));
            }
            image.setPixel(x, y, PixelRGBA8(clampByte(static_cast<int>(src.r) + dr),
                                            clampByte(static_cast<int>(src.g) + dg),
                                            clampByte(static_cast<int>(src.b) + db),
                                            src.a));
        }
    }
}

bool hatchHit(int x, int y, int spacing, int width, int mode) {
    const int m = std::max(1, spacing);
    const int w = std::max(1, width);
    if (mode == 0) { // /
        return ((x + y) % m) < w;
    }
    if (mode == 1) { // backslash diagonal
        return ((x - y + 1000000) % m) < w;
    }
    if (mode == 2) { // horizontal
        return (y % m) < w;
    }
    return (x % m) < w; // vertical
}

void applyHatchToBuffer(ImageBuffer& image,
                        int spacing,
                        int lineWidth,
                        const PixelRGBA8& ink,
                        float opacity,
                        bool preserveHighlights) {
    const float mixBase = clamp01(opacity);
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const PixelRGBA8 src = image.getPixel(x, y);
            const float darkness = 1.0f - luma01(src);
            if (darkness <= 0.05f && preserveHighlights) {
                continue;
            }

            bool hit = false;
            if (darkness > 0.18f) hit |= hatchHit(x, y, spacing, lineWidth, 0);
            if (darkness > 0.35f) hit |= hatchHit(x, y, spacing + 2, lineWidth, 1);
            if (darkness > 0.55f) hit |= hatchHit(x, y, spacing + 4, lineWidth, 2);
            if (darkness > 0.75f) hit |= hatchHit(x, y, spacing + 6, lineWidth, 3);
            if (!hit) {
                continue;
            }

            const float mix = clamp01(mixBase * darkness);
            PixelRGBA8 target = ink;
            target.a = src.a;
            image.setPixel(x, y, lerpPixel(src, target, mix));
        }
    }
}

void blendPixelOver(ImageBuffer& image, int x, int y, const PixelRGBA8& color, float alpha) {
    if (!image.inBounds(x, y) || alpha <= 0.0f) {
        return;
    }
    const float a = clamp01(alpha);
    const PixelRGBA8 dst = image.getPixel(x, y);
    image.setPixel(x, y, lerpPixel(dst, PixelRGBA8(color.r, color.g, color.b, dst.a), a));
}

void drawSoftLine(ImageBuffer& image,
                  int x0,
                  int y0,
                  int x1,
                  int y1,
                  const PixelRGBA8& ink,
                  float opacity,
                  int thickness) {
    const int dx = std::abs(x1 - x0);
    const int dy = std::abs(y1 - y0);
    const int steps = std::max(1, std::max(dx, dy));
    const float invSteps = 1.0f / static_cast<float>(steps);
    const int radius = std::max(0, thickness / 2);

    for (int i = 0; i <= steps; ++i) {
        const float t = static_cast<float>(i) * invSteps;
        const int x = static_cast<int>(std::lround(static_cast<float>(x0) + (static_cast<float>(x1 - x0) * t)));
        const int y = static_cast<int>(std::lround(static_cast<float>(y0) + (static_cast<float>(y1 - y0) * t)));

        for (int oy = -radius; oy <= radius; ++oy) {
            for (int ox = -radius; ox <= radius; ++ox) {
                const float d2 = static_cast<float>(ox * ox + oy * oy);
                const float falloff = radius == 0 ? 1.0f : std::max(0.0f, 1.0f - (d2 / static_cast<float>((radius + 1) * (radius + 1))));
                blendPixelOver(image, x + ox, y + oy, ink, opacity * falloff);
            }
        }
    }
}

void applyPencilStrokesToBuffer(ImageBuffer& image,
                                int spacing,
                                int length,
                                int thickness,
                                double angleDegrees,
                                double angleJitterDegrees,
                                int positionJitter,
                                const PixelRGBA8& ink,
                                float opacity,
                                float minDarkness,
                                std::uint32_t seed) {
    const int step = std::max(1, spacing);
    const int strokeLength = std::max(1, length);
    const int jitter = std::max(0, positionJitter);
    const float minDark = clamp01(minDarkness);

    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> unit(0.0f, 1.0f);
    std::uniform_real_distribution<float> angleJitter(-static_cast<float>(angleJitterDegrees),
                                                      static_cast<float>(angleJitterDegrees));
    std::uniform_int_distribution<int> posJitter(-jitter, jitter);

    const double baseRad = angleDegrees * 3.14159265358979323846 / 180.0;
    for (int y = 0; y < image.height(); y += step) {
        for (int x = 0; x < image.width(); x += step) {
            const int sx = x + posJitter(rng);
            const int sy = y + posJitter(rng);
            if (!image.inBounds(sx, sy)) {
                continue;
            }

            const float darkness = 1.0f - luma01(image.getPixel(sx, sy));
            if (darkness < minDark) {
                continue;
            }

            const float spawnChance = clamp01((darkness - minDark) / std::max(0.0001f, 1.0f - minDark));
            if (unit(rng) > spawnChance) {
                continue;
            }

            const double theta = baseRad + (static_cast<double>(angleJitter(rng)) * 3.14159265358979323846 / 180.0);
            const double half = static_cast<double>(strokeLength) * 0.5;
            const int x0 = static_cast<int>(std::lround(static_cast<double>(sx) - std::cos(theta) * half));
            const int y0 = static_cast<int>(std::lround(static_cast<double>(sy) - std::sin(theta) * half));
            const int x1 = static_cast<int>(std::lround(static_cast<double>(sx) + std::cos(theta) * half));
            const int y1 = static_cast<int>(std::lround(static_cast<double>(sy) + std::sin(theta) * half));
            const float strokeOpacity = clamp01(opacity * (0.45f + darkness * 0.9f));
            drawSoftLine(image, x0, y0, x1, y1, ink, strokeOpacity, thickness);
        }
    }
}

std::vector<std::size_t> parsePathIndices(const std::string& path) {
    if (path.empty() || path[0] != '/') {
        throw std::runtime_error("Path must start with '/': " + path);
    }
    if (path == "/") {
        return {};
    }

    std::vector<std::size_t> indices;
    std::size_t start = 1;
    while (start < path.size()) {
        std::size_t end = path.find('/', start);
        const std::string piece = path.substr(start, end == std::string::npos ? std::string::npos : end - start);
        if (piece.empty()) {
            throw std::runtime_error("Invalid empty segment in path: " + path);
        }
        const int value = std::stoi(piece);
        if (value < 0) {
            throw std::runtime_error("Negative path segment in path: " + path);
        }
        indices.push_back(static_cast<std::size_t>(value));
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }
    return indices;
}

LayerGroup& resolveGroupPath(Document& document, const std::string& path) {
    LayerGroup* group = &document.rootGroup();
    const std::vector<std::size_t> indices = parsePathIndices(path);
    for (std::size_t index : indices) {
        LayerNode& node = group->node(index);
        if (!node.isGroup()) {
            throw std::runtime_error("Path does not resolve to group: " + path);
        }
        group = &node.asGroup();
    }
    return *group;
}

LayerNode& resolveNodePath(Document& document, const std::string& path) {
    const std::vector<std::size_t> indices = parsePathIndices(path);
    if (indices.empty()) {
        throw std::runtime_error("Path '/' resolves to root group, not a node");
    }

    LayerGroup* group = &document.rootGroup();
    for (std::size_t i = 0; i + 1 < indices.size(); ++i) {
        LayerNode& node = group->node(indices[i]);
        if (!node.isGroup()) {
            throw std::runtime_error("Intermediate path segment must be a group: " + path);
        }
        group = &node.asGroup();
    }
    return group->node(indices.back());
}

Layer& resolveLayerPath(Document& document, const std::string& path) {
    LayerNode& node = resolveNodePath(document, path);
    if (!node.isLayer()) {
        throw std::runtime_error("Path does not resolve to layer: " + path);
    }
    return node.asLayer();
}

BlendMode parseBlendMode(const std::string& value) {
    const std::string lowered = toLower(value);
    if (lowered == "normal") return BlendMode::Normal;
    if (lowered == "multiply") return BlendMode::Multiply;
    if (lowered == "screen") return BlendMode::Screen;
    if (lowered == "overlay") return BlendMode::Overlay;
    if (lowered == "darken") return BlendMode::Darken;
    if (lowered == "lighten") return BlendMode::Lighten;
    if (lowered == "add") return BlendMode::Add;
    if (lowered == "subtract") return BlendMode::Subtract;
    if (lowered == "difference") return BlendMode::Difference;
    if (lowered == "color-dodge" || lowered == "colordodge") return BlendMode::ColorDodge;
    throw std::runtime_error("Unsupported blend mode: " + value);
}

std::unordered_map<std::string, std::string> parseKeyValues(const std::vector<std::string>& tokens, std::size_t startIndex) {
    std::unordered_map<std::string, std::string> kv;
    for (std::size_t i = startIndex; i < tokens.size(); ++i) {
        const std::size_t split = tokens[i].find('=');
        if (split == std::string::npos || split == 0 || split + 1 >= tokens[i].size()) {
            throw std::runtime_error("Expected key=value token but got: " + tokens[i]);
        }
        kv[tokens[i].substr(0, split)] = tokens[i].substr(split + 1);
    }
    return kv;
}

ResizeFilter parseResizeFilter(const std::string& value) {
    const std::string lowered = toLower(value);
    if (lowered == "nearest") return ResizeFilter::Nearest;
    if (lowered == "bilinear") return ResizeFilter::Bilinear;
    if (lowered == "box" || lowered == "boxaverage" || lowered == "box_average") return ResizeFilter::BoxAverage;
    throw std::runtime_error("Unsupported resize filter: " + value);
}

void addOpsFromStream(std::istream& in, std::vector<std::string>& outOps) {
    std::string line;
    while (std::getline(in, line)) {
        std::size_t start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) {
            continue;
        }
        if (line[start] == '#') {
            continue;
        }
        const std::size_t end = line.find_last_not_of(" \t\r\n");
        outOps.push_back(line.substr(start, end - start + 1));
    }
}

std::vector<std::string> gatherOps(const std::vector<std::string>& args) {
    std::vector<std::string> ops = getFlagValues(args, "--op");

    std::string opsFilePath;
    if (getFlagValue(args, "--ops-file", opsFilePath)) {
        std::ifstream file(opsFilePath);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open ops file: " + opsFilePath);
        }
        addOpsFromStream(file, ops);
    }

    const bool useStdin = std::find(args.begin(), args.end(), "--stdin") != args.end();
    if (useStdin) {
        addOpsFromStream(std::cin, ops);
    }

    return ops;
}

RasterImage* loadImageByExtension(const std::string& imagePath, BMPImage& bmp, PNGImage& png, JPGImage& jpg, GIFImage& gif, WEBPImage& webp) {
    const std::string ext = extensionLower(imagePath);
    if (ext == "bmp") {
        bmp = BMPImage::load(imagePath);
        return &bmp;
    }
    if (ext == "png") {
        png = PNGImage::load(imagePath);
        return &png;
    }
    if (ext == "jpg" || ext == "jpeg") {
        jpg = JPGImage::load(imagePath);
        return &jpg;
    }
    if (ext == "gif") {
        gif = GIFImage::load(imagePath);
        return &gif;
    }
    if (ext == "webp") {
        if (!WEBPImage::isToolingAvailable()) {
            throw std::runtime_error("WebP tooling unavailable (install cwebp and dwebp)");
        }
        webp = WEBPImage::load(imagePath);
        return &webp;
    }
    throw std::runtime_error("Unsupported image format for --from-image: " + imagePath);
}

void importImageIntoLayer(Layer& layer, const std::string& imagePath, std::uint8_t alpha, const std::unordered_map<std::string, std::string>& kv) {
    const std::string ext = extensionLower(imagePath);
    if (ext == "png") {
        layer.setImageFromRaster(PNGImage::load(imagePath), alpha);
        return;
    }
    if (ext == "bmp") {
        layer.setImageFromRaster(BMPImage::load(imagePath), alpha);
        return;
    }
    if (ext == "jpg" || ext == "jpeg") {
        layer.setImageFromRaster(JPGImage::load(imagePath), alpha);
        return;
    }
    if (ext == "gif") {
        layer.setImageFromRaster(GIFImage::load(imagePath), alpha);
        return;
    }
    if (ext == "webp") {
        if (!WEBPImage::isToolingAvailable()) {
            throw std::runtime_error("WebP tooling unavailable (install cwebp and dwebp)");
        }
        layer.setImageFromRaster(WEBPImage::load(imagePath), alpha);
        return;
    }
    if (ext == "svg") {
        int rasterWidth = layer.image().width();
        int rasterHeight = layer.image().height();
        const auto widthIt = kv.find("width");
        const auto heightIt = kv.find("height");
        if (widthIt != kv.end()) {
            rasterWidth = std::stoi(widthIt->second);
        }
        if (heightIt != kv.end()) {
            rasterHeight = std::stoi(heightIt->second);
        }
        SVGImage svg = SVGImage::load(imagePath, rasterWidth, rasterHeight);
        ImageBuffer buffer(svg.width(), svg.height(), PixelRGBA8(0, 0, 0, alpha));
        for (int y = 0; y < svg.height(); ++y) {
            for (int x = 0; x < svg.width(); ++x) {
                const Color& c = svg.getPixel(x, y);
                buffer.setPixel(x, y, PixelRGBA8(c.r, c.g, c.b, alpha));
            }
        }
        layer.image() = buffer;
        if (layer.hasMask()) {
            layer.clearMask();
        }
        return;
    }
    throw std::runtime_error("Unsupported import extension: " + ext);
}

void resizeLayer(Layer& layer, int width, int height, ResizeFilter filter) {
    PNGImage source(layer.image().width(), layer.image().height(), Color(0, 0, 0));
    for (int y = 0; y < source.height(); ++y) {
        for (int x = 0; x < source.width(); ++x) {
            const PixelRGBA8& p = layer.image().getPixel(x, y);
            source.setPixel(x, y, Color(p.r, p.g, p.b));
        }
    }
    PNGImage resized = resizeImage(source, width, height, filter);
    layer.setImageFromRaster(resized, 255);
}

ImageBuffer& resolveDrawTargetBuffer(Layer& layer, const std::unordered_map<std::string, std::string>& kv) {
    const std::string target = kv.find("target") == kv.end() ? "image" : toLower(kv.at("target"));
    if (target == "image") {
        return layer.image();
    }
    if (target == "mask") {
        if (!layer.hasMask()) {
            const PixelRGBA8 maskFill = kv.find("mask_fill") == kv.end() ? PixelRGBA8(0, 0, 0, 255) : parseRGBA(kv.at("mask_fill"), true);
            layer.enableMask(maskFill);
        }
        return layer.mask();
    }
    throw std::runtime_error("target must be image or mask");
}

Transform2D buildTransformFromKV(const std::unordered_map<std::string, std::string>& kv) {
    Transform2D transform;
    if (kv.find("matrix") != kv.end()) {
        const std::vector<std::string> parts = splitByChar(kv.at("matrix"), ',');
        if (parts.size() != 6) {
            throw std::runtime_error("matrix= expects 6 comma-separated values");
        }
        transform = Transform2D::fromMatrix(std::stod(parts[0]), std::stod(parts[1]), std::stod(parts[2]),
                                            std::stod(parts[3]), std::stod(parts[4]), std::stod(parts[5]));
        return transform;
    }

    transform.setIdentity();
    const std::pair<double, double> pivot = kv.find("pivot") == kv.end()
                                                ? std::pair<double, double>(0.0, 0.0)
                                                : parseDoublePair(kv.at("pivot"));

    if (kv.find("translate") != kv.end()) {
        const std::pair<double, double> t = parseDoublePair(kv.at("translate"));
        transform.translate(t.first, t.second);
    }

    if (kv.find("scale") != kv.end()) {
        const std::vector<std::string> parts = splitByChar(kv.at("scale"), ',');
        if (parts.size() == 1) {
            const double s = std::stod(parts[0]);
            transform.scale(s, s, pivot.first, pivot.second);
        } else if (parts.size() == 2) {
            transform.scale(std::stod(parts[0]), std::stod(parts[1]), pivot.first, pivot.second);
        } else {
            throw std::runtime_error("scale= expects s or sx,sy");
        }
    }

    if (kv.find("skew") != kv.end()) {
        const std::pair<double, double> skewDegrees = parseDoublePair(kv.at("skew"));
        const double shx = std::tan(skewDegrees.first * 3.14159265358979323846 / 180.0);
        const double shy = std::tan(skewDegrees.second * 3.14159265358979323846 / 180.0);
        transform.shear(shx, shy, pivot.first, pivot.second);
    }

    if (kv.find("rotate") != kv.end()) {
        transform.rotateDegrees(std::stod(kv.at("rotate")), pivot.first, pivot.second);
    }

    return transform;
}

void applyOperation(Document& document, const std::string& opSpec) {
    const std::vector<std::string> tokens = splitWhitespace(opSpec);
    if (tokens.empty()) {
        throw std::runtime_error("Empty --op value");
    }

    const std::string action = tokens[0];
    const std::unordered_map<std::string, std::string> kv = parseKeyValues(tokens, 1);

    enum class ActionType {
        Unknown,
        AddLayer,
        AddGridLayers,
        AddGroup,
        SetLayer,
        SetGroup,
        SetTransform,
        ConcatTransform,
        ClearTransform,
        DrawFill,
        DrawLine,
        DrawRect,
        DrawFillRect,
        DrawEllipse,
        DrawFillEllipse,
        DrawPolyline,
        DrawPolygon,
        DrawFillPolygon,
        DrawCircle,
        DrawFillCircle,
        DrawArc,
        GradientLayer,
        CheckerLayer,
        NoiseLayer,
        FillLayer,
        SetPixel,
        MaskEnable,
        MaskClear,
        MaskSetPixel,
        ImportImage,
        ResizeLayer,
    };

    static const std::unordered_map<std::string, ActionType> actionTypes = {
        {"add-layer", ActionType::AddLayer},
        {"add-grid-layers", ActionType::AddGridLayers},
        {"add-group", ActionType::AddGroup},
        {"set-layer", ActionType::SetLayer},
        {"set-group", ActionType::SetGroup},
        {"set-transform", ActionType::SetTransform},
        {"concat-transform", ActionType::ConcatTransform},
        {"clear-transform", ActionType::ClearTransform},
        {"draw-fill", ActionType::DrawFill},
        {"draw-line", ActionType::DrawLine},
        {"draw-rect", ActionType::DrawRect},
        {"draw-fill-rect", ActionType::DrawFillRect},
        {"draw-ellipse", ActionType::DrawEllipse},
        {"draw-fill-ellipse", ActionType::DrawFillEllipse},
        {"draw-polyline", ActionType::DrawPolyline},
        {"draw-polygon", ActionType::DrawPolygon},
        {"draw-fill-polygon", ActionType::DrawFillPolygon},
        {"draw-circle", ActionType::DrawCircle},
        {"draw-fill-circle", ActionType::DrawFillCircle},
        {"draw-arc", ActionType::DrawArc},
        {"gradient-layer", ActionType::GradientLayer},
        {"checker-layer", ActionType::CheckerLayer},
        {"noise-layer", ActionType::NoiseLayer},
        {"fill-layer", ActionType::FillLayer},
        {"set-pixel", ActionType::SetPixel},
        {"mask-enable", ActionType::MaskEnable},
        {"mask-clear", ActionType::MaskClear},
        {"mask-set-pixel", ActionType::MaskSetPixel},
        {"import-image", ActionType::ImportImage},
        {"resize-layer", ActionType::ResizeLayer},
    };
    const auto actionTypeIt = actionTypes.find(action);
    const ActionType actionType = actionTypeIt == actionTypes.end() ? ActionType::Unknown : actionTypeIt->second;

    using OpHandler = std::function<void()>;
    const std::unordered_map<std::string, OpHandler> dispatch = {
        {"apply-effect", [&]() {
             if (kv.find("path") == kv.end() || kv.find("effect") == kv.end()) {
                 throw std::runtime_error("apply-effect requires path= and effect=");
             }
             Layer& layer = resolveLayerPath(document, kv.at("path"));
             const std::string effect = toLower(kv.at("effect"));
             if (effect == "grayscale") {
                 applyGrayscale(layer);
                 return;
             }
             if (effect == "sepia") {
                 const float strength = kv.find("strength") == kv.end() ? 1.0f : std::stof(kv.at("strength"));
                 applySepia(layer, strength);
                 return;
             }
             if (effect == "invert") {
                 const bool preserveAlpha = kv.find("preserve_alpha") == kv.end() ? true : parseBoolFlag(kv.at("preserve_alpha"));
                 applyInvertToLayer(layer, preserveAlpha);
                 return;
             }
             if (effect == "threshold") {
                 const int threshold = kv.find("threshold") == kv.end() ? 128 : std::stoi(kv.at("threshold"));
                 const PixelRGBA8 lo = kv.find("lo") == kv.end() ? PixelRGBA8(0, 0, 0, 255) : parseRGBA(kv.at("lo"), true);
                 const PixelRGBA8 hi = kv.find("hi") == kv.end() ? PixelRGBA8(255, 255, 255, 255) : parseRGBA(kv.at("hi"), true);
                 applyThresholdToLayer(layer, threshold, lo, hi);
                 return;
             }
             throw std::runtime_error("Unsupported effect: " + effect);
         }},
        {"gaussian-blur", [&]() {
             if (kv.find("path") == kv.end()) {
                 throw std::runtime_error("gaussian-blur requires path=");
             }
             Layer& layer = resolveLayerPath(document, kv.at("path"));
             ImageBuffer& target = resolveDrawTargetBuffer(layer, kv);
             const int radius = kv.find("radius") == kv.end() ? 3 : std::stoi(kv.at("radius"));
             const double sigma = kv.find("sigma") == kv.end() ? 0.0 : std::stod(kv.at("sigma"));
             applyGaussianBlurToBuffer(target, radius, sigma);
         }},
        {"edge-detect", [&]() {
             if (kv.find("path") == kv.end()) {
                 throw std::runtime_error("edge-detect requires path=");
             }
             Layer& layer = resolveLayerPath(document, kv.at("path"));
             ImageBuffer& target = resolveDrawTargetBuffer(layer, kv);
             const std::string method = kv.find("method") == kv.end() ? "sobel" : toLower(kv.at("method"));
             const bool keepAlpha = kv.find("keep_alpha") == kv.end() ? true : parseBoolFlag(kv.at("keep_alpha"));
             if (method == "sobel") {
                 applySobelToBuffer(target, keepAlpha);
                 return;
             }
             if (method == "canny") {
                 const int low = kv.find("low") == kv.end() ? 40 : std::stoi(kv.at("low"));
                 const int high = kv.find("high") == kv.end() ? 90 : std::stoi(kv.at("high"));
                 applyCannyToBuffer(target, low, high, keepAlpha);
                 return;
             }
             throw std::runtime_error("edge-detect method must be sobel or canny");
         }},
        {"morphology", [&]() {
             if (kv.find("path") == kv.end()) {
                 throw std::runtime_error("morphology requires path=");
             }
             Layer& layer = resolveLayerPath(document, kv.at("path"));
             ImageBuffer& target = resolveDrawTargetBuffer(layer, kv);
             const std::string op = kv.find("op") == kv.end() ? "dilate" : toLower(kv.at("op"));
             const int radius = kv.find("radius") == kv.end() ? 1 : std::stoi(kv.at("radius"));
             const int iterations = kv.find("iterations") == kv.end() ? 1 : std::stoi(kv.at("iterations"));
             applyMorphologyToBuffer(target, op, radius, iterations);
         }},
        {"gamma", [&]() {
             if (kv.find("path") == kv.end()) {
                 throw std::runtime_error("gamma requires path=");
             }
             Layer& layer = resolveLayerPath(document, kv.at("path"));
             ImageBuffer& target = resolveDrawTargetBuffer(layer, kv);
             const double gamma = kv.find("value") == kv.end() ? (kv.find("gamma") == kv.end() ? 1.0 : std::stod(kv.at("gamma"))) : std::stod(kv.at("value"));
             applyGammaToBuffer(target, gamma);
         }},
        {"levels", [&]() {
             if (kv.find("path") == kv.end()) {
                 throw std::runtime_error("levels requires path=");
             }
             Layer& layer = resolveLayerPath(document, kv.at("path"));
             ImageBuffer& target = resolveDrawTargetBuffer(layer, kv);
             const int inBlack = kv.find("in_black") == kv.end() ? 0 : std::stoi(kv.at("in_black"));
             const int inWhite = kv.find("in_white") == kv.end() ? 255 : std::stoi(kv.at("in_white"));
             const double midGamma = kv.find("gamma") == kv.end() ? 1.0 : std::stod(kv.at("gamma"));
             const int outBlack = kv.find("out_black") == kv.end() ? 0 : std::stoi(kv.at("out_black"));
             const int outWhite = kv.find("out_white") == kv.end() ? 255 : std::stoi(kv.at("out_white"));
             applyLevelsToBuffer(target, inBlack, inWhite, midGamma, outBlack, outWhite);
         }},
        {"curves", [&]() {
             if (kv.find("path") == kv.end()) {
                 throw std::runtime_error("curves requires path=");
             }
             Layer& layer = resolveLayerPath(document, kv.at("path"));
             ImageBuffer& target = resolveDrawTargetBuffer(layer, kv);
             const std::vector<std::pair<int, int>> rgbPoints = kv.find("rgb") == kv.end()
                                                                     ? std::vector<std::pair<int, int>>{{0, 0}, {255, 255}}
                                                                     : parseCurvePoints(kv.at("rgb"));
             const std::array<std::uint8_t, 256> rgbLut = buildCurveLut(rgbPoints);
             std::array<std::uint8_t, 256> rLut{};
             std::array<std::uint8_t, 256> gLut{};
             std::array<std::uint8_t, 256> bLut{};
             const bool hasR = kv.find("r") != kv.end();
             const bool hasG = kv.find("g") != kv.end();
             const bool hasB = kv.find("b") != kv.end();
             if (hasR) {
                 rLut = buildCurveLut(parseCurvePoints(kv.at("r")));
             }
             if (hasG) {
                 gLut = buildCurveLut(parseCurvePoints(kv.at("g")));
             }
             if (hasB) {
                 bLut = buildCurveLut(parseCurvePoints(kv.at("b")));
             }
             applyCurvesToBuffer(target, rgbLut, hasR ? &rLut : nullptr, hasG ? &gLut : nullptr, hasB ? &bLut : nullptr);
         }},
        {"fractal-noise", [&]() {
             if (kv.find("path") == kv.end()) {
                 throw std::runtime_error("fractal-noise requires path=");
             }
             Layer& layer = resolveLayerPath(document, kv.at("path"));
             ImageBuffer& target = resolveDrawTargetBuffer(layer, kv);
             const float scale = kv.find("scale") == kv.end() ? 64.0f : std::stof(kv.at("scale"));
             const int octaves = kv.find("octaves") == kv.end() ? 5 : std::stoi(kv.at("octaves"));
             const float lacunarity = kv.find("lacunarity") == kv.end() ? 2.0f : std::stof(kv.at("lacunarity"));
             const float gain = kv.find("gain") == kv.end() ? 0.5f : std::stof(kv.at("gain"));
             const float amount = kv.find("amount") == kv.end() ? 0.2f : std::stof(kv.at("amount"));
             const std::uint32_t seed = kv.find("seed") == kv.end() ? 1337u : static_cast<std::uint32_t>(std::stoul(kv.at("seed")));
             const bool monochrome = kv.find("monochrome") == kv.end() ? true : parseBoolFlag(kv.at("monochrome"));
             applyFractalNoiseToBuffer(target, scale, octaves, lacunarity, gain, amount, seed, monochrome);
         }},
        {"hatch", [&]() {
             if (kv.find("path") == kv.end()) {
                 throw std::runtime_error("hatch requires path=");
             }
             Layer& layer = resolveLayerPath(document, kv.at("path"));
             ImageBuffer& target = resolveDrawTargetBuffer(layer, kv);
             const int spacing = kv.find("spacing") == kv.end() ? 8 : std::stoi(kv.at("spacing"));
             const int lineWidth = kv.find("line_width") == kv.end() ? 1 : std::stoi(kv.at("line_width"));
             const PixelRGBA8 ink = kv.find("ink") == kv.end() ? PixelRGBA8(28, 28, 28, 255) : parseRGBA(kv.at("ink"), true);
             const float opacity = kv.find("opacity") == kv.end() ? 0.9f : std::stof(kv.at("opacity"));
             const bool preserveHighlights = kv.find("preserve_highlights") == kv.end() ? true : parseBoolFlag(kv.at("preserve_highlights"));
             applyHatchToBuffer(target, spacing, lineWidth, ink, opacity, preserveHighlights);
         }},
        {"pencil-strokes", [&]() {
             if (kv.find("path") == kv.end()) {
                 throw std::runtime_error("pencil-strokes requires path=");
             }
             Layer& layer = resolveLayerPath(document, kv.at("path"));
             ImageBuffer& target = resolveDrawTargetBuffer(layer, kv);
             const int spacing = kv.find("spacing") == kv.end() ? 8 : std::stoi(kv.at("spacing"));
             const int length = kv.find("length") == kv.end() ? 14 : std::stoi(kv.at("length"));
             const int thickness = kv.find("thickness") == kv.end() ? 1 : std::stoi(kv.at("thickness"));
             const double angle = kv.find("angle") == kv.end() ? 28.0 : std::stod(kv.at("angle"));
             const double angleJitter = kv.find("angle_jitter") == kv.end() ? 26.0 : std::stod(kv.at("angle_jitter"));
             const int jitter = kv.find("jitter") == kv.end() ? 2 : std::stoi(kv.at("jitter"));
             const PixelRGBA8 ink = kv.find("ink") == kv.end() ? PixelRGBA8(26, 26, 26, 255) : parseRGBA(kv.at("ink"), true);
             const float opacity = kv.find("opacity") == kv.end() ? 0.22f : std::stof(kv.at("opacity"));
             const float minDarkness = kv.find("min_darkness") == kv.end() ? 0.15f : std::stof(kv.at("min_darkness"));
             const std::uint32_t seed = kv.find("seed") == kv.end() ? 1337u : static_cast<std::uint32_t>(std::stoul(kv.at("seed")));
             applyPencilStrokesToBuffer(target, spacing, length, thickness, angle, angleJitter, jitter, ink, opacity, minDarkness, seed);
         }},
        {"replace-color", [&]() {
             if (kv.find("path") == kv.end() || kv.find("from") == kv.end() || kv.find("to") == kv.end()) {
                 throw std::runtime_error("replace-color requires path= from= to=");
             }
             Layer& layer = resolveLayerPath(document, kv.at("path"));
             const PixelRGBA8 fromColor = parseRGBA(kv.at("from"), true);
             const PixelRGBA8 toColor = parseRGBA(kv.at("to"), true);
             const double tolerance = kv.find("tolerance") == kv.end() ? 36.0 : std::stod(kv.at("tolerance"));
             const double softness = kv.find("softness") == kv.end() ? 24.0 : std::stod(kv.at("softness"));
             const bool preserveLuma = kv.find("preserve_luma") == kv.end() ? true : parseBoolFlag(kv.at("preserve_luma"));
             applyReplaceColorToLayer(layer, fromColor, toColor, tolerance, softness, preserveLuma);
         }},
        {"channel-mix", [&]() {
             if (kv.find("path") == kv.end()) {
                 throw std::runtime_error("channel-mix requires path=");
             }
             Layer& layer = resolveLayerPath(document, kv.at("path"));
             const std::array<float, 9> matrix = {
                 kv.find("rr") == kv.end() ? 1.0f : std::stof(kv.at("rr")),
                 kv.find("rg") == kv.end() ? 0.0f : std::stof(kv.at("rg")),
                 kv.find("rb") == kv.end() ? 0.0f : std::stof(kv.at("rb")),
                 kv.find("gr") == kv.end() ? 0.0f : std::stof(kv.at("gr")),
                 kv.find("gg") == kv.end() ? 1.0f : std::stof(kv.at("gg")),
                 kv.find("gb") == kv.end() ? 0.0f : std::stof(kv.at("gb")),
                 kv.find("br") == kv.end() ? 0.0f : std::stof(kv.at("br")),
                 kv.find("bg") == kv.end() ? 0.0f : std::stof(kv.at("bg")),
                 kv.find("bb") == kv.end() ? 1.0f : std::stof(kv.at("bb"))};
             const float clampMin = kv.find("min") == kv.end() ? 0.0f : std::stof(kv.at("min"));
             const float clampMax = kv.find("max") == kv.end() ? 255.0f : std::stof(kv.at("max"));
             applyChannelMixToLayer(layer, matrix, clampMin, clampMax);
         }},
    };
    const auto dispatchIt = dispatch.find(action);
    if (dispatchIt != dispatch.end()) {
        dispatchIt->second();
        return;
    }

    if (actionType == ActionType::AddLayer) {
        const auto parentIt = kv.find("parent");
        const std::string parentPath = parentIt == kv.end() ? "/" : parentIt->second;
        const std::string name = kv.find("name") == kv.end() ? "Layer" : kv.at("name");
        const int width = kv.find("width") == kv.end() ? document.width() : std::stoi(kv.at("width"));
        const int height = kv.find("height") == kv.end() ? document.height() : std::stoi(kv.at("height"));
        const PixelRGBA8 fill = kv.find("fill") == kv.end() ? PixelRGBA8(0, 0, 0, 0) : parseRGBA(kv.at("fill"));
        resolveGroupPath(document, parentPath).addLayer(Layer(name, width, height, fill));
        return;
    }

    if (actionType == ActionType::AddGridLayers) {
        const auto parentIt = kv.find("parent");
        const std::string parentPath = parentIt == kv.end() ? "/" : parentIt->second;
        LayerGroup& group = resolveGroupPath(document, parentPath);

        const int rows = kv.find("rows") == kv.end() ? 1 : std::stoi(kv.at("rows"));
        const int cols = kv.find("cols") == kv.end() ? 1 : std::stoi(kv.at("cols"));
        if (rows <= 0 || cols <= 0) {
            throw std::runtime_error("add-grid-layers requires rows>0 and cols>0");
        }

        const int border = kv.find("border") == kv.end() ? 0 : std::stoi(kv.at("border"));
        const int startX = kv.find("start_x") == kv.end() ? 0 : std::stoi(kv.at("start_x"));
        const int startY = kv.find("start_y") == kv.end() ? 0 : std::stoi(kv.at("start_y"));

        int tileWidth = kv.find("tile_width") == kv.end() ? (document.width() / cols) : std::stoi(kv.at("tile_width"));
        int tileHeight = kv.find("tile_height") == kv.end() ? (document.height() / rows) : std::stoi(kv.at("tile_height"));
        if (tileWidth <= 0 || tileHeight <= 0) {
            throw std::runtime_error("add-grid-layers tile dimensions must be positive");
        }

        const int innerWidth = tileWidth - (border * 2);
        const int innerHeight = tileHeight - (border * 2);
        if (innerWidth <= 0 || innerHeight <= 0) {
            throw std::runtime_error("add-grid-layers border is too large for tile size");
        }

        const std::string prefix = kv.find("name_prefix") == kv.end() ? "Tile" : kv.at("name_prefix");
        const float opacity = kv.find("opacity") == kv.end() ? 1.0f : std::stof(kv.at("opacity"));
        const BlendMode blend = kv.find("blend") == kv.end() ? BlendMode::Normal : parseBlendMode(kv.at("blend"));
        const PixelRGBA8 defaultFill = kv.find("fill") == kv.end() ? PixelRGBA8(0, 0, 0, 0) : parseRGBA(kv.at("fill"));

        std::vector<PixelRGBA8> fillSequence;
        if (kv.find("fills") != kv.end()) {
            const std::vector<std::string> fillTokens = splitNonEmptyByChar(kv.at("fills"), ';');
            for (const std::string& token : fillTokens) {
                fillSequence.push_back(parseRGBA(token));
            }
        }

        std::vector<BlendMode> blendSequence;
        if (kv.find("blends") != kv.end()) {
            const std::vector<std::string> blendTokens = splitNonEmptyByChar(kv.at("blends"), ';');
            for (const std::string& token : blendTokens) {
                blendSequence.push_back(parseBlendMode(token));
            }
        }

        int sequenceIndex = 0;
        for (int row = 0; row < rows; ++row) {
            for (int col = 0; col < cols; ++col) {
                const int x = startX + col * tileWidth + border;
                const int y = startY + row * tileHeight + border;
                const PixelRGBA8 fill = fillSequence.empty() ? defaultFill : fillSequence[sequenceIndex % static_cast<int>(fillSequence.size())];
                const BlendMode layerBlend = blendSequence.empty() ? blend : blendSequence[sequenceIndex % static_cast<int>(blendSequence.size())];

                Layer layer(prefix + "_" + std::to_string(row) + "_" + std::to_string(col), innerWidth, innerHeight, fill);
                layer.setOpacity(opacity);
                layer.setBlendMode(layerBlend);
                layer.setOffset(x, y);
                group.addLayer(layer);
                ++sequenceIndex;
            }
        }
        return;
    }

    if (actionType == ActionType::AddGroup) {
        const auto parentIt = kv.find("parent");
        const std::string parentPath = parentIt == kv.end() ? "/" : parentIt->second;
        const std::string name = kv.find("name") == kv.end() ? "Group" : kv.at("name");
        resolveGroupPath(document, parentPath).addGroup(LayerGroup(name));
        return;
    }

    if (actionType == ActionType::SetLayer) {
        if (kv.find("path") == kv.end()) {
            throw std::runtime_error("set-layer requires path=");
        }
        Layer& layer = resolveLayerPath(document, kv.at("path"));
        if (kv.find("name") != kv.end()) layer.setName(kv.at("name"));
        if (kv.find("visible") != kv.end()) layer.setVisible(parseBoolFlag(kv.at("visible")));
        if (kv.find("opacity") != kv.end()) layer.setOpacity(std::stof(kv.at("opacity")));
        if (kv.find("blend") != kv.end()) layer.setBlendMode(parseBlendMode(kv.at("blend")));
        if (kv.find("offset") != kv.end()) {
            const std::pair<int, int> offset = parseIntPair(kv.at("offset"));
            layer.setOffset(offset.first, offset.second);
        }
        return;
    }

    if (actionType == ActionType::SetGroup) {
        if (kv.find("path") == kv.end()) {
            throw std::runtime_error("set-group requires path=");
        }
        LayerNode& node = resolveNodePath(document, kv.at("path"));
        if (!node.isGroup()) {
            throw std::runtime_error("set-group path must resolve to a group");
        }
        LayerGroup& group = node.asGroup();
        if (kv.find("name") != kv.end()) group.setName(kv.at("name"));
        if (kv.find("visible") != kv.end()) group.setVisible(parseBoolFlag(kv.at("visible")));
        if (kv.find("opacity") != kv.end()) group.setOpacity(std::stof(kv.at("opacity")));
        if (kv.find("blend") != kv.end()) group.setBlendMode(parseBlendMode(kv.at("blend")));
        if (kv.find("offset") != kv.end()) {
            const std::pair<int, int> offset = parseIntPair(kv.at("offset"));
            group.setOffset(offset.first, offset.second);
        }
        return;
    }

    if (actionType == ActionType::SetTransform) {
        if (kv.find("path") == kv.end()) {
            throw std::runtime_error("set-transform requires path=");
        }
        LayerNode& node = resolveNodePath(document, kv.at("path"));
        const Transform2D transform = buildTransformFromKV(kv);
        if (node.isLayer()) {
            node.asLayer().transform() = transform;
        } else {
            node.asGroup().transform() = transform;
        }
        return;
    }

    if (actionType == ActionType::ConcatTransform) {
        if (kv.find("path") == kv.end()) {
            throw std::runtime_error("concat-transform requires path=");
        }
        LayerNode& node = resolveNodePath(document, kv.at("path"));
        const Transform2D transform = buildTransformFromKV(kv);
        if (node.isLayer()) {
            node.asLayer().transform() *= transform;
        } else {
            node.asGroup().transform() *= transform;
        }
        return;
    }

    if (actionType == ActionType::ClearTransform) {
        if (kv.find("path") == kv.end()) {
            throw std::runtime_error("clear-transform requires path=");
        }
        LayerNode& node = resolveNodePath(document, kv.at("path"));
        if (node.isLayer()) {
            node.asLayer().transform().setIdentity();
        } else {
            node.asGroup().transform().setIdentity();
        }
        return;
    }

    if (actionType == ActionType::DrawFill) {
        if (kv.find("path") == kv.end() || kv.find("rgba") == kv.end()) {
            throw std::runtime_error("draw-fill requires path= and rgba=");
        }
        Layer& layer = resolveLayerPath(document, kv.at("path"));
        ImageBuffer& targetBuffer = resolveDrawTargetBuffer(layer, kv);
        const PixelRGBA8 rgba = parseRGBA(kv.at("rgba"), true);
        BufferImageView view(targetBuffer, rgba.a, true);
        Drawable drawable(view);
        drawable.fill(Color(rgba.r, rgba.g, rgba.b));
        return;
    }

    if (actionType == ActionType::DrawLine) {
        if (kv.find("path") == kv.end() || kv.find("x0") == kv.end() || kv.find("y0") == kv.end() ||
            kv.find("x1") == kv.end() || kv.find("y1") == kv.end() || kv.find("rgba") == kv.end()) {
            throw std::runtime_error("draw-line requires path= x0= y0= x1= y1= rgba=");
        }
        Layer& layer = resolveLayerPath(document, kv.at("path"));
        ImageBuffer& targetBuffer = resolveDrawTargetBuffer(layer, kv);
        const PixelRGBA8 rgba = parseRGBA(kv.at("rgba"), true);
        BufferImageView view(targetBuffer, rgba.a, true);
        Drawable drawable(view);
        drawable.line(std::stoi(kv.at("x0")), std::stoi(kv.at("y0")),
                      std::stoi(kv.at("x1")), std::stoi(kv.at("y1")),
                      Color(rgba.r, rgba.g, rgba.b));
        return;
    }

    if (actionType == ActionType::DrawRect) {
        if (kv.find("path") == kv.end() || kv.find("x") == kv.end() || kv.find("y") == kv.end() ||
            kv.find("width") == kv.end() || kv.find("height") == kv.end() || kv.find("rgba") == kv.end()) {
            throw std::runtime_error("draw-rect requires path= x= y= width= height= rgba=");
        }
        Layer& layer = resolveLayerPath(document, kv.at("path"));
        ImageBuffer& targetBuffer = resolveDrawTargetBuffer(layer, kv);
        const PixelRGBA8 rgba = parseRGBA(kv.at("rgba"), true);
        BufferImageView view(targetBuffer, rgba.a, true);
        Drawable drawable(view);
        drawable.rect(std::stoi(kv.at("x")), std::stoi(kv.at("y")),
                      std::stoi(kv.at("width")), std::stoi(kv.at("height")),
                      Color(rgba.r, rgba.g, rgba.b));
        return;
    }

    if (actionType == ActionType::DrawFillRect) {
        if (kv.find("path") == kv.end() || kv.find("x") == kv.end() || kv.find("y") == kv.end() ||
            kv.find("width") == kv.end() || kv.find("height") == kv.end() || kv.find("rgba") == kv.end()) {
            throw std::runtime_error("draw-fill-rect requires path= x= y= width= height= rgba=");
        }
        Layer& layer = resolveLayerPath(document, kv.at("path"));
        ImageBuffer& targetBuffer = resolveDrawTargetBuffer(layer, kv);
        const PixelRGBA8 rgba = parseRGBA(kv.at("rgba"), true);
        BufferImageView view(targetBuffer, rgba.a, true);
        Drawable drawable(view);
        drawable.fillRect(std::stoi(kv.at("x")), std::stoi(kv.at("y")),
                          std::stoi(kv.at("width")), std::stoi(kv.at("height")),
                          Color(rgba.r, rgba.g, rgba.b));
        return;
    }

    if (actionType == ActionType::DrawEllipse) {
        if (kv.find("path") == kv.end() || kv.find("cx") == kv.end() || kv.find("cy") == kv.end() ||
            kv.find("rx") == kv.end() || kv.find("ry") == kv.end() || kv.find("rgba") == kv.end()) {
            throw std::runtime_error("draw-ellipse requires path= cx= cy= rx= ry= rgba=");
        }
        Layer& layer = resolveLayerPath(document, kv.at("path"));
        ImageBuffer& targetBuffer = resolveDrawTargetBuffer(layer, kv);
        const PixelRGBA8 rgba = parseRGBA(kv.at("rgba"), true);
        BufferImageView view(targetBuffer, rgba.a, true);
        Drawable drawable(view);
        drawable.ellipse(std::stoi(kv.at("cx")), std::stoi(kv.at("cy")),
                         std::stoi(kv.at("rx")), std::stoi(kv.at("ry")),
                         Color(rgba.r, rgba.g, rgba.b));
        return;
    }

    if (actionType == ActionType::DrawFillEllipse) {
        if (kv.find("path") == kv.end() || kv.find("cx") == kv.end() || kv.find("cy") == kv.end() ||
            kv.find("rx") == kv.end() || kv.find("ry") == kv.end() || kv.find("rgba") == kv.end()) {
            throw std::runtime_error("draw-fill-ellipse requires path= cx= cy= rx= ry= rgba=");
        }
        Layer& layer = resolveLayerPath(document, kv.at("path"));
        ImageBuffer& targetBuffer = resolveDrawTargetBuffer(layer, kv);
        const PixelRGBA8 rgba = parseRGBA(kv.at("rgba"), true);
        BufferImageView view(targetBuffer, rgba.a, true);
        Drawable drawable(view);
        drawable.fillEllipse(std::stoi(kv.at("cx")), std::stoi(kv.at("cy")),
                             std::stoi(kv.at("rx")), std::stoi(kv.at("ry")),
                             Color(rgba.r, rgba.g, rgba.b));
        return;
    }

    if (actionType == ActionType::DrawPolyline) {
        if (kv.find("path") == kv.end() || kv.find("points") == kv.end() || kv.find("rgba") == kv.end()) {
            throw std::runtime_error("draw-polyline requires path= points= rgba=");
        }
        Layer& layer = resolveLayerPath(document, kv.at("path"));
        ImageBuffer& targetBuffer = resolveDrawTargetBuffer(layer, kv);
        const std::vector<std::pair<int, int>> points = parseDrawPoints(kv.at("points"), 2, "draw-polyline");
        const PixelRGBA8 rgba = parseRGBA(kv.at("rgba"), true);
        BufferImageView view(targetBuffer, rgba.a, true);
        Drawable drawable(view);
        drawable.polyline(points, Color(rgba.r, rgba.g, rgba.b));
        return;
    }

    if (actionType == ActionType::DrawPolygon) {
        if (kv.find("path") == kv.end() || kv.find("points") == kv.end() || kv.find("rgba") == kv.end()) {
            throw std::runtime_error("draw-polygon requires path= points= rgba=");
        }
        Layer& layer = resolveLayerPath(document, kv.at("path"));
        ImageBuffer& targetBuffer = resolveDrawTargetBuffer(layer, kv);
        const std::vector<std::pair<int, int>> points = parseDrawPoints(kv.at("points"), 3, "draw-polygon");
        const PixelRGBA8 rgba = parseRGBA(kv.at("rgba"), true);
        BufferImageView view(targetBuffer, rgba.a, true);
        Drawable drawable(view);
        drawable.polygon(points, Color(rgba.r, rgba.g, rgba.b));
        return;
    }

    if (actionType == ActionType::DrawFillPolygon) {
        if (kv.find("path") == kv.end() || kv.find("points") == kv.end() || kv.find("rgba") == kv.end()) {
            throw std::runtime_error("draw-fill-polygon requires path= points= rgba=");
        }
        Layer& layer = resolveLayerPath(document, kv.at("path"));
        ImageBuffer& targetBuffer = resolveDrawTargetBuffer(layer, kv);
        const std::vector<std::pair<int, int>> points = parseDrawPoints(kv.at("points"), 3, "draw-fill-polygon");
        const PixelRGBA8 rgba = parseRGBA(kv.at("rgba"), true);
        BufferImageView view(targetBuffer, rgba.a, true);
        Drawable drawable(view);
        drawable.fillPolygon(points, Color(rgba.r, rgba.g, rgba.b));
        return;
    }

    if (actionType == ActionType::DrawCircle) {
        if (kv.find("path") == kv.end() || kv.find("cx") == kv.end() || kv.find("cy") == kv.end() ||
            kv.find("radius") == kv.end() || kv.find("rgba") == kv.end()) {
            throw std::runtime_error("draw-circle requires path= cx= cy= radius= rgba=");
        }
        Layer& layer = resolveLayerPath(document, kv.at("path"));
        ImageBuffer& targetBuffer = resolveDrawTargetBuffer(layer, kv);
        const PixelRGBA8 rgba = parseRGBA(kv.at("rgba"), true);
        BufferImageView view(targetBuffer, rgba.a, true);
        Drawable drawable(view);
        drawable.circle(std::stoi(kv.at("cx")), std::stoi(kv.at("cy")), std::stoi(kv.at("radius")),
                        Color(rgba.r, rgba.g, rgba.b));
        return;
    }

    if (actionType == ActionType::DrawFillCircle) {
        if (kv.find("path") == kv.end() || kv.find("cx") == kv.end() || kv.find("cy") == kv.end() ||
            kv.find("radius") == kv.end() || kv.find("rgba") == kv.end()) {
            throw std::runtime_error("draw-fill-circle requires path= cx= cy= radius= rgba=");
        }
        Layer& layer = resolveLayerPath(document, kv.at("path"));
        ImageBuffer& targetBuffer = resolveDrawTargetBuffer(layer, kv);
        const PixelRGBA8 rgba = parseRGBA(kv.at("rgba"), true);
        BufferImageView view(targetBuffer, rgba.a, true);
        Drawable drawable(view);
        drawable.fillCircle(std::stoi(kv.at("cx")), std::stoi(kv.at("cy")), std::stoi(kv.at("radius")),
                            Color(rgba.r, rgba.g, rgba.b));
        return;
    }

    if (actionType == ActionType::DrawArc) {
        if (kv.find("path") == kv.end() || kv.find("cx") == kv.end() || kv.find("cy") == kv.end() ||
            kv.find("radius") == kv.end() || kv.find("rgba") == kv.end()) {
            throw std::runtime_error("draw-arc requires path= cx= cy= radius= rgba= and start/end");
        }
        Layer& layer = resolveLayerPath(document, kv.at("path"));
        ImageBuffer& targetBuffer = resolveDrawTargetBuffer(layer, kv);
        const PixelRGBA8 rgba = parseRGBA(kv.at("rgba"), true);
        BufferImageView view(targetBuffer, rgba.a, true);
        Drawable drawable(view);

        float startRadians = 0.0f;
        float endRadians = 0.0f;
        if (kv.find("start_rad") != kv.end() && kv.find("end_rad") != kv.end()) {
            startRadians = std::stof(kv.at("start_rad"));
            endRadians = std::stof(kv.at("end_rad"));
        } else if (kv.find("start_deg") != kv.end() && kv.find("end_deg") != kv.end()) {
            startRadians = static_cast<float>(std::stod(kv.at("start_deg")) * 3.14159265358979323846 / 180.0);
            endRadians = static_cast<float>(std::stod(kv.at("end_deg")) * 3.14159265358979323846 / 180.0);
        } else {
            throw std::runtime_error("draw-arc requires start_rad/end_rad or start_deg/end_deg");
        }

        drawable.arc(std::stoi(kv.at("cx")), std::stoi(kv.at("cy")), std::stoi(kv.at("radius")),
                     startRadians, endRadians, Color(rgba.r, rgba.g, rgba.b));
        return;
    }

    if (actionType == ActionType::GradientLayer) {
        if (kv.find("path") == kv.end()) {
            throw std::runtime_error("gradient-layer requires path=");
        }
        Layer& layer = resolveLayerPath(document, kv.at("path"));
        const std::string type = kv.find("type") == kv.end() ? "linear" : toLower(kv.at("type"));
        const PixelRGBA8 fromColor = kv.find("from") == kv.end() ? PixelRGBA8(0, 0, 0, 255) : parseRGBA(kv.at("from"), true);
        const PixelRGBA8 toColor = kv.find("to") == kv.end() ? PixelRGBA8(255, 255, 255, 255) : parseRGBA(kv.at("to"), true);

        if (type == "linear") {
            const std::pair<double, double> fromPoint = kv.find("from_point") == kv.end()
                                                             ? std::pair<double, double>(0.0, 0.0)
                                                             : parseDoublePair(kv.at("from_point"));
            const std::pair<double, double> toPoint = kv.find("to_point") == kv.end()
                                                           ? std::pair<double, double>(static_cast<double>(layer.image().width() - 1),
                                                                                      static_cast<double>(layer.image().height() - 1))
                                                           : parseDoublePair(kv.at("to_point"));
            applyLinearGradientToLayer(layer, fromColor, toColor, fromPoint.first, fromPoint.second, toPoint.first, toPoint.second);
            return;
        }

        if (type == "radial") {
            const std::pair<double, double> center = kv.find("center") == kv.end()
                                                          ? std::pair<double, double>(static_cast<double>(layer.image().width()) / 2.0,
                                                                                     static_cast<double>(layer.image().height()) / 2.0)
                                                          : parseDoublePair(kv.at("center"));
            const double defaultRadius = static_cast<double>(std::min(layer.image().width(), layer.image().height())) * 0.5;
            const double radius = kv.find("radius") == kv.end() ? defaultRadius : std::stod(kv.at("radius"));
            applyRadialGradientToLayer(layer, fromColor, toColor, center.first, center.second, radius);
            return;
        }

        throw std::runtime_error("gradient-layer type must be linear or radial");
    }

    if (actionType == ActionType::CheckerLayer) {
        if (kv.find("path") == kv.end()) {
            throw std::runtime_error("checker-layer requires path=");
        }
        Layer& layer = resolveLayerPath(document, kv.at("path"));
        const int cellWidth = kv.find("cell_width") == kv.end() ? (kv.find("cell") == kv.end() ? 32 : std::stoi(kv.at("cell")))
                                                                 : std::stoi(kv.at("cell_width"));
        const int cellHeight = kv.find("cell_height") == kv.end() ? cellWidth : std::stoi(kv.at("cell_height"));
        const PixelRGBA8 colorA = kv.find("a") == kv.end() ? PixelRGBA8(0, 0, 0, 255) : parseRGBA(kv.at("a"), true);
        const PixelRGBA8 colorB = kv.find("b") == kv.end() ? PixelRGBA8(255, 255, 255, 255) : parseRGBA(kv.at("b"), true);
        const int offsetX = kv.find("offset_x") == kv.end() ? 0 : std::stoi(kv.at("offset_x"));
        const int offsetY = kv.find("offset_y") == kv.end() ? 0 : std::stoi(kv.at("offset_y"));
        applyCheckerToLayer(layer, cellWidth, cellHeight, colorA, colorB, offsetX, offsetY);
        return;
    }

    if (actionType == ActionType::NoiseLayer) {
        if (kv.find("path") == kv.end()) {
            throw std::runtime_error("noise-layer requires path=");
        }
        Layer& layer = resolveLayerPath(document, kv.at("path"));
        const std::uint32_t seed = kv.find("seed") == kv.end() ? 1337u : static_cast<std::uint32_t>(std::stoul(kv.at("seed")));
        const float amount = kv.find("amount") == kv.end() ? 0.2f : std::stof(kv.at("amount"));
        const bool monochrome = kv.find("monochrome") == kv.end() ? false : parseBoolFlag(kv.at("monochrome"));
        const bool affectAlpha = kv.find("affect_alpha") == kv.end() ? false : parseBoolFlag(kv.at("affect_alpha"));
        applyNoiseToLayer(layer, seed, amount, monochrome, affectAlpha);
        return;
    }

    if (actionType == ActionType::FillLayer) {
        if (kv.find("path") == kv.end() || kv.find("rgba") == kv.end()) {
            throw std::runtime_error("fill-layer requires path= and rgba=");
        }
        Layer& layer = resolveLayerPath(document, kv.at("path"));
        layer.image().fill(parseRGBA(kv.at("rgba")));
        return;
    }

    if (actionType == ActionType::SetPixel) {
        if (kv.find("path") == kv.end() || kv.find("x") == kv.end() || kv.find("y") == kv.end() || kv.find("rgba") == kv.end()) {
            throw std::runtime_error("set-pixel requires path= x= y= rgba=");
        }
        Layer& layer = resolveLayerPath(document, kv.at("path"));
        layer.image().setPixel(std::stoi(kv.at("x")), std::stoi(kv.at("y")), parseRGBA(kv.at("rgba")));
        return;
    }

    if (actionType == ActionType::MaskEnable) {
        if (kv.find("path") == kv.end()) {
            throw std::runtime_error("mask-enable requires path=");
        }
        Layer& layer = resolveLayerPath(document, kv.at("path"));
        const PixelRGBA8 fill = kv.find("fill") == kv.end() ? PixelRGBA8(255, 255, 255, 255) : parseRGBA(kv.at("fill"));
        layer.enableMask(fill);
        return;
    }

    if (actionType == ActionType::MaskClear) {
        if (kv.find("path") == kv.end()) {
            throw std::runtime_error("mask-clear requires path=");
        }
        resolveLayerPath(document, kv.at("path")).clearMask();
        return;
    }

    if (actionType == ActionType::MaskSetPixel) {
        if (kv.find("path") == kv.end() || kv.find("x") == kv.end() || kv.find("y") == kv.end() || kv.find("rgba") == kv.end()) {
            throw std::runtime_error("mask-set-pixel requires path= x= y= rgba=");
        }
        Layer& layer = resolveLayerPath(document, kv.at("path"));
        if (!layer.hasMask()) {
            layer.enableMask();
        }
        layer.mask().setPixel(std::stoi(kv.at("x")), std::stoi(kv.at("y")), parseRGBA(kv.at("rgba")));
        return;
    }

    if (actionType == ActionType::ImportImage) {
        if (kv.find("path") == kv.end() || kv.find("file") == kv.end()) {
            throw std::runtime_error("import-image requires path= and file=");
        }
        Layer& layer = resolveLayerPath(document, kv.at("path"));
        const std::uint8_t alpha = kv.find("alpha") == kv.end() ? 255 : static_cast<std::uint8_t>(std::stoi(kv.at("alpha")));
        importImageIntoLayer(layer, kv.at("file"), alpha, kv);
        return;
    }

    if (actionType == ActionType::ResizeLayer) {
        if (kv.find("path") == kv.end() || kv.find("width") == kv.end() || kv.find("height") == kv.end()) {
            throw std::runtime_error("resize-layer requires path= width= height=");
        }
        Layer& layer = resolveLayerPath(document, kv.at("path"));
        const ResizeFilter filter = kv.find("filter") == kv.end() ? ResizeFilter::Bilinear : parseResizeFilter(kv.at("filter"));
        resizeLayer(layer, std::stoi(kv.at("width")), std::stoi(kv.at("height")), filter);
        return;
    }

    throw std::runtime_error("Unknown op action: " + action);
}

int runIFLOWOps(const std::vector<std::string>& args) {
    std::string inPath;
    std::string outPath;
    std::string widthValue;
    std::string heightValue;
    std::string renderPath;
    const bool hasIn = getFlagValue(args, "--in", inPath);
    const bool hasOut = getFlagValue(args, "--out", outPath);
    const bool hasWidth = getFlagValue(args, "--width", widthValue);
    const bool hasHeight = getFlagValue(args, "--height", heightValue);
    const bool hasRender = getFlagValue(args, "--render", renderPath);
    const std::vector<std::string> opSpecs = gatherOps(args);

    if (!hasOut || opSpecs.empty() || (!hasIn && (!hasWidth || !hasHeight))) {
        std::cerr << "Usage: image_flow ops --in <project.iflow> --out <project.iflow> --op \"<action key=value ...>\" [--op ...]\n"
                  << "   or: image_flow ops --width <w> --height <h> --out <project.iflow> [--op ...|--ops-file <path>|--stdin]\n";
        return 1;
    }

    Document document = hasIn ? loadDocumentIFLOW(inPath) : Document(std::stoi(widthValue), std::stoi(heightValue));
    for (std::size_t i = 0; i < opSpecs.size(); ++i) {
        try {
            applyOperation(document, opSpecs[i]);
        } catch (const std::exception& ex) {
            std::ostringstream error;
            error << "Failed op[" << i << "] \"" << opSpecs[i] << "\": " << ex.what();
            throw std::runtime_error(error.str());
        }
    }

    const std::filesystem::path outFsPath(outPath);
    if (outFsPath.has_parent_path()) {
        std::filesystem::create_directories(outFsPath.parent_path());
    }
    if (!saveDocumentIFLOW(document, outPath)) {
        std::cerr << "Failed saving IFLOW document: " << outPath << "\n";
        return 1;
    }

    if (hasRender) {
        const ImageBuffer composite = document.composite();
        const std::filesystem::path renderFsPath(renderPath);
        if (renderFsPath.has_parent_path()) {
            std::filesystem::create_directories(renderFsPath.parent_path());
        }
        if (!saveCompositeByExtension(composite, renderPath)) {
            std::cerr << "Failed writing render output: " << renderPath << "\n";
            return 1;
        }
        std::cout << "Saved " << outPath << " and rendered " << renderPath << "\n";
        return 0;
    }

    std::cout << "Saved " << outPath << " after " << opSpecs.size() << " ops\n";
    return 0;
}

int runIFLOWNew(const std::vector<std::string>& args) {
    std::string widthValue;
    std::string heightValue;
    std::string outPath;
    std::string fromImagePath;
    std::string fitValue;
    const bool hasWidth = getFlagValue(args, "--width", widthValue);
    const bool hasHeight = getFlagValue(args, "--height", heightValue);
    const bool hasFromImage = getFlagValue(args, "--from-image", fromImagePath);
    const bool hasFit = getFlagValue(args, "--fit", fitValue);

    if (!getFlagValue(args, "--out", outPath) || ((hasWidth != hasHeight) || (!hasFromImage && (!hasWidth || !hasHeight)))) {
        std::cerr << "Usage: image_flow new --width <w> --height <h> --out <project.iflow>\n"
                  << "   or: image_flow new --from-image <file> [--fit <w>x<h>] --out <project.iflow>\n";
        return 1;
    }

    int width = 0;
    int height = 0;
    Layer baseLayer;
    bool addBaseLayer = false;

    if (hasFromImage) {
        BMPImage bmp;
        PNGImage png;
        JPGImage jpg;
        GIFImage gif;
        WEBPImage webp;
        RasterImage* source = loadImageByExtension(fromImagePath, bmp, png, jpg, gif, webp);
        width = source->width();
        height = source->height();
        addBaseLayer = true;
        baseLayer = Layer("Base", width, height, PixelRGBA8(0, 0, 0, 0));
        baseLayer.setImageFromRaster(*source, 255);

        if (hasFit) {
            const std::size_t split = fitValue.find('x');
            if (split == std::string::npos || split == 0 || split + 1 >= fitValue.size()) {
                throw std::runtime_error("Invalid --fit value; expected <w>x<h>");
            }
            width = std::stoi(fitValue.substr(0, split));
            height = std::stoi(fitValue.substr(split + 1));
            const ResizeFilter filter = ResizeFilter::Bilinear;
            resizeLayer(baseLayer, width, height, filter);
        }
    } else {
        width = std::stoi(widthValue);
        height = std::stoi(heightValue);
    }

    Document document(width, height);
    if (addBaseLayer) {
        document.addLayer(baseLayer);
    }
    if (!saveDocumentIFLOW(document, outPath)) {
        std::cerr << "Failed saving IFLOW document: " << outPath << "\n";
        return 1;
    }

    std::cout << "Created IFLOW project " << outPath << " (" << width << "x" << height << ")";
    if (addBaseLayer) {
        std::cout << " with imported base layer";
    }
    std::cout << "\n";
    return 0;
}

int runIFLOWInfo(const std::vector<std::string>& args) {
    std::string inPath;
    if (!getFlagValue(args, "--in", inPath)) {
        std::cerr << "Usage: image_flow info --in <project.iflow>\n";
        return 1;
    }

    Document document = loadDocumentIFLOW(inPath);
    std::cout << "Document: " << inPath << "\n";
    std::cout << "Size: " << document.width() << "x" << document.height() << "\n";
    printGroupInfo(document.rootGroup(), "");
    return 0;
}

int runIFLOWRender(const std::vector<std::string>& args) {
    std::string inPath;
    std::string outPath;
    if (!getFlagValue(args, "--in", inPath) || !getFlagValue(args, "--out", outPath)) {
        std::cerr << "Usage: image_flow render --in <project.iflow> --out <image.{png|bmp|jpg|gif|webp|svg}>\n";
        return 1;
    }

    Document document = loadDocumentIFLOW(inPath);
    ImageBuffer composite = document.composite();

    const std::filesystem::path outFsPath(outPath);
    if (outFsPath.has_parent_path()) {
        std::filesystem::create_directories(outFsPath.parent_path());
    }

    if (!saveCompositeByExtension(composite, outPath)) {
        std::cerr << "Failed writing image output: " << outPath << "\n";
        return 1;
    }

    std::cout << "Rendered " << inPath << " -> " << outPath << "\n";
    return 0;
}

int runCommand(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        std::cerr << "Usage: image_flow <new|info|render|ops|help> ...\n";
        return 1;
    }

    const std::string sub = args[1];
    if (sub == "new") {
        return runIFLOWNew(args);
    }
    if (sub == "info") {
        return runIFLOWInfo(args);
    }
    if (sub == "render") {
        return runIFLOWRender(args);
    }
    if (sub == "ops") {
        return runIFLOWOps(args);
    }

    std::cerr << "Unknown command: " << sub << "\n";
    return 1;
}
} // namespace

int runCLI(int argc, char** argv) {
    try {
        const std::vector<std::string> args = collectArgs(argc, argv);
        if (args.size() <= 1) {
            writeUsage();
            return 1;
        }

        const std::string command = args[1];
        if (command == "help" || command == "--help" || command == "-h") {
            writeUsage();
            return 0;
        }
        return runCommand(args);
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << '\n';
        return 1;
    }
}
