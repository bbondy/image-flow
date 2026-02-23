#include "cli_ops_core.h"

#include "cli_ops_draw.h"
#include "cli_ops_effects.h"
#include "cli_ops_resolve.h"
#include "cli_parse.h"
#include "cli_shared.h"

#include "bmp.h"
#include "drawable.h"
#include "gif.h"
#include "jpg.h"
#include "png.h"
#include "resize.h"
#include "svg.h"
#include "webp.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <deque>
#include <limits>
#include <random>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace {
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

} // namespace

void applyDocumentOperation(Document& document, const std::string& opSpec, const std::function<void(const std::string&)>& emitOutput) {
    const std::vector<std::string> tokens = tokenizeOpSpec(opSpec);
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
        Emit,
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
        {"emit", ActionType::Emit},
    };
    const auto actionTypeIt = actionTypes.find(action);
    const ActionType actionType = actionTypeIt == actionTypes.end() ? ActionType::Unknown : actionTypeIt->second;

    if (tryApplyEffectsOperation(action, document, kv)) {
        return;
    }
    if (tryApplyDrawOperation(action, document, kv)) {
        return;
    }

    switch (actionType) {
    case ActionType::AddLayer: {
        const auto parentIt = kv.find("parent");
        const std::string parentPath = parentIt == kv.end() ? "/" : parentIt->second;
        const std::string name = kv.find("name") == kv.end() ? "Layer" : kv.at("name");
        const int width = kv.find("width") == kv.end() ? document.width() : std::stoi(kv.at("width"));
        const int height = kv.find("height") == kv.end() ? document.height() : std::stoi(kv.at("height"));
        const PixelRGBA8 fill = kv.find("fill") == kv.end() ? PixelRGBA8(0, 0, 0, 0) : parseRGBA(kv.at("fill"));
        resolveGroupPath(document, parentPath).addLayer(Layer(name, width, height, fill));
        return;
    }

    case ActionType::AddGridLayers: {
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

    case ActionType::AddGroup: {
        const auto parentIt = kv.find("parent");
        const std::string parentPath = parentIt == kv.end() ? "/" : parentIt->second;
        const std::string name = kv.find("name") == kv.end() ? "Group" : kv.at("name");
        resolveGroupPath(document, parentPath).addGroup(LayerGroup(name));
        return;
    }

    case ActionType::SetLayer: {
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

    case ActionType::SetGroup: {
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

    case ActionType::SetTransform: {
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

    case ActionType::ConcatTransform: {
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

    case ActionType::ClearTransform: {
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

    case ActionType::GradientLayer: {
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

    case ActionType::CheckerLayer: {
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

    case ActionType::NoiseLayer: {
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

    case ActionType::FillLayer: {
        if (kv.find("path") == kv.end() || kv.find("rgba") == kv.end()) {
            throw std::runtime_error("fill-layer requires path= and rgba=");
        }
        Layer& layer = resolveLayerPath(document, kv.at("path"));
        layer.image().fill(parseRGBA(kv.at("rgba")));
        return;
    }

    case ActionType::SetPixel: {
        if (kv.find("path") == kv.end() || kv.find("x") == kv.end() || kv.find("y") == kv.end() || kv.find("rgba") == kv.end()) {
            throw std::runtime_error("set-pixel requires path= x= y= rgba=");
        }
        Layer& layer = resolveLayerPath(document, kv.at("path"));
        layer.image().setPixel(std::stoi(kv.at("x")), std::stoi(kv.at("y")), parseRGBA(kv.at("rgba")));
        return;
    }

    case ActionType::MaskEnable: {
        if (kv.find("path") == kv.end()) {
            throw std::runtime_error("mask-enable requires path=");
        }
        Layer& layer = resolveLayerPath(document, kv.at("path"));
        const PixelRGBA8 fill = kv.find("fill") == kv.end() ? PixelRGBA8(255, 255, 255, 255) : parseRGBA(kv.at("fill"));
        layer.enableMask(fill);
        return;
    }

    case ActionType::MaskClear: {
        if (kv.find("path") == kv.end()) {
            throw std::runtime_error("mask-clear requires path=");
        }
        resolveLayerPath(document, kv.at("path")).clearMask();
        return;
    }

    case ActionType::MaskSetPixel: {
        if (kv.find("path") == kv.end() || kv.find("x") == kv.end() || kv.find("y") == kv.end() || kv.find("rgba") == kv.end()) {
            throw std::runtime_error("mask-set-pixel requires path= x= y= rgba=");
        }
        Layer& layer = resolveLayerPath(document, kv.at("path"));
        if (!layer.hasMask()) {
            layer.ensureMask();
        }
        layer.maskOrThrow().setPixel(std::stoi(kv.at("x")), std::stoi(kv.at("y")), parseRGBA(kv.at("rgba")));
        return;
    }

    case ActionType::ImportImage: {
        if (kv.find("path") == kv.end() || kv.find("file") == kv.end()) {
            throw std::runtime_error("import-image requires path= and file=");
        }
        Layer& layer = resolveLayerPath(document, kv.at("path"));
        const std::uint8_t alpha = kv.find("alpha") == kv.end() ? 255 : parseByte(kv.at("alpha"), "alpha");
        importImageIntoLayer(layer, kv.at("file"), alpha, kv);
        return;
    }

    case ActionType::ResizeLayer: {
        if (kv.find("path") == kv.end() || kv.find("width") == kv.end() || kv.find("height") == kv.end()) {
            throw std::runtime_error("resize-layer requires path= width= height=");
        }
        Layer& layer = resolveLayerPath(document, kv.at("path"));
        const ResizeFilter filter = kv.find("filter") == kv.end() ? ResizeFilter::Bilinear : parseResizeFilter(kv.at("filter"));
        resizeLayer(layer,
                    parseIntInRange(kv.at("width"), "width", 1, std::numeric_limits<int>::max()),
                    parseIntInRange(kv.at("height"), "height", 1, std::numeric_limits<int>::max()),
                    filter);
        return;
    }

    case ActionType::Emit: {
        if (!emitOutput) {
            throw std::runtime_error("emit is not supported in this context");
        }
        const auto fileIt = kv.find("file");
        const auto outIt = kv.find("out");
        const std::string outputPath = fileIt != kv.end() ? fileIt->second : (outIt != kv.end() ? outIt->second : "");
        if (outputPath.empty()) {
            throw std::runtime_error("emit requires file= (or out=)");
        }
        emitOutput(outputPath);
        return;
    }

    case ActionType::Unknown:
    default:
        break;
    }

    throw std::runtime_error("Unknown op action: " + action);
}
