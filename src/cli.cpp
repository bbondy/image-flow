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
#include <exception>
#include <filesystem>
#include <fstream>
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

    if (action == "add-layer") {
        const auto parentIt = kv.find("parent");
        const std::string parentPath = parentIt == kv.end() ? "/" : parentIt->second;
        const std::string name = kv.find("name") == kv.end() ? "Layer" : kv.at("name");
        const int width = kv.find("width") == kv.end() ? document.width() : std::stoi(kv.at("width"));
        const int height = kv.find("height") == kv.end() ? document.height() : std::stoi(kv.at("height"));
        const PixelRGBA8 fill = kv.find("fill") == kv.end() ? PixelRGBA8(0, 0, 0, 0) : parseRGBA(kv.at("fill"));
        resolveGroupPath(document, parentPath).addLayer(Layer(name, width, height, fill));
        return;
    }

    if (action == "add-grid-layers") {
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

    if (action == "add-group") {
        const auto parentIt = kv.find("parent");
        const std::string parentPath = parentIt == kv.end() ? "/" : parentIt->second;
        const std::string name = kv.find("name") == kv.end() ? "Group" : kv.at("name");
        resolveGroupPath(document, parentPath).addGroup(LayerGroup(name));
        return;
    }

    if (action == "set-layer") {
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

    if (action == "set-group") {
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

    if (action == "set-transform") {
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

    if (action == "concat-transform") {
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

    if (action == "clear-transform") {
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

    if (action == "apply-effect") {
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
    }

    if (action == "replace-color") {
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
        return;
    }

    if (action == "channel-mix") {
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
        return;
    }

    if (action == "draw-fill") {
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

    if (action == "draw-line") {
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

    if (action == "draw-circle") {
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

    if (action == "draw-fill-circle") {
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

    if (action == "draw-arc") {
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

    if (action == "gradient-layer") {
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

    if (action == "checker-layer") {
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

    if (action == "noise-layer") {
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

    if (action == "fill-layer") {
        if (kv.find("path") == kv.end() || kv.find("rgba") == kv.end()) {
            throw std::runtime_error("fill-layer requires path= and rgba=");
        }
        Layer& layer = resolveLayerPath(document, kv.at("path"));
        layer.image().fill(parseRGBA(kv.at("rgba")));
        return;
    }

    if (action == "set-pixel") {
        if (kv.find("path") == kv.end() || kv.find("x") == kv.end() || kv.find("y") == kv.end() || kv.find("rgba") == kv.end()) {
            throw std::runtime_error("set-pixel requires path= x= y= rgba=");
        }
        Layer& layer = resolveLayerPath(document, kv.at("path"));
        layer.image().setPixel(std::stoi(kv.at("x")), std::stoi(kv.at("y")), parseRGBA(kv.at("rgba")));
        return;
    }

    if (action == "mask-enable") {
        if (kv.find("path") == kv.end()) {
            throw std::runtime_error("mask-enable requires path=");
        }
        Layer& layer = resolveLayerPath(document, kv.at("path"));
        const PixelRGBA8 fill = kv.find("fill") == kv.end() ? PixelRGBA8(255, 255, 255, 255) : parseRGBA(kv.at("fill"));
        layer.enableMask(fill);
        return;
    }

    if (action == "mask-clear") {
        if (kv.find("path") == kv.end()) {
            throw std::runtime_error("mask-clear requires path=");
        }
        resolveLayerPath(document, kv.at("path")).clearMask();
        return;
    }

    if (action == "mask-set-pixel") {
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

    if (action == "import-image") {
        if (kv.find("path") == kv.end() || kv.find("file") == kv.end()) {
            throw std::runtime_error("import-image requires path= and file=");
        }
        Layer& layer = resolveLayerPath(document, kv.at("path"));
        const std::uint8_t alpha = kv.find("alpha") == kv.end() ? 255 : static_cast<std::uint8_t>(std::stoi(kv.at("alpha")));
        importImageIntoLayer(layer, kv.at("file"), alpha, kv);
        return;
    }

    if (action == "resize-layer") {
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
